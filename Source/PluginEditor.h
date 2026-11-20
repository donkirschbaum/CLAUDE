#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace g8
{
namespace col
{
    const juce::Colour panel { 0xffd7dadd }, panel2 { 0xffc8ccd0 }, ink { 0xff17191c }, ink2 { 0xff565c63 },
        led { 0xffff4a1c }, ledPale { 0xffffd2c2 }, well { 0xff24272b }, wellInk { 0xffe9ecef }, ok { 0xff2f9e5a },
        solo { 0xffe8b51a }, btn { 0xffeef0f2 }, rec { 0xffe01d2e };
}
juce::Font fnt (float h, bool bold = false);

class LnF : public juce::LookAndFeel_V4
{
public:
    LnF();
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return fnt (12.f); }
    juce::Font getPopupMenuFont() override { return fnt (13.f); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
};

using APVTS = juce::AudioProcessorValueTreeState;

class XYPad : public juce::Component
{
public:
    XYPad (Grids8Processor& p, const juce::String& xId, const juce::String& yId);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    float effX = .5f, effY = .5f; bool hot = false;
    std::function<void()> onUserMove;
private:
    void setFrom (juce::Point<float>);
    Grids8Processor& proc;
    juce::RangedAudioParameter *px, *py;
};

class StepView : public juce::Component
{
public:
    StepView (Grids8Processor& p, int t) : proc (p), track (t) {}
    void paint (juce::Graphics&) override;
    int64_t step = -1;
private:
    Grids8Processor& proc; int track;
};

class LabeledSlider : public juce::Component
{
public:
    LabeledSlider (APVTS& s, const juce::String& id, const juce::String& label, std::function<juce::String (double)> fmt);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider slider;
private:
    juce::String label;
    std::function<juce::String (double)> fmt;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

class TrackRow : public juce::Component
{
public:
    TrackRow (Grids8Processor& p, int t);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick (int64_t g);
private:
    Grids8Processor& proc; int track;
    juce::TextButton fileBtn, mute { "M" }, solo { "S" }, dice { "?" };
    XYPad pad;
    LabeledSlider dens, chaos, len, vol, pitch, decay, mdx, mdy, mdd;
    StepView steps;
    juce::ComboBox shape, rate;
    std::unique_ptr<APVTS::ButtonAttachment> aMute, aSolo;
    std::unique_ptr<APVTS::ComboBoxAttachment> aShape, aRate;
    std::unique_ptr<juce::FileChooser> chooser;
    uint32_t lastHits = 0; float flash = 0; int modX = 0;
};

class Section : public juce::Component
{
public:
    Section (const juce::String& t) : title (t)
    {
        addAndMakeVisible (toggle);
        toggle.setButtonText ("-");
        toggle.onClick = [this] { setCollapsed (! collapsed); if (onLayout) onLayout(); };
    }
    void setCollapsed (bool c)
    {
        collapsed = c; toggle.setButtonText (c ? "+" : "-");
        for (auto* ch : getChildren()) if (ch != &toggle) ch->setVisible (! c);
    }
    int preferredHeight() const { return collapsed ? 34 : 34 + contentHeight + 8; }
    void paint (juce::Graphics&) override;
    void resized() override;
    virtual void layoutContent (juce::Rectangle<int>) = 0;
    bool collapsed = false;
    int contentHeight = 100;
    std::function<void()> onLayout;
protected:
    juce::String title;
    juce::TextButton toggle;
};

class MasterSection : public Section
{
public:
    MasterSection (Grids8Processor& p);
    void layoutContent (juce::Rectangle<int>) override;
    XYPad pad;
private:
    LabeledSlider dens, chaos, vol, dx, dy, dd;
    juce::ComboBox shape, rate;
    std::unique_ptr<APVTS::ComboBoxAttachment> aShape, aRate;
};

class RandomSection : public Section
{
public:
    RandomSection (Grids8Processor& p);
    void layoutContent (juce::Rectangle<int>) override;
    void paint (juce::Graphics&) override;
private:
    Grids8Processor& proc;
    juce::OwnedArray<juce::TextButton> buttons;
    LabeledSlider amt, hTime, hVel;
    juce::ToggleButton evOn { "Evolve alle" }, afOn { "Auto-Fill alle" };
    juce::ComboBox evEvery, afEvery;
    std::unique_ptr<APVTS::ButtonAttachment> aEv, aAf;
    std::unique_ptr<APVTS::ComboBoxAttachment> aEvE, aAfE;
};

class OptionsSection : public Section
{
public:
    OptionsSection (Grids8Processor& p);
    void layoutContent (juce::Rectangle<int>) override;
    void paint (juce::Graphics&) override;
private:
    juce::ComboBox noteMap, midiCh;
    juce::ToggleButton choke { "Hat-Choke (Spur 3/4)" }, sound { "Interne Sounds" };
    std::unique_ptr<APVTS::ComboBoxAttachment> aNote, aCh;
    std::unique_ptr<APVTS::ButtonAttachment> aChoke, aSound;
    juce::Rectangle<int> infoArea;
};

class LeftPanel : public juce::Component
{
public:
    LeftPanel (Grids8Processor& p);
    void resized() override;
    void relayout();
    MasterSection master; RandomSection random; OptionsSection options;
};

class DragChip : public juce::Component
{
public:
    DragChip (Grids8Processor& p, int slotIndex, bool multiTrack, const juce::String& t) : proc (p), slot (slotIndex), multi (multiTrack), text (t)
    { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
    bool enabledChip = true;
private:
    Grids8Processor& proc; int slot; bool multi; juce::String text; bool dragging = false;
};

class SlotView : public juce::Component
{
public:
    SlotView (Grids8Processor& p, int i);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
private:
    Grids8Processor& proc; int idx;
    juce::ComboBox lenBox;
    juce::TextButton hold { "Halten" }, loop { "Loop" }, clear { "Leeren" }, saveBtn { "Sichern..." };
    DragChip chipRack, chipMulti;
    juce::String meta;
    std::unique_ptr<juce::FileChooser> chooser;
};

class Content : public juce::Component
{
public:
    Content (Grids8Processor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
    static constexpr int W = 1480, H = 1020;
private:
    Grids8Processor& proc;
    juce::TextButton play { "Play" }, fill { "Fill" }, holdAll { "Alle halten" };
    LabeledSlider bpm, swing;
    juce::Viewport leftView;
    LeftPanel left;
    juce::OwnedArray<TrackRow> rows;
    juce::OwnedArray<SlotView> slotViews;
    juce::String capText;
    int lastVersion = -1;
};

class Grids8Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Grids8Editor (Grids8Processor&);
    ~Grids8Editor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (col::panel); }
private:
    void timerCallback() override { content.tick(); }
    LnF lnf;
    Content content;
};
} // namespace g8
