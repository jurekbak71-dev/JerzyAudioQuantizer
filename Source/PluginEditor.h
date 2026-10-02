#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class JerzyAudioQuantizerAudioProcessorEditor final
    : public juce::AudioProcessorEditor,
      private juce::Timer
{
public:
    explicit JerzyAudioQuantizerAudioProcessorEditor (JerzyAudioQuantizerAudioProcessor&);
    ~JerzyAudioQuantizerAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob : public juce::Slider
    {
        Knob()
        {
            setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 20);
        }
    };

    void addKnob (Knob&, juce::Label&, const juce::String& name);

    JerzyAudioQuantizerAudioProcessor& processor;

    juce::ToggleButton enabled { "QUANTIZE" };
    juce::ComboBox grid;
    juce::ComboBox quality;

    Knob sensitivity, threshold, strength, window, lookahead, smooth, swing, preserve;
    juce::Label sensitivityL, thresholdL, strengthL, windowL, lookaheadL, smoothL, swingL, preserveL;
    juce::Label gridL, qualityL;

    juce::Label status;
    juce::Label correction;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ButtonAttachment> enabledA;
    std::unique_ptr<ComboAttachment> gridA, qualityA;
    std::unique_ptr<SliderAttachment> sensitivityA, thresholdA, strengthA, windowA;
    std::unique_ptr<SliderAttachment> lookaheadA, smoothA, swingA, preserveA;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JerzyAudioQuantizerAudioProcessorEditor)
};
