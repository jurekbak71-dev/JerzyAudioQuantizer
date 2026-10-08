#include "PluginEditor.h"

namespace Param
{
    static constexpr auto enabled = "enabled";
    static constexpr auto sensitivity = "sensitivity";
    static constexpr auto threshold = "threshold";
    static constexpr auto grid = "grid";
    static constexpr auto strength = "strength";
    static constexpr auto window = "window";
    static constexpr auto preserve = "preserve";
    static constexpr auto swing = "swing";
    static constexpr auto dynamics = "dynamics";
}

JerzyAudioQuantizerAudioProcessorEditor::JerzyAudioQuantizerAudioProcessorEditor(
    JerzyAudioQuantizerAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setResizable(true, true);
    setResizeLimits(980, 680, 1600, 1040);
    setSize(1180, 800);

    enabled.setClickingTogglesState(true);
    enabled.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible(enabled);

    gridTitle.setText("SIATKA RYTMICZNA", juce::dontSendNotification);
    gridTitle.setColour(juce::Label::textColourId, juce::Colours::white);
    gridTitle.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    addAndMakeVisible(gridTitle);

    gridHelp.setText(
        "Wybierz rytm partii. AUTO pr\u00f3buje go rozpozna\u0107, ale przy precyzyjnej obr\u00f3bce "
        "najpewniejszy jest konkretny podzia\u0142, np. 1/8 lub 1/16.",
        juce::dontSendNotification);
    gridHelp.setColour(juce::Label::textColourId, juce::Colour(0xffb7bcc4));
    gridHelp.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(gridHelp);

    grid.addItemList({
        "AUTO",
        "\u0106wier\u0107nuty 1/4",
        "\u00d3semki 1/8",
        "Szesnastki 1/16",
        "Trzydziestodw\u00f3jki 1/32",
        "Triole \u00f3semkowe 1/8T",
        "Triole szesnastkowe 1/16T",
        "Shuffle 1/8",
        "Shuffle 1/16"
    }, 1);
    grid.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff20242a));
    grid.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    grid.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff6b727c));
    grid.setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    addAndMakeVisible(grid);

    setupRow(sensitivity,
        "CZU\u0141O\u015a\u0106 ATAKU",
        "Wi\u0119cej = wykrywa delikatniejsze uderzenia. Zmniejsz, je\u015bli reaguje na przesuwanie palc\u00f3w.");

    setupRow(threshold,
        "PR\u00d3G CISZY",
        "D\u017awi\u0119ki poni\u017cej tego poziomu nie s\u0105 traktowane jako nowe uderzenia.",
        " dB");

    setupRow(strength,
        "SI\u0141A KWANTYZACJI",
        "Jak mocno przesuwa wykryte uderzenie do wybranej siatki rytmicznej.",
        " %");

    setupRow(window,
        "MAKSYMALNY B\u0141\u0104D DO NAPRAWY",
        "Wi\u0119ksza warto\u015b\u0107 pozwala naprawi\u0107 bardziej sp\u00f3\u017anione lub przyspieszone uderzenia.",
        " ms");

    setupRow(preserve,
        "OCHRONA ATAKU KOSTKI",
        "Zmiana timingu odbywa si\u0119 przed atakiem, aby sam pocz\u0105tek nuty pozosta\u0142 czysty.",
        " ms");

    setupRow(swing,
        "SWING",
        "Przesuwa co drugi krok siatki. Zostaw 0%, je\u015bli grasz prosty rytm.",
        " %");

    setupRow(dynamics,
        "WYR\u00d3WNANIE DYNAMIKI",
        "\u0141agodnie uspokaja zbyt g\u0142o\u015bne uderzenia po korekcji rytmu.",
        " %");

    stats.setColour(juce::Label::textColourId, juce::Colour(0xffc7ccd3));
    stats.setJustificationType(juce::Justification::centredLeft);
    stats.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(stats);

    footer.setText(
        "Kolejno\u015b\u0107 ustawiania: 1. wybierz siatk\u0119, 2. ustaw wykrywanie atak\u00f3w, "
        "3. zwi\u0119kszaj si\u0142\u0119 kwantyzacji, 4. dopiero potem zwi\u0119kszaj maksymalny b\u0142\u0105d.",
        juce::dontSendNotification);
    footer.setColour(juce::Label::textColourId, juce::Colour(0xffaeb4bc));
    footer.setJustificationType(juce::Justification::centredLeft);
    footer.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(footer);

    enabledA = std::make_unique<ButtonAttachment>(processor.apvts, Param::enabled, enabled);
    gridA = std::make_unique<ComboAttachment>(processor.apvts, Param::grid, grid);
    sensitivityA = std::make_unique<SliderAttachment>(processor.apvts, Param::sensitivity, sensitivity.slider);
    thresholdA = std::make_unique<SliderAttachment>(processor.apvts, Param::threshold, threshold.slider);
    strengthA = std::make_unique<SliderAttachment>(processor.apvts, Param::strength, strength.slider);
    windowA = std::make_unique<SliderAttachment>(processor.apvts, Param::window, window.slider);
    preserveA = std::make_unique<SliderAttachment>(processor.apvts, Param::preserve, preserve.slider);
    swingA = std::make_unique<SliderAttachment>(processor.apvts, Param::swing, swing.slider);
    dynamicsA = std::make_unique<SliderAttachment>(processor.apvts, Param::dynamics, dynamics.slider);

    startTimerHz(15);
}

void JerzyAudioQuantizerAudioProcessorEditor::setupRow(
    ParameterRow& row, const juce::String& title,
    const juce::String& help, const juce::String& suffix)
{
    row.title.setText(title, juce::dontSendNotification);
    row.title.setColour(juce::Label::textColourId, juce::Colours::white);
    row.title.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    addAndMakeVisible(row.title);

    row.help.setText(help, juce::dontSendNotification);
    row.help.setColour(juce::Label::textColourId, juce::Colour(0xffaeb4bc));
    row.help.setFont(juce::FontOptions(11.5f));
    row.help.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(row.help);

    row.slider.setSliderStyle(juce::Slider::LinearHorizontal);
    row.slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 92, 24);
    row.slider.setTextValueSuffix(suffix);
    row.slider.setColour(juce::Slider::trackColourId, juce::Colour(0xff7e8cff));
    row.slider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff333840));
    row.slider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    row.slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    row.slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff20242a));
    row.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff59616c));
    addAndMakeVisible(row.slider);
}

void JerzyAudioQuantizerAudioProcessorEditor::layoutRow(
    ParameterRow& row, juce::Rectangle<int> area)
{
    area.reduce(12, 8);
    row.title.setBounds(area.removeFromTop(22));
    row.help.setBounds(area.removeFromTop(38));
    row.slider.setBounds(area.removeFromTop(34));
}

void JerzyAudioQuantizerAudioProcessorEditor::drawStatus(
    juce::Graphics& g, juce::Rectangle<float> r,
    const juce::String& title, const juce::String& value, float amount) const
{
    amount = juce::jlimit(0.0f, 1.0f, amount);

    g.setColour(juce::Colour(0xff181c21));
    g.fillRoundedRectangle(r, 7.0f);
    g.setColour(juce::Colour(0xff454c56));
    g.drawRoundedRectangle(r, 7.0f, 1.0f);

    auto inner = r.reduced(10.0f);
    auto text = inner.removeFromTop(20.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.drawText(title, text, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xffcbd0d6));
    g.setFont(juce::FontOptions(11.5f));
    g.drawText(value, text, juce::Justification::centredRight);

    auto bar = inner.reduced(0.0f, 7.0f);
    g.setColour(juce::Colour(0xff30363d));
    g.fillRoundedRectangle(bar, 3.0f);

    auto fill = bar;
    fill.setWidth(bar.getWidth() * amount);
    g.setColour(juce::Colour(0xff7e8cff));
    g.fillRoundedRectangle(fill, 3.0f);
}

void JerzyAudioQuantizerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0e1115));

    auto outer = getLocalBounds().toFloat().reduced(10.0f);
    g.setColour(juce::Colour(0xff171b20));
    g.fillRoundedRectangle(outer, 12.0f);
    g.setColour(juce::Colour(0xff4d5560));
    g.drawRoundedRectangle(outer, 12.0f, 1.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    g.drawText("JERZY AUDIO QUANTIZER 2",
               28, 18, getWidth() - 56, 34,
               juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xffaeb4bc));
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("Korekcja rytmu nagranej gitary do tempa i siatki hosta",
               28, 50, getWidth() - 56, 22,
               juce::Justification::centredLeft);

    const float conf = processor.attackConfidence.load(std::memory_order_relaxed);
    const float corr = processor.lastCorrectionMs.load(std::memory_order_relaxed);
    const float risk = processor.artifactRisk.load(std::memory_order_relaxed);
    const float dyn = processor.dynamicsReductionDb.load(std::memory_order_relaxed);
    const float bpm = processor.currentBpm.load(std::memory_order_relaxed);

    auto meterArea = juce::Rectangle<float>(28.0f, 84.0f, getWidth() - 56.0f, 62.0f);
    constexpr float gap = 10.0f;
    const float w = (meterArea.getWidth() - 3.0f * gap) / 4.0f;

    drawStatus(g, meterArea.removeFromLeft(w),
               "WYKRYCIE ATAKU",
               juce::String(conf * 100.0f, 0) + " %",
               conf);
    meterArea.removeFromLeft(gap);

    drawStatus(g, meterArea.removeFromLeft(w),
               "KOREKTA CZASU",
               (corr >= 0.0f ? "+" : "") + juce::String(corr, 1) + " ms",
               juce::jlimit(0.0f, 1.0f, std::abs(corr) / 120.0f));
    meterArea.removeFromLeft(gap);

    drawStatus(g, meterArea.removeFromLeft(w),
               "RYZYKO ARTEFAKTU",
               risk < 0.33f ? "NISKIE" : (risk < 0.66f ? "\u015aREDNIE" : "WYSOKIE"),
               risk);
    meterArea.removeFromLeft(gap);

    drawStatus(g, meterArea,
               "HOST / DYNAMIKA",
               juce::String(bpm, 1) + " BPM  |  " + juce::String(dyn, 1) + " dB",
               juce::jlimit(0.0f, 1.0f, dyn / 9.0f));
}

void JerzyAudioQuantizerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(28);
    area.removeFromTop(126);

    auto top = area.removeFromTop(94);
    enabled.setBounds(top.removeFromLeft(250).reduced(0, 28));
    top.removeFromLeft(18);

    auto gridArea = top;
    gridTitle.setBounds(gridArea.removeFromTop(22));
    gridHelp.setBounds(gridArea.removeFromTop(34));
    grid.setBounds(gridArea.removeFromTop(34));

    area.removeFromTop(12);

    auto footerArea = area.removeFromBottom(68);
    stats.setBounds(footerArea.removeFromTop(26));
    footer.setBounds(footerArea);

    const int gap = 12;
    const int leftWidth = (area.getWidth() - gap) / 2;
    auto left = area.removeFromLeft(leftWidth);
    area.removeFromLeft(gap);
    auto right = area;

    const int rowGap = 8;
    const int leftRows = 4;
    const int rightRows = 3;
    const int leftH = (left.getHeight() - rowGap * (leftRows - 1)) / leftRows;
    const int rightH = (right.getHeight() - rowGap * (rightRows - 1)) / rightRows;

    auto takeRow = [rowGap](juce::Rectangle<int>& col, int h)
    {
        auto r = col.removeFromTop(h);
        col.removeFromTop(rowGap);
        return r;
    };

    layoutRow(sensitivity, takeRow(left, leftH));
    layoutRow(threshold, takeRow(left, leftH));
    layoutRow(strength, takeRow(left, leftH));
    layoutRow(window, takeRow(left, leftH));

    layoutRow(preserve, takeRow(right, rightH));
    layoutRow(swing, takeRow(right, rightH));
    layoutRow(dynamics, takeRow(right, rightH));
}

void JerzyAudioQuantizerAudioProcessorEditor::timerCallback()
{
    const int detected = processor.detectedAttacks.load(std::memory_order_relaxed);
    const int accepted = processor.acceptedAttacks.load(std::memory_order_relaxed);
    const int rejected = processor.rejectedAttacks.load(std::memory_order_relaxed);

    stats.setText(
        "Ataki wykryte: " + juce::String(detected)
        + "    |    skorygowane: " + juce::String(accepted)
        + "    |    odrzucone: " + juce::String(rejected),
        juce::dontSendNotification);

    repaint();
}
