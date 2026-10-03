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
    setResizeLimits(900, 650, 1600, 1050);
    setSize(1240, 840);

    enabled.setClickingTogglesState(true);
    enabled.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible(enabled);

    grid.addItemList({ "AUTO", "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" }, 1);
    addAndMakeVisible(grid);

    gridL.setText("SIATKA RYTMU — tryb AUTO jest zalecany przy mocno nierównej grze", juce::dontSendNotification);
    gridL.setJustificationType(juce::Justification::centredLeft);
    gridL.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(gridL);

    addKnob(sensitivity, sensitivityL, "CZUŁOŚĆ ATAKU KOSTKI");
    addKnob(threshold, thresholdL, "PRÓG SZUMU I PRZECIEKÓW");
    addKnob(strength, strengthL, "SIŁA KOREKCJI RYTMU");
    addKnob(window, windowL, "MAKSYMALNY BŁĄD CZASU");
    addKnob(analysis, analysisL, "DŁUGOŚĆ ANALIZY AUDIO");
    addKnob(preserve, preserveL, "OCHRONA ATAKU DŹWIĘKU");
    addKnob(swing, swingL, "SWING RYTMU");

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
    label.setMinimumHorizontalScale(0.88f);
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
    auto area = getLocalBounds().reduced(34);
    area.removeFromTop(172);

    auto setup = area.removeFromTop(86);
    enabled.setBounds(setup.removeFromLeft(280).reduced(12, 16));

    setup.removeFromLeft(24);

    auto gridArea = setup.removeFromLeft(520).reduced(10, 6);
    gridL.setBounds(gridArea.removeFromTop(30));
    gridArea.removeFromTop(4);
    grid.setBounds(gridArea.removeFromTop(38));

    area.removeFromTop(16);

    // Trzy szerokie kolumny zamiast czterech:
    // podpisy mają więcej miejsca i nie nachodzą na potencjometry.
    const int columns = 3;
    const int rows = 3;
    const int gapX = 18;
    const int gapY = 14;

    auto controlsArea = area;
    controlsArea.removeFromBottom(118);

    const int cellW = (controlsArea.getWidth() - gapX * (columns - 1)) / columns;
    const int cellH = (controlsArea.getHeight() - gapY * (rows - 1)) / rows;

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
            controlsArea.getX() + col * (cellW + gapX),
            controlsArea.getY() + row * (cellH + gapY),
            cellW,
            cellH);

        cell.reduce(10, 6);

        auto labelArea = cell.removeFromTop(34);
        items[i].label->setBounds(labelArea);

        cell.removeFromTop(6);
        items[i].knob->setBounds(cell.reduced(14, 0));
    }

    auto bottom = area.removeFromBottom(108);
    stats.setBounds(bottom.removeFromTop(30));
    bottom.removeFromTop(8);
    hint.setBounds(bottom.reduced(2));
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
