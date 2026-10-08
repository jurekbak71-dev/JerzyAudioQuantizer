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

    struct ParameterRow
    {
        juce::Label title;
        juce::Label help;
        juce::Slider slider;
    };

    void setupRow(ParameterRow&, const juce::String& title,
                  const juce::String& help, const juce::String& suffix = {});
    void layoutRow(ParameterRow&, juce::Rectangle<int>);
    void drawStatus(juce::Graphics&, juce::Rectangle<float>,
                    const juce::String& title, const juce::String& value,
                    float amount) const;

    JerzyAudioQuantizerAudioProcessor& processor;

    juce::ToggleButton enabled { "W\u0141\u0104CZ KOREKCJ\u0118 RYTMU" };

    juce::Label gridTitle;
    juce::Label gridHelp;
    juce::ComboBox grid;

    ParameterRow sensitivity;
    ParameterRow threshold;
    ParameterRow strength;
    ParameterRow window;
    ParameterRow preserve;
    ParameterRow swing;
    ParameterRow dynamics;

    juce::Label stats;
    juce::Label footer;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ButtonAttachment> enabledA;
    std::unique_ptr<ComboAttachment> gridA;
    std::unique_ptr<SliderAttachment> sensitivityA;
    std::unique_ptr<SliderAttachment> thresholdA;
    std::unique_ptr<SliderAttachment> strengthA;
    std::unique_ptr<SliderAttachment> windowA;
    std::unique_ptr<SliderAttachment> preserveA;
    std::unique_ptr<SliderAttachment> swingA;
    std::unique_ptr<SliderAttachment> dynamicsA;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JerzyAudioQuantizerAudioProcessorEditor)
};
