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
    static constexpr auto dynamics = "dynamics";
}

JerzyAudioQuantizerAudioProcessorEditor::JerzyAudioQuantizerAudioProcessorEditor(
    JerzyAudioQuantizerAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setResizable(true, true);
    setResizeLimits(1020, 720, 1720, 1120);
    setSize(1360, 920);

    enabled.setClickingTogglesState(true);
    enabled.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible(enabled);

    grid.addItemList({
        "AUTO — rozpoznaj z nagrania",
        "Ćwierćnuty 1/4",
        "Ósemki 1/8",
        "Szesnastki 1/16",
        "Trzydziestodwójki 1/32",
        "Triole ósemkowe 1/8T",
        "Triole szesnastkowe 1/16T",
        "Shuffle ósemkowy",
        "Shuffle szesnastkowy"
    }, 1);
    addAndMakeVisible(grid);

    gridTitle.setText("DO JAKIEGO RYTMU MA WYRÓWNYWAĆ", juce::dontSendNotification);
    gridTitle.setColour(juce::Label::textColourId, juce::Colours::white);
    gridTitle.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(gridTitle);

    gridHelp.setText("AUTO analizuje kilka ostatnich pewnych uderzeń. Możesz też wymusić konkretny podział lub shuffle.",
                     juce::dontSendNotification);
    gridHelp.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    gridHelp.setFont(juce::FontOptions(12.0f));
    gridHelp.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(gridHelp);

    addKnob(sensitivity, sensitivityText,
            "JAK ŁATWO ROZPOZNAJE UDERZENIE KOSTKI",
            "Więcej = wykrywa delikatniejsze ataki. Zmniejsz, jeśli łapie przesuwanie palców.");

    addKnob(threshold, thresholdText,
            "PONIŻEJ JAKIEGO POZIOMU IGNORUJE DŹWIĘK",
            "Odcina szum, brum i ciche przecieki, żeby nie tworzyły fałszywych punktów korekcji.");

    addKnob(strength, strengthText,
            "JAK MOCNO DOCIĄGA GRĘ DO RYTMU",
            "0% zostawia timing bez zmian. 100% próbuje ustawić pewne ataki dokładnie na siatce.");

    addKnob(window, windowText,
            "JAK DUŻY BŁĄD RYTMU JESZCZE NAPRAWIA",
            "Większa wartość pozwala ratować mocno spóźnione lub przyspieszone uderzenia.");

    addKnob(analysis, analysisText,
            "ILE NAGRANIA SPRAWDZA PRZED DECYZJĄ",
            "Więcej materiału daje stabilniejszą decyzję. Wersja 2 utrzymuje stałą latencję dla hosta.");

    addKnob(preserve, preserveText,
            "JAK MOCNO CHRONI POCZĄTEK NUTY",
            "Zostawia atak kostki naturalny i przenosi rozciąganie głównie na dalszą część dźwięku.");

    addKnob(swing, swingText,
            "ILE SWINGU DODAJE DO PROSTEJ SIATKI",
            "Przesuwa co drugi krok. Przy wybranym Shuffle ustawienie jest już narzucone przez rytm.");

    addKnob(dynamics, dynamicsText,
            "JAK MOCNO WYRÓWNUJE GŁOŚNOŚĆ UDERZEŃ",
            "Łagodnie uspokaja zbyt mocne uderzenia po korekcji rytmu. Nie zmienia wykrywania ataków.");

    threshold.setTextValueSuffix(" dB");
    strength.setTextValueSuffix(" %");
    window.setTextValueSuffix(" ms");
    analysis.setTextValueSuffix(" ms");
    preserve.setTextValueSuffix(" ms");
    swing.setTextValueSuffix(" %");
    dynamics.setTextValueSuffix(" %");

    setupHint.setText(
        "USTAWIANIE: najpierw dopasuj wykrywanie ataków, potem siłę korekcji i maksymalny błąd. "
        "Jeśli ryzyko artefaktów rośnie, zmniejsz siłę lub zakres naprawy.",
        juce::dontSendNotification);
    setupHint.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    setupHint.setJustificationType(juce::Justification::centredLeft);
    setupHint.setFont(juce::FontOptions(12.5f));
    addAndMakeVisible(setupHint);

    stats.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    stats.setJustificationType(juce::Justification::centredLeft);
    stats.setFont(juce::FontOptions(12.0f));
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
    dynamicsA = std::make_unique<SliderAttachment>(processor.apvts, Param::dynamics, dynamics);

    startTimerHz(20);
}

JerzyAudioQuantizerAudioProcessorEditor::~JerzyAudioQuantizerAudioProcessorEditor()\n{\n    setLookAndFeel(nullptr);\n}\n\nvoid JerzyAudioQuantizerAudioProcessorEditor::addKnob(
    Knob& knob, ControlText& text, const juce::String& title, const juce::String& help)
{
    addAndMakeVisible(knob);

    text.title.setText(title, juce::dontSendNotification);
    text.title.setColour(juce::Label::textColourId, juce::Colours::white);
    text.title.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    text.title.setJustificationType(juce::Justification::centred);
    text.title.setMinimumHorizontalScale(0.78f);
    addAndMakeVisible(text.title);

    text.help.setText(help, juce::dontSendNotification);
    text.help.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    text.help.setFont(juce::FontOptions(11.5f));
    text.help.setJustificationType(juce::Justification::centredTop);
    text.help.setMinimumHorizontalScale(0.82f);
    addAndMakeVisible(text.help);
}

void JerzyAudioQuantizerAudioProcessorEditor::drawMeter(
    juce::Graphics& g, juce::Rectangle<float> r, float value,
    const juce::String& title, const juce::String& valueText) const
{
    value = juce::jlimit(0.0f, 1.0f, value);

    g.setColour(juce::Colour(0xff111417));
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(juce::Colour(0xff383e44));
    g.drawRoundedRectangle(r, 8.0f, 1.0f);

    auto inside = r.reduced(10.0f);
    auto textArea = inside.removeFromTop(24.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    g.drawText(title, textArea, juce::Justification::centredLeft);

    g.setColour(juce::Colours::lightgrey);
    g.setFont(juce::FontOptions(11.5f));
    g.drawText(valueText, textArea, juce::Justification::centredRight);

    auto meter = inside.reduced(0.0f, 6.0f);
    g.setColour(juce::Colour(0xff272c31));
    g.fillRoundedRectangle(meter, 4.0f);

    auto fill = meter;
    fill.setWidth(meter.getWidth() * value);
    g.setColour(juce::Colour(0xffd0d5da));
    g.fillRoundedRectangle(fill, 4.0f);
}

void JerzyAudioQuantizerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff090b0d));
    const auto b = getLocalBounds().toFloat();

    juce::ColourGradient grad(juce::Colour(0xff24292e), b.getTopLeft(),
                              juce::Colour(0xff0b0d0f), b.getBottomRight(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(b.reduced(8.0f), 15.0f);

    g.setColour(juce::Colour(0xff555d64));
    g.drawRoundedRectangle(b.reduced(9.0f), 15.0f, 1.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(30.0f, juce::Font::bold));
    g.drawText("JERZY AUDIO QUANTIZER 2", 24, 16, getWidth() - 48, 42,
               juce::Justification::centred);

    g.setColour(juce::Colours::lightgrey);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("Studyjna korekcja nierównej gitary — host tempo + markery ataków + bezpieczny time-stretch",
               24, 56, getWidth() - 48, 22, juce::Justification::centred);

    const float conf = processor.attackConfidence.load(std::memory_order_relaxed);
    const float ratio = processor.lastStretchRatio.load(std::memory_order_relaxed);
    const float risk = processor.artifactRisk.load(std::memory_order_relaxed);
    const float corr = processor.lastCorrectionMs.load(std::memory_order_relaxed);
    const float dyn = processor.dynamicsReductionDb.load(std::memory_order_relaxed);
    const float bpm = processor.currentBpm.load(std::memory_order_relaxed);

    auto meterArea = juce::Rectangle<float>(24.0f, 92.0f, getWidth() - 48.0f, 72.0f);
    constexpr float gap = 9.0f;
    const float w = (meterArea.getWidth() - gap * 3.0f) / 4.0f;

    drawMeter(g, meterArea.removeFromLeft(w), conf,
              "CZY TO PRAWDZIWY ATAK", juce::String(conf * 100.0f, 0) + " %");
    meterArea.removeFromLeft(gap);

    drawMeter(g, meterArea.removeFromLeft(w),
              juce::jlimit(0.0f, 1.0f, std::abs(corr) / 120.0f),
              "OSTATNIA POPRAWKA",
              (corr >= 0.0f ? "+" : "") + juce::String(corr, 1) + " ms");
    meterArea.removeFromLeft(gap);

    juce::String riskText = "NISKIE";
    if (risk > 0.66f) riskText = "WYSOKIE";
    else if (risk > 0.33f) riskText = "ŚREDNIE";

    drawMeter(g, meterArea.removeFromLeft(w), risk,
              "RYZYKO ARTEFAKTÓW",
              riskText + "  " + juce::String(ratio, 3) + "x");
    meterArea.removeFromLeft(gap);

    drawMeter(g, meterArea,
              juce::jlimit(0.0f, 1.0f, dyn / 9.0f),
              "WYRÓWNANIE DYNAMIKI",
              juce::String(dyn, 1) + " dB  |  " + juce::String(bpm, 1) + " BPM");
}

void JerzyAudioQuantizerAudioProcessorEditor::layoutCell(
    juce::Rectangle<int> cell, Knob& knob, ControlText& text)
{
    cell.reduce(8, 5);
    text.title.setBounds(cell.removeFromTop(28));
    text.help.setBounds(cell.removeFromTop(42).reduced(8, 0));
    cell.removeFromTop(2);
    knob.setBounds(cell.reduced(22, 0));
}

void JerzyAudioQuantizerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(32);
    area.removeFromTop(166);

    auto setup = area.removeFromTop(92);
    enabled.setBounds(setup.removeFromLeft(250).reduced(10, 24));
    setup.removeFromLeft(18);

    auto gridArea = setup.reduced(6, 2);
    gridTitle.setBounds(gridArea.removeFromTop(23));
    gridHelp.setBounds(gridArea.removeFromTop(34));
    grid.setBounds(gridArea.removeFromTop(32));

    area.removeFromTop(8);

    auto bottom = area.removeFromBottom(72);
    stats.setBounds(bottom.removeFromTop(28));
    setupHint.setBounds(bottom.reduced(2));

    area.removeFromBottom(4);

    const int columns = 3;
    const int rows = 3;
    const int gapX = 12;
    const int gapY = 10;
    const int cellW = (area.getWidth() - gapX * (columns - 1)) / columns;
    const int cellH = (area.getHeight() - gapY * (rows - 1)) / rows;

    struct Item { Knob* knob; ControlText* text; };
    Item items[] = {
        { &sensitivity, &sensitivityText },
        { &threshold, &thresholdText },
        { &strength, &strengthText },
        { &window, &windowText },
        { &analysis, &analysisText },
        { &preserve, &preserveText },
        { &swing, &swingText },
        { &dynamics, &dynamicsText }
    };

    for (int i = 0; i < 8; ++i)
    {
        const int row = i / columns;
        const int col = i % columns;
        juce::Rectangle<int> cell(
            area.getX() + col * (cellW + gapX),
            area.getY() + row * (cellH + gapY),
            cellW,
            cellH);
        layoutCell(cell, *items[i].knob, *items[i].text);
    }
}

void JerzyAudioQuantizerAudioProcessorEditor::timerCallback()
{
    const int detected = processor.detectedAttacks.load(std::memory_order_relaxed);
    const int accepted = processor.acceptedAttacks.load(std::memory_order_relaxed);
    const int rejected = processor.rejectedAttacks.load(std::memory_order_relaxed);

    stats.setText(
        "Wykryte ataki: " + juce::String(detected)
        + "   |   użyte do poprawy: " + juce::String(accepted)
        + "   |   odrzucone jako niepewne / zbyt odległe: " + juce::String(rejected),
        juce::dontSendNotification);

    repaint();
}
