#pragma once
#include <JuceHeader.h>
#include "TransientDetector.h"
#include "QuantizerEngine.h"
#include "StudioSegmentEngine.h"
#include "DynamicsLeveler.h"

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
    double getTailLengthSeconds() const override { return 0.0; }

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
    std::atomic<float> dynamicsReductionDb { 0.0f };
    std::atomic<float> currentBpm { 120.0f };
    std::atomic<int> detectedAttacks { 0 };
    std::atomic<int> acceptedAttacks { 0 };
    std::atomic<int> rejectedAttacks { 0 };

private:
    TransientDetector detector;
    QuantizerEngine quantizer;
    StudioSegmentEngine studioEngine;
    DynamicsLeveler dynamics;

    double sampleRateHz = 44100.0;
    std::int64_t absoluteSamples = 0;
    double fallbackPpq = 0.0;
    std::int64_t lastHostSamplePosition = -1;
    int previousBlockSize = 0;
    bool wasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JerzyAudioQuantizerAudioProcessor)
};
