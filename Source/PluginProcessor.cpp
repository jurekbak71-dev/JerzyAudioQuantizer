#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace Param
{
    static constexpr auto enabled = "enabled";
    static constexpr auto sensitivity = "sensitivity";
    static constexpr auto threshold = "threshold";
    static constexpr auto grid = "grid";
    static constexpr auto strength = "strength";
    static constexpr auto window = "window";
    static constexpr auto analysis = "analysis";
    static constexpr auto preserve = "preserve";
    static constexpr auto swing = "swing";
    static constexpr auto dynamics = "dynamics";
}

JerzyAudioQuantizerAudioProcessor::JerzyAudioQuantizerAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
JerzyAudioQuantizerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{Param::enabled, 2}, "Włącz poprawę rytmu", true));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::sensitivity, 2}, "Jak łatwo rozpoznaje uderzenie kostki",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.64f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::threshold, 2}, "Poniżej jakiego poziomu ignoruje dźwięk",
        juce::NormalisableRange<float>(-72.0f, -18.0f, 0.1f), -48.0f, "dB"));

    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{Param::grid, 2}, "Do jakiego rytmu wyrównuje",
        juce::StringArray{
            "AUTO — rozpoznaj z nagrania",
            "Ćwierćnuty 1/4",
            "Ósemki 1/8",
            "Szesnastki 1/16",
            "Trzydziestodwójki 1/32",
            "Triole ósemkowe 1/8T",
            "Triole szesnastkowe 1/16T",
            "Shuffle ósemkowy",
            "Shuffle szesnastkowy"
        }, 0));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::strength, 2}, "Jak mocno dociąga grę do rytmu",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 88.0f, "%"));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::window, 2}, "Jak duży błąd rytmu jeszcze naprawia",
        juce::NormalisableRange<float>(5.0f, 180.0f, 0.1f, 0.6f), 85.0f, "ms"));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::analysis, 2}, "Ile nagrania sprawdza przed decyzją",
        juce::NormalisableRange<float>(250.0f, 1400.0f, 1.0f, 0.6f), 850.0f, "ms"));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::preserve, 2}, "Jak mocno chroni początek nuty",
        juce::NormalisableRange<float>(0.0f, 45.0f, 0.1f), 18.0f, "ms"));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::swing, 2}, "Ile swingu dodaje do prostej siatki",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::dynamics, 2}, "Jak mocno wyrównuje głośność uderzeń",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 25.0f, "%"));

    return { p.begin(), p.end() };
}

void JerzyAudioQuantizerAudioProcessor::prepareToPlay(double sr, int samplesPerBlock)
{
    sampleRateHz = juce::jmax(1.0, sr);
    absoluteSamples = 0;
    fallbackPpq = 0.0;
    lastHostSamplePosition = -1;
    previousBlockSize = 0;
    wasPlaying = false;

    detector.prepare(sampleRateHz);
    quantizer.prepare(sampleRateHz);
    studioEngine.prepare(sampleRateHz, samplesPerBlock, getTotalNumOutputChannels());
    dynamics.prepare(sampleRateHz);

    // Report one stable studio latency. Parameter automation no longer causes
    // host latency/PDC changes from inside processBlock().
    setLatencySamples(studioEngine.getLatencySamples());

    detectedAttacks = acceptedAttacks = rejectedAttacks = 0;
}

bool JerzyAudioQuantizerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void JerzyAudioQuantizerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const bool enabled = apvts.getRawParameterValue(Param::enabled)->load() > 0.5f;
    const float sensitivity = apvts.getRawParameterValue(Param::sensitivity)->load();
    const float threshold = apvts.getRawParameterValue(Param::threshold)->load();
    const int gridIndex = static_cast<int>(apvts.getRawParameterValue(Param::grid)->load());
    const float strength = apvts.getRawParameterValue(Param::strength)->load() * 0.01f;
    const float windowMs = apvts.getRawParameterValue(Param::window)->load();
    const float analysisMs = apvts.getRawParameterValue(Param::analysis)->load();
    const float preserveMs = apvts.getRawParameterValue(Param::preserve)->load();
    const float swing = apvts.getRawParameterValue(Param::swing)->load() * 0.01f;
    const float dynamicsAmount = apvts.getRawParameterValue(Param::dynamics)->load() * 0.01f;

    detector.setSensitivity(sensitivity);
    detector.setThresholdDb(threshold);
    studioEngine.setAnalysisMs(analysisMs);
    studioEngine.setTransientPreserveMs(preserveMs);
    dynamics.setAmount(dynamicsAmount);

    double bpm = 120.0;
    double blockPpq = fallbackPpq;
    std::int64_t hostSamplePosition = -1;
    bool hostPlaying = true;

    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (auto v = pos->getBpm())
                bpm = juce::jlimit(20.0, 400.0, *v);
            if (auto v = pos->getPpqPosition())
                blockPpq = *v;
            if (auto v = pos->getTimeInSamples())
                hostSamplePosition = *v;
            hostPlaying = pos->getIsPlaying();
        }
    }

    // A loop, seek or fresh transport start invalidates buffered audio from the
    // previous timeline position. Reset before capturing the new host block so
    // no stale tail can appear as an echo after a transport jump.
    bool transportJump = false;
    if (hostPlaying && wasPlaying && hostSamplePosition >= 0 && lastHostSamplePosition >= 0)
    {
        const auto expected = lastHostSamplePosition + previousBlockSize;
        transportJump = std::abs(hostSamplePosition - expected) > juce::jmax<std::int64_t>(8, numSamples * 2);
    }

    if ((hostPlaying && !wasPlaying) || transportJump)
    {
        detector.reset();
        quantizer.prepare(sampleRateHz);
        studioEngine.reset();
        dynamics.reset();
        absoluteSamples = 0;
        fallbackPpq = blockPpq;
    }

    currentBpm.store(static_cast<float>(bpm), std::memory_order_relaxed);
    const std::int64_t blockAbsStart = absoluteSamples;

    // The clean input is captured once. All decisions then reference this same timeline.
    studioEngine.pushInput(buffer);

    if (enabled)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float mono = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                mono += buffer.getSample(ch, i);
            mono /= static_cast<float>(numChannels);

            const auto scan = detector.processSample(mono);
            attackConfidence.store(scan.confidence, std::memory_order_relaxed);

            if (!scan.hit)
                continue;

            detectedAttacks.fetch_add(1, std::memory_order_relaxed);

            const auto q = quantizer.quantize(
                blockPpq,
                i,
                bpm,
                static_cast<QuantizerEngine::Grid>(juce::jlimit(0, 8, gridIndex)),
                strength,
                windowMs,
                swing,
                scan.confidence);

            if (!q.valid)
            {
                rejectedAttacks.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            const bool used = studioEngine.addAnchor(
                blockAbsStart + i,
                q.correctionSamples,
                q.timingConfidence);

            if (!used)
            {
                rejectedAttacks.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            acceptedAttacks.fetch_add(1, std::memory_order_relaxed);
            lastCorrectionMs.store(
                static_cast<float>(1000.0 * q.correctionSamples / sampleRateHz),
                std::memory_order_relaxed);
        }

        studioEngine.commitSafeAudio();
        studioEngine.pullOutput(buffer);
        dynamics.process(buffer);
    }
    else
    {
        // Internal off means true dry monitoring. The plugin is intended primarily
        // for rendered/studio use; host bypass can be used when latency-compensated A/B is required.
        attackConfidence.store(0.0f, std::memory_order_relaxed);
    }

    lastStretchRatio.store(studioEngine.getLastStretchRatio(), std::memory_order_relaxed);
    artifactRisk.store(studioEngine.getArtifactRisk(), std::memory_order_relaxed);
    dynamicsReductionDb.store(dynamics.getGainReductionDb(), std::memory_order_relaxed);

    absoluteSamples += numSamples;
    fallbackPpq = blockPpq + numSamples * bpm / (60.0 * sampleRateHz);
    lastHostSamplePosition = hostSamplePosition;
    previousBlockSize = numSamples;
    wasPlaying = hostPlaying;
}

void JerzyAudioQuantizerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void JerzyAudioQuantizerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* JerzyAudioQuantizerAudioProcessor::createEditor()
{
    return new JerzyAudioQuantizerAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JerzyAudioQuantizerAudioProcessor();
}
