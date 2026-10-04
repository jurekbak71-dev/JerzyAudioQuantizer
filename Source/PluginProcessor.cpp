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
        juce::ParameterID{Param::enabled, 1}, "Włącz korekcję rytmu", true));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::sensitivity, 1}, "Czułość na atak kostki",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.62f));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::threshold, 1}, "Próg ignorowania szumu",
        juce::NormalisableRange<float>(-72.0f, -18.0f, 0.1f), -48.0f, "dB"));

    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{Param::grid, 1}, "Siatka rytmu",
        juce::StringArray{"AUTO", "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T"}, 0));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::strength, 1}, "Jak mocno poprawia rytm",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 90.0f, "%"));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::window, 1}, "Maksymalny błąd do naprawy",
        juce::NormalisableRange<float>(5.0f, 180.0f, 0.1f, 0.6f), 85.0f, "ms"));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::analysis, 1}, "Ile audio analizuje przed korekcją",
        juce::NormalisableRange<float>(150.0f, 1800.0f, 1.0f, 0.55f), 750.0f, "ms"));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::preserve, 1}, "Ochrona początku dźwięku",
        juce::NormalisableRange<float>(0.0f, 45.0f, 0.1f), 18.0f, "ms"));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{Param::swing, 1}, "Swing siatki",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));

    return { p.begin(), p.end() };
}

void JerzyAudioQuantizerAudioProcessor::prepareToPlay(double sr, int samplesPerBlock)
{
    sampleRateHz = sr;
    absoluteSamples = 0;
    fallbackPpq = 0.0;

    detector.prepare(sr);
    quantizer.prepare(sr);
    studioEngine.prepare(sr, samplesPerBlock, getTotalNumOutputChannels());
    updateLatency();

    detectedAttacks = acceptedAttacks = rejectedAttacks = 0;
}

bool JerzyAudioQuantizerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void JerzyAudioQuantizerAudioProcessor::updateLatency()
{
    const float analysisMs = apvts.getRawParameterValue(Param::analysis)->load();
    studioEngine.setAnalysisMs(analysisMs);
    setLatencySamples(studioEngine.getLatencySamples());
}

void JerzyAudioQuantizerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumSamples() <= 0)
        return;

    updateLatency();

    const bool enabled = apvts.getRawParameterValue(Param::enabled)->load() > 0.5f;
    const float sensitivity = apvts.getRawParameterValue(Param::sensitivity)->load();
    const float threshold = apvts.getRawParameterValue(Param::threshold)->load();
    const int gridIndex = static_cast<int>(apvts.getRawParameterValue(Param::grid)->load());
    const float strength = apvts.getRawParameterValue(Param::strength)->load() * 0.01f;
    const float windowMs = apvts.getRawParameterValue(Param::window)->load();
    const float preserveMs = apvts.getRawParameterValue(Param::preserve)->load();
    const float swing = apvts.getRawParameterValue(Param::swing)->load() * 0.01f;

    studioEngine.setTransientPreserveMs(preserveMs);
    detector.setSensitivity(sensitivity);
    detector.setThresholdDb(threshold);

    double bpm = 120.0;
    double blockPpq = fallbackPpq;

    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (auto v = pos->getBpm()) bpm = *v;
            if (auto v = pos->getPpqPosition()) blockPpq = *v;
        }
    }

    const std::int64_t blockAbsStart = absoluteSamples;

    // Save clean input in the analysis buffer before overwriting the host block.
    studioEngine.pushInput(buffer);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            mono += buffer.getSample(ch, i);
        mono /= static_cast<float>(juce::jmax(1, buffer.getNumChannels()));

        const auto scan = detector.processSample(mono);
        attackConfidence.store(scan.confidence, std::memory_order_relaxed);

        if (!scan.hit)
            continue;

        detectedAttacks.fetch_add(1);
        transientFlash.store(8);

        if (!enabled)
        {
            rejectedAttacks.fetch_add(1);
            continue;
        }

        const auto q = quantizer.quantize(
            blockPpq,
            i,
            bpm,
            static_cast<QuantizerEngine::Grid>(juce::jlimit(0, 6, gridIndex)),
            strength,
            windowMs,
            swing,
            scan.confidence);

        if (!q.valid)
        {
            rejectedAttacks.fetch_add(1);
            continue;
        }

        studioEngine.addAnchor(blockAbsStart + i, q.correctionSamples, q.timingConfidence);
        acceptedAttacks.fetch_add(1);
        correctionFlash.store(8);
        lastCorrectionMs.store(static_cast<float>(1000.0 * q.correctionSamples / sampleRateHz));
    }

    studioEngine.commitSafeAudio();

    if (enabled)
        studioEngine.pullOutput(buffer);
    // When disabled, leave dry input untouched.

    lastStretchRatio.store(studioEngine.getLastStretchRatio());
    artifactRisk.store(studioEngine.getArtifactRisk());

    if (transientFlash.load() > 0) transientFlash.fetch_sub(1);
    if (correctionFlash.load() > 0) correctionFlash.fetch_sub(1);

    absoluteSamples += buffer.getNumSamples();
    fallbackPpq = blockPpq + buffer.getNumSamples() * bpm / (60.0 * sampleRateHz);
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
