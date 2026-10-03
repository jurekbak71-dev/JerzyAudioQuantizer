#pragma once
#include <JuceHeader.h>
#include "TransientDetector.h"
#include "QuantizerEngine.h"
#include "StudioSegmentEngine.h"

class JerzyAudioQuantizerAudioProcessor final : public juce::AudioProcessor
{
public:
    JerzyAudioQuantizerAudioProcessor();
    ~JerzyAudioQuantizerAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> attackConfidence { 0.0f };
    std::atomic<float> lastCorrectionMs { 0.0f };
    std::atomic<float> lastStretchRatio { 1.0f };
    std::atomic<float> artifactRisk { 0.0f };
    std::atomic<int> detectedAttacks { 0 };
    std::atomic<int> acceptedAttacks { 0 };
    std::atomic<int> rejectedAttacks { 0 };
    std::atomic<int> transientFlash { 0 };
    std::atomic<int> correctionFlash { 0 };

private:
    void updateLatency();

    TransientDetector detector;
    QuantizerEngine quantizer;
    StudioSegmentEngine studioEngine;

    double sampleRateHz = 44100.0;
    std::int64_t absoluteSamples = 0;
    double fallbackPpq = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JerzyAudioQuantizerAudioProcessor)
};
