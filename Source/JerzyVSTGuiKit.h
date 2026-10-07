#pragma once
#include <JuceHeader.h>

namespace JerzyAudioUI {

struct Theme {
    juce::Colour chassisTop, chassisBottom;
    juce::Colour panelTop, panelBottom;
    juce::Colour accent, text, muted, edge, ledOn, display;
};

inline Theme petrol()   { return {0xff124b49,0xff071c1d,0xff102f2f,0xff071819,0xfff1a649,0xffefe2c4,0xff8aa29b,0xff708475,0xff44dc60,0xfff1a649}; }
inline Theme brass()    { return {0xff8c6425,0xff18130b,0xff493615,0xff1d160c,0xfff2bc5c,0xfff1e0be,0xffa78d64,0xff977640,0xff44dc60,0xfff2bc5c}; }
inline Theme steel()    { return {0xff394a58,0xff0d1419,0xff202c35,0xff0e171d,0xff73c8e8,0xffd8e5e9,0xff91a5b0,0xff758e9c,0xff53dc76,0xff73c8e8}; }
inline Theme burgundy() { return {0xff6e2521,0xff160c0a,0xff351815,0xff1c0d0c,0xffe69443,0xffecd7b9,0xffa87860,0xff8b503d,0xff55dc6e,0xffe69443}; }
inline Theme violet()   { return {0xff41386d,0xff100e1e,0xff24203f,0xff161328,0xffa994ed,0xffe5e0f1,0xff9a91b7,0xff695d94,0xff59de85,0xff68c6d8}; }

class HardwareLookAndFeel : public juce::LookAndFeel_V4 {
public:
    explicit HardwareLookAndFeel(Theme t):theme(std::move(t)){
        setColour(juce::Slider::textBoxTextColourId,theme.text);
        setColour(juce::Slider::textBoxBackgroundColourId,theme.panelBottom);
        setColour(juce::Slider::textBoxOutlineColourId,theme.edge);
        setColour(juce::ComboBox::backgroundColourId,theme.panelBottom);
        setColour(juce::ComboBox::textColourId,theme.text);
        setColour(juce::ComboBox::outlineColourId,theme.edge);
        setColour(juce::ComboBox::arrowColourId,theme.accent);
        setColour(juce::PopupMenu::backgroundColourId,theme.panelTop);
        setColour(juce::PopupMenu::textColourId,theme.text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId,theme.accent.withAlpha(.22f));
        setColour(juce::TextButton::buttonColourId,theme.panelTop);
        setColour(juce::TextButton::buttonOnColourId,theme.accent.withAlpha(.20f));
        setColour(juce::TextButton::textColourOffId,theme.text);
        setColour(juce::TextButton::textColourOnId,theme.accent);
        setColour(juce::ToggleButton::textColourId,theme.text);
        setColour(juce::ToggleButton::tickColourId,theme.ledOn);
        setColour(juce::Label::textColourId,theme.text);
    }
    const Theme& getTheme() const noexcept { return theme; }

    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float start,float end,juce::Slider&) override {
        auto bounds=juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h).reduced(6.0f);
        const float d=juce::jmin(bounds.getWidth(),bounds.getHeight());
        auto r=juce::Rectangle<float>(d,d).withCentre(bounds.getCentre()); auto c=r.getCentre();
        const float radius=d*.36f;

        for(int i=0;i<=20;++i){
            const float a=start+(end-start)*(float)i/20.0f;
            auto p=c+juce::Point<float>(std::sin(a),-std::cos(a))*radius*1.34f;
            const float rr=(i%5==0?1.8f:1.1f);
            g.setColour((i%5==0?theme.accent:theme.accent.withMultipliedAlpha(.68f)));
            g.fillEllipse(p.x-rr,p.y-rr,rr*2,rr*2);
        }

        g.setColour(juce::Colours::black.withAlpha(.55f)); g.fillEllipse(r.reduced(d*.10f).translated(2.5f,4.0f));
        auto outer=r.reduced(d*.13f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff30312d),outer.getTopLeft(),juce::Colour(0xff0e100f),outer.getBottomRight(),false));
        g.fillEllipse(outer); g.setColour(juce::Colour(0xff7d7058)); g.drawEllipse(outer,1.2f);

        auto cap=outer.reduced(d*.18f);
        juce::ColourGradient metal(juce::Colour(0xffded7c5),cap.getX(),cap.getY(),juce::Colour(0xff55544f),cap.getRight(),cap.getBottom(),false);
        metal.addColour(.47,juce::Colour(0xff9a988f));
        g.setGradientFill(metal); g.fillEllipse(cap);
        g.setColour(juce::Colour(0x55ffffff)); g.drawEllipse(cap.reduced(1),1.0f);

        const float a=start+pos*(end-start);
        juce::Path pointer; pointer.startNewSubPath(c); pointer.lineTo(c+juce::Point<float>(std::sin(a),-std::cos(a))*radius*.78f);
        g.setColour(juce::Colour(0xff2a2117)); g.strokePath(pointer,juce::PathStrokeType(4.0f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.setColour(juce::Colour(0xffffe8b7)); g.strokePath(pointer,juce::PathStrokeType(2.0f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    }

    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool over,bool down) override {
        auto r=b.getLocalBounds().toFloat().reduced(1);
        auto top=b.getToggleState()?theme.panelTop.brighter(.20f):theme.panelTop;
        auto bottom=b.getToggleState()?theme.panelBottom.brighter(.08f):theme.panelBottom;
        if(over){top=top.brighter(.08f);bottom=bottom.brighter(.05f);}
        if(down){top=top.darker(.15f);bottom=bottom.darker(.12f);}
        g.setGradientFill(juce::ColourGradient(top,r.getTopLeft(),bottom,r.getBottomLeft(),false));
        g.fillRoundedRectangle(r,5);
        g.setColour(b.getToggleState()?theme.accent:theme.edge);g.drawRoundedRectangle(r,5,b.getToggleState()?1.8f:1.0f);
    }

    void drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool,bool) override {
        auto r=b.getLocalBounds().toFloat().reduced(1);
        g.setColour(theme.panelBottom);g.fillRoundedRectangle(r,4);
        g.setColour(b.getToggleState()?theme.accent:theme.edge);g.drawRoundedRectangle(r,4,b.getToggleState()?1.8f:1.0f);
        if(b.getToggleState()){g.setColour(theme.ledOn);g.fillEllipse(8.0f,r.getCentreY()-3.0f,6.0f,6.0f);}
        g.setColour(theme.text);g.setFont(12.0f);g.drawText(b.getButtonText(),18,0,b.getWidth()-20,b.getHeight(),juce::Justification::centredLeft);
    }

    void drawComboBox(juce::Graphics& g,int w,int h,bool down,int bx,int by,int bw,int bh,juce::ComboBox&) override {
        auto r=juce::Rectangle<float>(0,0,(float)w,(float)h).reduced(1);
        g.setGradientFill(juce::ColourGradient(theme.panelTop,r.getTopLeft(),theme.panelBottom,r.getBottomLeft(),false));g.fillRoundedRectangle(r,4);
        g.setColour(theme.edge);g.drawRoundedRectangle(r,4,1);
        juce::Path a;a.startNewSubPath((float)bx+3,(float)by+6);a.lineTo((float)bx+bw*.5f,(float)by+bh-4);a.lineTo((float)bx+bw-3,(float)by+6);
        g.setColour(down?theme.accent.brighter(.2f):theme.accent);g.strokePath(a,juce::PathStrokeType(1.5f));
    }

private:
    Theme theme;
};

inline void paintChassis(juce::Graphics& g,juce::Rectangle<float> area,const Theme& t){
    g.setGradientFill(juce::ColourGradient(t.chassisTop,area.getTopLeft(),t.chassisBottom,area.getBottomRight(),false));
    g.fillRect(area);
    g.setColour(t.text.withAlpha(.05f));
    for(float y=area.getY()+4;y<area.getBottom();y+=11)g.drawLine(area.getX()+5,y,area.getRight()-5,y,.5f);
    g.setColour(t.text.withAlpha(.35f));g.drawRoundedRectangle(area.reduced(7),8,2);
    g.setColour(t.edge.withAlpha(.65f));g.drawRoundedRectangle(area.reduced(12),6,1);
}

inline void paintPanel(juce::Graphics& g,juce::Rectangle<float> r,const Theme& t,float radius=7.0f){
    g.setGradientFill(juce::ColourGradient(t.panelTop,r.getTopLeft(),t.panelBottom,r.getBottomRight(),false));
    g.fillRoundedRectangle(r,radius);
    g.setColour(t.edge);g.drawRoundedRectangle(r,radius,1.2f);
    g.setColour(t.text.withAlpha(.12f));g.drawRoundedRectangle(r.reduced(4),juce::jmax(2.0f,radius-2.0f),.8f);
    const juce::Point<float> ps[]={{r.getX()+10,r.getY()+10},{r.getRight()-10,r.getY()+10},{r.getX()+10,r.getBottom()-10},{r.getRight()-10,r.getBottom()-10}};
    for(auto p:ps){g.setColour(juce::Colour(0xff777264));g.fillEllipse(p.x-4,p.y-4,8,8);g.setColour(juce::Colour(0xffd0c5a6));g.drawEllipse(p.x-4,p.y-4,8,8,1);}
}

} // namespace JerzyAudioUI
