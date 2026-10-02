#pragma once
#include <JuceHeader.h>
#include "TransientDetector.h"
#include "QuantizerEngine.h"
#include "WSOLAQuantizeWarper.h"

class JerzyAudioQuantizerAudioProcessor final : public juce::AudioProcessor
{
public:
    JerzyAudioQuantizerAudioProcessor();
    ~JerzyAudioQuantizerAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    double getTailLengthSeconds() const override { return 0.35; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> lastInputPeak { 0.0f };
    std::atomic<float> lastOutputPeak { 0.0f };
    std::atomic<int> transientFlash { 0 };
    std::atomic<int> correctionFlash { 0 };
    std::atomic<float> lastCorrectionMs { 0.0f };

private:
    void updateLatencyFromParameter();

    TransientDetector transientDetector;
    QuantizerEngine quantizer;
    WSOLAQuantizeWarper warper;

    double currentSampleRate = 44100.0;
    int currentLatencySamples = 0;

    double fallbackPpq = 0.0;
    double lastHostPpq = 0.0;
    bool hadHostPpq = false;
    bool wasEnabled = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JerzyAudioQuantizerAudioProcessor)
};
