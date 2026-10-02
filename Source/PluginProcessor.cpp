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
    static constexpr auto lookahead = "lookahead";
    static constexpr auto smooth = "smooth";
    static constexpr auto swing = "swing";
    static constexpr auto preserve = "preserve";
    static constexpr auto quality = "quality";
}

JerzyAudioQuantizerAudioProcessor::JerzyAudioQuantizerAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
JerzyAudioQuantizerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { Param::enabled, 1 }, "Quantize", true));

    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Param::sensitivity, 1 }, "Sensitivity",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.68f));

    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Param::threshold, 1 }, "Threshold",
        juce::NormalisableRange<float> (-70.0f, -12.0f, 0.1f), -42.0f, "dB"));

    p.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { Param::grid, 1 }, "Grid",
        juce::StringArray { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" }, 2));

    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Param::strength, 1 }, "Strength",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, "%"));

    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Param::window, 1 }, "Window",
        juce::NormalisableRange<float> (2.0f, 120.0f, 0.1f, 0.55f), 55.0f, "ms"));

    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Param::lookahead, 1 }, "Lookahead",
        juce::NormalisableRange<float> (10.0f, 300.0f, 0.1f, 0.5f), 90.0f, "ms"));

    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Param::smooth, 1 }, "Smooth",
        juce::NormalisableRange<float> (1.0f, 25.0f, 0.1f), 5.0f, "ms"));

    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Param::swing, 1 }, "Swing",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, "%"));

    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Param::preserve, 1 }, "Transient Preserve",
        juce::NormalisableRange<float> (0.0f, 40.0f, 0.1f), 18.0f, "ms"));

    p.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { Param::quality, 1 }, "Quality",
        juce::StringArray { "LIVE", "STUDIO" }, 1));

    return { p.begin(), p.end() };
}

void JerzyAudioQuantizerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    transientDetector.prepare (sampleRate);
    quantizer.prepare (sampleRate);
    warper.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());

    updateLatencyFromParameter();

    fallbackPpq = 0.0;
    lastHostPpq = 0.0;
    hadHostPpq = false;
}

void JerzyAudioQuantizerAudioProcessor::releaseResources()
{
}

bool JerzyAudioQuantizerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return in == juce::AudioChannelSet::mono()
        || in == juce::AudioChannelSet::stereo();
}

void JerzyAudioQuantizerAudioProcessor::updateLatencyFromParameter()
{
    const float lookaheadMs = apvts.getRawParameterValue (Param::lookahead)->load();
    warper.setBaseDelayMs (lookaheadMs);

    const int newLatency = warper.getBaseDelaySamples();
    if (newLatency != currentLatencySamples)
    {
        currentLatencySamples = newLatency;
        setLatencySamples (currentLatencySamples);
    }
}

void JerzyAudioQuantizerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                       juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numInputChannels = getTotalNumInputChannels();
    const int numOutputChannels = getTotalNumOutputChannels();

    for (int ch = numInputChannels; ch < numOutputChannels; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() == 0 || numInputChannels == 0)
        return;

    updateLatencyFromParameter();

    const bool enabled = apvts.getRawParameterValue (Param::enabled)->load() > 0.5f;
    const float sensitivity = apvts.getRawParameterValue (Param::sensitivity)->load();
    const float thresholdDb = apvts.getRawParameterValue (Param::threshold)->load();
    const int gridIndex = static_cast<int> (apvts.getRawParameterValue (Param::grid)->load());
    const float strength = apvts.getRawParameterValue (Param::strength)->load() * 0.01f;
    const float windowMs = apvts.getRawParameterValue (Param::window)->load();
    const float smoothMs = apvts.getRawParameterValue (Param::smooth)->load();
    const float swing = apvts.getRawParameterValue (Param::swing)->load() * 0.01f;
    const float preserveMs = apvts.getRawParameterValue (Param::preserve)->load();
    const int qualityIndex = static_cast<int> (apvts.getRawParameterValue (Param::quality)->load());

    warper.setTransientPreserveMs (preserveMs);
    warper.setQuality (qualityIndex == 0 ? WSOLAQuantizeWarper::Quality::live
                                         : WSOLAQuantizeWarper::Quality::studio);

    transientDetector.setSensitivity (sensitivity);
    transientDetector.setThresholdDb (thresholdDb);

    float inPeak = 0.0f;
    for (int ch = 0; ch < numInputChannels; ++ch)
        inPeak = juce::jmax (inPeak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
    lastInputPeak.store (inPeak, std::memory_order_relaxed);

    double bpm = 120.0;
    double blockStartPpq = fallbackPpq;
    bool gotHostTiming = false;

    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            if (auto hostBpm = position->getBpm())
                bpm = *hostBpm;

            if (auto hostPpq = position->getPpqPosition())
            {
                blockStartPpq = *hostPpq;
                gotHostTiming = true;

                if (hadHostPpq)
                {
                    const double expectedAdvance = static_cast<double> (buffer.getNumSamples())
                                                 * bpm / (60.0 * currentSampleRate);
                    const double error = std::abs ((blockStartPpq - lastHostPpq) - expectedAdvance);

                    if (error > 0.5)
                    {
                        transientDetector.reset();
                        warper.reset();
                    }
                }

                lastHostPpq = blockStartPpq;
                hadHostPpq = true;
            }
        }
    }

    if (! gotHostTiming)
        hadHostPpq = false;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < numInputChannels; ++ch)
            mono += buffer.getSample (ch, i);
        mono /= static_cast<float> (numInputChannels);

        if (! transientDetector.processSample (mono))
            continue;

        transientFlash.store (6, std::memory_order_relaxed);

        if (! enabled)
            continue;

        const auto result = quantizer.quantize (
            blockStartPpq,
            i,
            bpm,
            static_cast<QuantizerEngine::Grid> (
                juce::jlimit (0, 5, gridIndex)),
            strength,
            windowMs,
            swing);

        if (! result.valid)
            continue;

        const int correction = juce::jmax (
            result.correctionSamples,
            -juce::jmax (0, warper.getBaseDelaySamples() - 2));

        warper.scheduleCorrectionSamples (correction, smoothMs);
        correctionFlash.store (6, std::memory_order_relaxed);
        lastCorrectionMs.store (
            static_cast<float> (1000.0 * correction / currentSampleRate),
            std::memory_order_relaxed);
    }

    if (! enabled && wasEnabled)
        warper.returnToBase (smoothMs);

    wasEnabled = enabled;
    warper.process (buffer);

    float outPeak = 0.0f;
    for (int ch = 0; ch < numOutputChannels; ++ch)
        outPeak = juce::jmax (outPeak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
    lastOutputPeak.store (outPeak, std::memory_order_relaxed);

    if (transientFlash.load() > 0)
        transientFlash.fetch_sub (1);
    if (correctionFlash.load() > 0)
        correctionFlash.fetch_sub (1);

    const double advance = static_cast<double> (buffer.getNumSamples())
                         * bpm / (60.0 * currentSampleRate);
    fallbackPpq = blockStartPpq + advance;
}

void JerzyAudioQuantizerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void JerzyAudioQuantizerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* JerzyAudioQuantizerAudioProcessor::createEditor()
{
    return new JerzyAudioQuantizerAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JerzyAudioQuantizerAudioProcessor();
}
