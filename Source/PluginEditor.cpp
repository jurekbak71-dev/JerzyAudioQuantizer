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

JerzyAudioQuantizerAudioProcessorEditor::JerzyAudioQuantizerAudioProcessorEditor(
    JerzyAudioQuantizerAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setResizable(true, true);
    setResizeLimits(760, 500, 1500, 980);
    setSize(1120, 720);

    enabled.setClickingTogglesState(true);
    enabled.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible(enabled);

    grid.addItemList({ "AUTO", "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" }, 1);
    addAndMakeVisible(grid);

    gridL.setText("SIATKA RYTMU — AUTO jest zalecane do nierównej gry", juce::dontSendNotification);
    gridL.setJustificationType(juce::Justification::centredLeft);
    gridL.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(gridL);

    addKnob(sensitivity, sensitivityL, "CZUŁOŚĆ NA ATAK KOSTKI");
    addKnob(threshold, thresholdL, "PRÓG IGNOROWANIA SZUMU");
    addKnob(strength, strengthL, "JAK MOCNO POPRAWIA RYTM");
    addKnob(window, windowL, "MAKS. BŁĄD DO NAPRAWY");
    addKnob(analysis, analysisL, "ILE AUDIO ANALIZUJE WCZEŚNIEJ");
    addKnob(preserve, preserveL, "OCHRONA POCZĄTKU DŹWIĘKU");
    addKnob(swing, swingL, "SWING SIATKI");

    threshold.setTextValueSuffix(" dB");
    strength.setTextValueSuffix(" %");
    window.setTextValueSuffix(" ms");
    analysis.setTextValueSuffix(" ms");
    preserve.setTextValueSuffix(" ms");
    swing.setTextValueSuffix(" %");

    hint.setText(
        "Strojenie: 1) ustaw czułość tak, aby wskaźnik ATAK zapalał się tylko przy prawdziwych uderzeniach; "
        "2) zwiększ maksymalny błąd tylko wtedy, gdy partia jest mocno rozchwiana; "
        "3) gdy RYZYKO ARTEFAKTÓW rośnie, zmniejsz siłę poprawy lub maksymalny błąd.",
        juce::dontSendNotification);
    hint.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    hint.setJustificationType(juce::Justification::topLeft);
    hint.setMinimumHorizontalScale(0.8f);
    addAndMakeVisible(hint);

    stats.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    stats.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(stats);

    enabledA = std::make_unique<ButtonAttachment>(processor.apvts, Param::enabled, enabled);
    gridA = std::make_unique<ComboAttachment>(processor.apvts, Param::grid, grid);

    sensitivityA = std::make_unique<SliderAttachment>(processor.apvts, Param::sensitivity, sensitivity);
    thresholdA = std::make_unique<SliderAttachment>(processor.apvts, Param::threshold, threshold);
    strengthA = std::make_unique<SliderAttachment>(processor.apvts, Param::strength, strength);
    windowA = std::make_unique<SliderAttachment>(processor.apvts, Param::window, window);
    analysisA = std::make_unique<SliderAttachment>(processor.apvts, Param::analysis, analysis);
    preserveA = std::make_unique<SliderAttachment>(processor.apvts, Param::preserve, preserve);
    swingA = std::make_unique<SliderAttachment>(processor.apvts, Param::swing, swing);

    startTimerHz(30);
}

void JerzyAudioQuantizerAudioProcessorEditor::addKnob(
    Knob& knob, juce::Label& label, const juce::String& text)
{
    addAndMakeVisible(knob);
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    label.setJustificationType(juce::Justification::centred);
    label.setMinimumHorizontalScale(0.72f);
    addAndMakeVisible(label);
}

void JerzyAudioQuantizerAudioProcessorEditor::drawMeter(
    juce::Graphics& g, juce::Rectangle<float> r, float value,
    const juce::String& title, const juce::String& valueText) const
{
    value = juce::jlimit(0.0f, 1.0f, value);

    g.setColour(juce::Colour(0xff111417));
    g.fillRoundedRectangle(r, 7.0f);

    auto bar = r.reduced(8.0f);
    auto textArea = bar.removeFromTop(22.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText(title, textArea, juce::Justification::centredLeft);

    g.setColour(juce::Colours::lightgrey);
    g.setFont(juce::FontOptions(12.0f));
    g.drawText(valueText, textArea, juce::Justification::centredRight);

    auto meter = bar.reduced(0.0f, 5.0f);
    g.setColour(juce::Colour(0xff262b30));
    g.fillRoundedRectangle(meter, 4.0f);

    auto fill = meter;
    fill.setWidth(meter.getWidth() * value);

    // Intentionally uses neutral UI colours; semantic text carries the meaning.
    g.setColour(juce::Colour(0xffc8cdd2));
    g.fillRoundedRectangle(fill, 4.0f);
}

void JerzyAudioQuantizerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0b0d0f));
    const auto b = getLocalBounds().toFloat();

    juce::ColourGradient grad(juce::Colour(0xff252a2f), b.getTopLeft(),
                              juce::Colour(0xff0c0e10), b.getBottomRight(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(b.reduced(8.0f), 15.0f);

    g.setColour(juce::Colour(0xff626970));
    g.drawRoundedRectangle(b.reduced(9.0f), 15.0f, 1.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    g.drawText("JERZY AUDIO QUANTIZER — STUDIO", 24, 18, getWidth()-48, 40, juce::Justification::centred);

    g.setColour(juce::Colours::lightgrey);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("Korekcja rytmu nagranej gitary na podstawie pewnych transjentów i segmentowego time-stretchu",
               24, 55, getWidth()-48, 24, juce::Justification::centred);

    const float conf = processor.attackConfidence.load();
    const float ratio = processor.lastStretchRatio.load();
    const float risk = processor.artifactRisk.load();
    const float corr = processor.lastCorrectionMs.load();

    auto meterArea = juce::Rectangle<float>(24.0f, 96.0f, getWidth()-48.0f, 76.0f);
    const float gap = 10.0f;
    const float w = (meterArea.getWidth() - gap * 2.0f) / 3.0f;

    drawMeter(g, meterArea.removeFromLeft(w), conf,
              "PEWNOŚĆ: PRAWDZIWY ATAK",
              juce::String(conf * 100.0f, 0) + " %");
    meterArea.removeFromLeft(gap);

    const float correctionNorm = juce::jlimit(0.0f, 1.0f, std::abs(corr) / 120.0f);
    drawMeter(g, meterArea.removeFromLeft(w), correctionNorm,
              "OSTATNIA KOREKTA",
              (corr >= 0 ? "+" : "") + juce::String(corr, 1) + " ms");
    meterArea.removeFromLeft(gap);

    juce::String riskText = "NISKIE";
    if (risk > 0.66f) riskText = "WYSOKIE — ZMNIEJSZ KOREKTĘ";
    else if (risk > 0.33f) riskText = "ŚREDNIE";

    drawMeter(g, meterArea, risk,
              "RYZYKO ARTEFAKTÓW",
              riskText + "   stretch " + juce::String(ratio, 3) + "x");
}

void JerzyAudioQuantizerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(28);
    area.removeFromTop(160);

    auto setup = area.removeFromTop(72);
    enabled.setBounds(setup.removeFromLeft(240).reduced(8));

    auto gridArea = setup.removeFromLeft(390).reduced(8);
    gridL.setBounds(gridArea.removeFromTop(24));
    grid.setBounds(gridArea.removeFromTop(34));

    area.removeFromTop(8);

    const int columns = 4;
    const int rows = 2;
    const int cellW = area.getWidth() / columns;
    const int cellH = juce::jmin(180, area.getHeight() / 2);

    struct Item { Knob* knob; juce::Label* label; };
    Item items[] = {
        { &sensitivity, &sensitivityL },
        { &threshold, &thresholdL },
        { &strength, &strengthL },
        { &window, &windowL },
        { &analysis, &analysisL },
        { &preserve, &preserveL },
        { &swing, &swingL }
    };

    for (int i = 0; i < 7; ++i)
    {
        const int row = i / columns;
        const int col = i % columns;

        juce::Rectangle<int> cell(
            area.getX() + col * cellW,
            area.getY() + row * cellH,
            cellW,
            cellH);

        cell.reduce(8, 4);
        items[i].label->setBounds(cell.removeFromTop(30));
        items[i].knob->setBounds(cell);
    }

    auto bottom = getLocalBounds().reduced(28);
    bottom.removeFromTop(160 + cellH * 2 + 8);
    stats.setBounds(bottom.removeFromTop(28));
    hint.setBounds(bottom.reduced(4));
}

void JerzyAudioQuantizerAudioProcessorEditor::timerCallback()
{
    const int detected = processor.detectedAttacks.load();
    const int accepted = processor.acceptedAttacks.load();
    const int rejected = processor.rejectedAttacks.load();

    stats.setText(
        "Ataki wykryte: " + juce::String(detected)
        + "   |   użyte do korekcji: " + juce::String(accepted)
        + "   |   odrzucone jako niepewne / poza zakresem: " + juce::String(rejected),
        juce::dontSendNotification);

    repaint();
}
