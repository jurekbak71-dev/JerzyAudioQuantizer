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

JerzyAudioQuantizerAudioProcessorEditor::JerzyAudioQuantizerAudioProcessorEditor (
    JerzyAudioQuantizerAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setResizable (true, true);
    setResizeLimits (560, 360, 1400, 900);
    setSize (920, 560);

    enabled.setClickingTogglesState (true);
    enabled.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible (enabled);

    grid.addItemList ({ "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" }, 1);
    addAndMakeVisible (grid);
    gridL.setText ("GRID", juce::dontSendNotification);
    gridL.setJustificationType (juce::Justification::centred);
    gridL.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (gridL);

    quality.addItemList ({ "LIVE", "STUDIO" }, 1);
    addAndMakeVisible (quality);
    qualityL.setText ("QUALITY", juce::dontSendNotification);
    qualityL.setJustificationType (juce::Justification::centred);
    qualityL.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (qualityL);

    addKnob (sensitivity, sensitivityL, "SENSITIVITY");
    addKnob (threshold, thresholdL, "THRESHOLD");
    addKnob (strength, strengthL, "STRENGTH");
    addKnob (window, windowL, "WINDOW");
    addKnob (lookahead, lookaheadL, "LOOKAHEAD");
    addKnob (smooth, smoothL, "SMOOTH");
    addKnob (swing, swingL, "SWING");
    addKnob (preserve, preserveL, "TRANSIENT PRESERVE");

    threshold.setTextValueSuffix (" dB");
    strength.setTextValueSuffix (" %");
    window.setTextValueSuffix (" ms");
    lookahead.setTextValueSuffix (" ms");
    smooth.setTextValueSuffix (" ms");
    swing.setTextValueSuffix (" %");
    preserve.setTextValueSuffix (" ms");

    status.setJustificationType (juce::Justification::centred);
    correction.setJustificationType (juce::Justification::centred);
    status.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    correction.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (status);
    addAndMakeVisible (correction);

    enabledA = std::make_unique<ButtonAttachment> (processor.apvts, Param::enabled, enabled);
    gridA = std::make_unique<ComboAttachment> (processor.apvts, Param::grid, grid);
    qualityA = std::make_unique<ComboAttachment> (processor.apvts, Param::quality, quality);

    sensitivityA = std::make_unique<SliderAttachment> (processor.apvts, Param::sensitivity, sensitivity);
    thresholdA = std::make_unique<SliderAttachment> (processor.apvts, Param::threshold, threshold);
    strengthA = std::make_unique<SliderAttachment> (processor.apvts, Param::strength, strength);
    windowA = std::make_unique<SliderAttachment> (processor.apvts, Param::window, window);
    lookaheadA = std::make_unique<SliderAttachment> (processor.apvts, Param::lookahead, lookahead);
    smoothA = std::make_unique<SliderAttachment> (processor.apvts, Param::smooth, smooth);
    swingA = std::make_unique<SliderAttachment> (processor.apvts, Param::swing, swing);
    preserveA = std::make_unique<SliderAttachment> (processor.apvts, Param::preserve, preserve);

    startTimerHz (30);
}

void JerzyAudioQuantizerAudioProcessorEditor::addKnob (
    Knob& knob, juce::Label& label, const juce::String& name)
{
    addAndMakeVisible (knob);
    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (label);
}

void JerzyAudioQuantizerAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.fillAll (juce::Colour (0xff0b0d0f));

    juce::ColourGradient grad (
        juce::Colour (0xff22272b), bounds.getTopLeft(),
        juce::Colour (0xff090a0c), bounds.getBottomRight(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bounds.reduced (8.0f), 14.0f);

    g.setColour (juce::Colour (0xff50565c));
    g.drawRoundedRectangle (bounds.reduced (9.0f), 14.0f, 1.2f);

    auto title = getLocalBounds().removeFromTop (70);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    g.drawFittedText ("JERZY AUDIO QUANTIZER", title, juce::Justification::centred, 1);

    const bool tr = processor.transientFlash.load (std::memory_order_relaxed) > 0;
    const bool cor = processor.correctionFlash.load (std::memory_order_relaxed) > 0;

    g.setColour (tr ? juce::Colours::red : juce::Colour (0xff451010));
    g.fillEllipse (26.0f, 26.0f, 12.0f, 12.0f);

    g.setColour (cor ? juce::Colours::yellow : juce::Colour (0xff4b4510));
    g.fillEllipse (44.0f, 26.0f, 12.0f, 12.0f);

    const float in = processor.lastInputPeak.load (std::memory_order_relaxed);
    const float out = processor.lastOutputPeak.load (std::memory_order_relaxed);

    g.setColour (juce::Colour (0xff141719));
    g.fillRoundedRectangle (20.0f, bounds.getHeight() - 30.0f, bounds.getWidth() - 40.0f, 8.0f, 4.0f);

    const float meterWidth = (bounds.getWidth() - 40.0f) * 0.5f;
    g.setColour (juce::Colours::green);
    g.fillRoundedRectangle (20.0f, bounds.getHeight() - 30.0f,
                            meterWidth * juce::jlimit (0.0f, 1.0f, in), 8.0f, 4.0f);
    g.fillRoundedRectangle (20.0f + meterWidth, bounds.getHeight() - 30.0f,
                            meterWidth * juce::jlimit (0.0f, 1.0f, out), 8.0f, 4.0f);
}

void JerzyAudioQuantizerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (28);
    area.removeFromTop (70);
    area.removeFromBottom (44);

    auto top = area.removeFromTop (72);
    enabled.setBounds (top.removeFromLeft (150).reduced (10));

    auto gridArea = top.removeFromLeft (150).reduced (8);
    gridL.setBounds (gridArea.removeFromTop (20));
    grid.setBounds (gridArea.reduced (4));

    auto qualityArea = top.removeFromLeft (150).reduced (8);
    qualityL.setBounds (qualityArea.removeFromTop (20));
    quality.setBounds (qualityArea.reduced (4));

    status.setBounds (top.removeFromLeft (130).reduced (6));
    correction.setBounds (top.reduced (6));

    area.removeFromTop (8);

    const int columns = 4;
    const int rows = 2;
    const int cellW = area.getWidth() / columns;
    const int cellH = area.getHeight() / rows;

    struct Item { Knob* knob; juce::Label* label; };
    Item items[] = {
        { &sensitivity, &sensitivityL },
        { &threshold, &thresholdL },
        { &strength, &strengthL },
        { &window, &windowL },
        { &lookahead, &lookaheadL },
        { &smooth, &smoothL },
        { &swing, &swingL },
        { &preserve, &preserveL }
    };

    for (int idx = 0; idx < 8; ++idx)
    {
        const int row = idx / columns;
        const int col = idx % columns;

        juce::Rectangle<int> cell (
            area.getX() + col * cellW,
            area.getY() + row * cellH,
            cellW,
            cellH);

        cell.reduce (10, 6);
        items[idx].label->setBounds (cell.removeFromTop (24));
        items[idx].knob->setBounds (cell);
    }
}

void JerzyAudioQuantizerAudioProcessorEditor::timerCallback()
{
    const bool transient = processor.transientFlash.load (std::memory_order_relaxed) > 0;
    const bool corrected = processor.correctionFlash.load (std::memory_order_relaxed) > 0;

    status.setText (transient ? "TRANSIENT" : "LISTENING", juce::dontSendNotification);

    if (corrected)
    {
        const float ms = processor.lastCorrectionMs.load (std::memory_order_relaxed);
        correction.setText (
            (ms >= 0.0f ? "+" : "") + juce::String (ms, 1) + " ms",
            juce::dontSendNotification);
    }

    repaint();
}
