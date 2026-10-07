#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "JerzyVSTGuiKit.h"

class JerzyAudioQuantizerAudioProcessorEditor final
    : public juce::AudioProcessorEditor,
      private juce::Timer
{
public:
    explicit JerzyAudioQuantizerAudioProcessorEditor(JerzyAudioQuantizerAudioProcessor&);
    ~JerzyAudioQuantizerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob : public juce::Slider
    {
        Knob()
        {
            setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            setTextBoxStyle(juce::Slider::TextBoxBelow, false, 104, 24);
            setDoubleClickReturnValue(true, 0.0);
        }
    };

    struct ControlText
    {
        juce::Label title;
        juce::Label help;
    };

    void addKnob(Knob&, ControlText&, const juce::String& title, const juce::String& help);
    void drawMeter(juce::Graphics&, juce::Rectangle<float>, float value,
                   const juce::String& title, const juce::String& valueText) const;
    void layoutCell(juce::Rectangle<int>, Knob&, ControlText&);

    JerzyAudioQuantizerAudioProcessor& processor;
    JerzyAudioUI::HardwareLookAndFeel look { JerzyAudioUI::violet() };

    juce::ToggleButton enabled { "WŁĄCZ POPRAWĘ RYTMU" };
    juce::ComboBox grid;
    juce::Label gridTitle, gridHelp;

    Knob sensitivity, threshold, strength, window, analysis, preserve, swing, dynamics;
    ControlText sensitivityText, thresholdText, strengthText, windowText;
    ControlText analysisText, preserveText, swingText, dynamicsText;

    juce::Label setupHint;
    juce::Label stats;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ButtonAttachment> enabledA;
    std::unique_ptr<ComboAttachment> gridA;
    std::unique_ptr<SliderAttachment> sensitivityA, thresholdA, strengthA, windowA;
    std::unique_ptr<SliderAttachment> analysisA, preserveA, swingA, dynamicsA;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JerzyAudioQuantizerAudioProcessorEditor)
};
