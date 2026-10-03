#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class JerzyAudioQuantizerAudioProcessorEditor final
    : public juce::AudioProcessorEditor,
      private juce::Timer
{
public:
    explicit JerzyAudioQuantizerAudioProcessorEditor(JerzyAudioQuantizerAudioProcessor&);
    ~JerzyAudioQuantizerAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob : public juce::Slider
    {
        Knob()
        {
            setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            setTextBoxStyle(juce::Slider::TextBoxBelow, false, 92, 22);
        }
    };

    void addKnob(Knob&, juce::Label&, const juce::String&);
    void drawMeter(juce::Graphics&, juce::Rectangle<float>, float value, const juce::String& title,
                   const juce::String& valueText) const;

    JerzyAudioQuantizerAudioProcessor& processor;

    juce::ToggleButton enabled { "WŁĄCZ KOREKCJĘ RYTMU" };
    juce::ComboBox grid;
    juce::Label gridL;

    Knob sensitivity, threshold, strength, window, analysis, preserve, swing;
    juce::Label sensitivityL, thresholdL, strengthL, windowL, analysisL, preserveL, swingL;

    juce::Label hint;
    juce::Label stats;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ButtonAttachment> enabledA;
    std::unique_ptr<ComboAttachment> gridA;
    std::unique_ptr<SliderAttachment> sensitivityA, thresholdA, strengthA, windowA;
    std::unique_ptr<SliderAttachment> analysisA, preserveA, swingA;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JerzyAudioQuantizerAudioProcessorEditor)
};
