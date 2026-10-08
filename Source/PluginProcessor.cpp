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
        juce::ParameterID{Param::enabled, 3}, "W\u0142\u0105cz korekcj\u0119 rytmu", true));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::sensitivity, 3}, "Czu\u0142o\u015b\u0107 wykrywania ataku",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.62f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::threshold, 3}, "Pr\u00f3g ciszy i szumu",
        juce::NormalisableRange<float>(-72.0f, -18.0f, 0.1f), -48.0f, "dB"));

    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{Param::grid, 3}, "Siatka rytmiczna",
        juce::StringArray{
            "AUTO",
            "\u0106wier\u0107nuty 1/4",
            "\u00d3semki 1/8",
            "Szesnastki 1/16",
            "Trzydziestodw\u00f3jki 1/32",
            "Triole \u00f3semkowe 1/8T",
            "Triole szesnastkowe 1/16T",
            "Shuffle 1/8",
            "Shuffle 1/16"
        }, 0));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::strength, 3}, "Si\u0142a wyr\u00f3wnania do rytmu",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 78.0f, "%"));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::window, 3}, "Najwi\u0119kszy b\u0142\u0105d do naprawy",
        juce::NormalisableRange<float>(5.0f, 160.0f, 0.1f, 0.65f), 75.0f, "ms"));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::preserve, 3}, "Ochrona ataku kostki",
        juce::NormalisableRange<float>(8.0f, 45.0f, 0.1f), 22.0f, "ms"));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::swing, 3}, "Swing",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::dynamics, 3}, "Wyr\u00f3wnanie dynamiki",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 15.0f, "%"));

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
    wasEnabled = false;

    detector.prepare(sampleRateHz);
    quantizer.prepare(sampleRateHz);
    studioEngine.prepare(sampleRateHz, samplesPerBlock, getTotalNumOutputChannels());
    dynamics.prepare(sampleRateHz);

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
    const float preserveMs = apvts.getRawParameterValue(Param::preserve)->load();
    const float swing = apvts.getRawParameterValue(Param::swing)->load() * 0.01f;
    const float dynamicsAmount = apvts.getRawParameterValue(Param::dynamics)->load() * 0.01f;

    detector.setSensitivity(sensitivity);
    detector.setThresholdDb(threshold);
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

    bool transportJump = false;
    if (hostPlaying && wasPlaying && hostSamplePosition >= 0 && lastHostSamplePosition >= 0)
    {
        const auto expected = lastHostSamplePosition + previousBlockSize;
        transportJump = std::abs(hostSamplePosition - expected)
                      > juce::jmax<std::int64_t>(8, numSamples * 2);
    }

    const bool justEnabled = enabled && !wasEnabled;

    if ((hostPlaying && !wasPlaying) || transportJump || justEnabled)
    {
        detector.reset();
        quantizer.prepare(sampleRateHz);
        studioEngine.reset();
        dynamics.reset();
        absoluteSamples = 0;
        fallbackPpq = blockPpq;
        lastCorrectionMs.store(0.0f, std::memory_order_relaxed);
    }

    currentBpm.store(static_cast<float>(bpm), std::memory_order_relaxed);
    const std::int64_t blockAbsStart = absoluteSamples;

    // Capture the clean block first. The output head is 1.2 s behind it,
    // so every correction is known well before that audio reaches the output.
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

            if (!studioEngine.addAnchor(blockAbsStart + i,
                                        q.correctionSamples,
                                        q.timingConfidence))
            {
                rejectedAttacks.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            acceptedAttacks.fetch_add(1, std::memory_order_relaxed);
            lastCorrectionMs.store(
                static_cast<float>(1000.0 * q.correctionSamples / sampleRateHz),
                std::memory_order_relaxed);
        }

        studioEngine.pullOutput(buffer);
        dynamics.process(buffer);
    }
    else
    {
        // Dry monitoring when the internal switch is off.
        attackConfidence.store(0.0f, std::memory_order_relaxed);
        dynamicsReductionDb.store(0.0f, std::memory_order_relaxed);
    }

    lastStretchRatio.store(studioEngine.getLastStretchRatio(), std::memory_order_relaxed);
    artifactRisk.store(studioEngine.getArtifactRisk(), std::memory_order_relaxed);
    dynamicsReductionDb.store(dynamics.getGainReductionDb(), std::memory_order_relaxed);

    absoluteSamples += numSamples;
    fallbackPpq = blockPpq + numSamples * bpm / (60.0 * sampleRateHz);
    lastHostSamplePosition = hostSamplePosition;
    previousBlockSize = numSamples;
    wasPlaying = hostPlaying;
    wasEnabled = enabled;
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
