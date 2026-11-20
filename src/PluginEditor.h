#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace ui
{
const juce::Colour bg { 0xff13110f }, panel { 0xff1d1a17 }, panel2 { 0xff25211d }, line { 0xff332e28 },
                   text { 0xfff1e8da }, mute { 0xff8f8578 }, wave { 0xff0f0e0c };
const juce::Colour track[4] = { juce::Colour (0xfff2b84b), juce::Colour (0xffef7d5a), juce::Colour (0xff5fc7b0), juce::Colour (0xffb9d46c) };
inline juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
}

class ShakerLook : public juce::LookAndFeel_V4
{
public:
    ShakerLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return ui::font (juce::jmin (14.0f, h * 0.55f), true); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return ui::font (13.0f); }
    juce::Font getPopupMenuFont() override { return ui::font (14.0f); }
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
};

class Knob : public juce::Component
{
public:
    Knob (const juce::String& label, juce::Colour accent);
    void attach (juce::AudioProcessorValueTreeState& s, const juce::String& id);
    void resized() override;
    juce::Slider slider;
    juce::Label name;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class StepGrid : public juce::Component
{
public:
    StepGrid (ShakerProcessor& p, int track);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    int shownStep = -1;
    float widthFrac = 1.0f;
private:
    juce::Rectangle<float> gridArea() const;
    int idxAt (float x) const;
    float velAt (float y) const;
    ShakerProcessor& proc;
    int t;
    float lastVel = 0.8f;
    bool levelMode = false;
    juce::Point<float> downPos;
    int downIdx = 0;
    juce::String info;
    double infoUntil = 0;
};

class TrackRow : public juce::Component
{
public:
    TrackRow (ShakerProcessor& p, int track, std::function<void (int)> onSelect, std::function<void (int)> onEdit);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void refreshSamples();
    void refresh();
    void tick();
    bool selected = false;
    StepGrid grid;
private:
    ShakerProcessor& proc;
    int t;
    juce::ComboBox sampleBox, rateBox;
    juce::TextButton edit { "Edit" }, aud { "Play" }, muteB { "M" }, soloB { "S" }, dice { "Neu" }, minus { "-" }, plus { "+" }, pm { "PM" }, pr { "PR" };
    juce::Label stepsLbl, cycLbl, roleLbl;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAtt, soloAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rateAtt;
    std::function<void (int)> onSelect, onEdit;
    unsigned int lastHits = 0;
    double ledUntil = 0;
    int lastSteps = -1, lastMode = -1, lastRate = -1;
};

class DetailPanel : public juce::Component
{
public:
    explicit DetailPanel (ShakerProcessor& p);
    void setTrack (int t);
    void paint (juce::Graphics&) override;
    void resized() override;
    int track = 0;
private:
    void showTab (int i);
    ShakerProcessor& proc;
    juce::TextButton tabs[3] { juce::TextButton ("Sound"), juce::TextButton ("Mix"), juce::TextButton ("Zufall") };
    juce::TextButton resetB { "Regler zuruecksetzen" };
    int tab = 0;
    struct Item { std::unique_ptr<Knob> knob; const char* id; int tab; int group; };
    std::vector<Item> items;
};

class SampleEditorView : public juce::Component
{
public:
    explicit SampleEditorView (ShakerProcessor& p);
    void open (int track);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void refreshSamples();
    void tick();
    std::function<void()> onClose;
private:
    const SampleData* sample() const;
    void zoomAll();
    void moveMarker (float x);
    juce::Rectangle<float> waveArea() const;
    ShakerProcessor& proc;
    int t = 0;
    double v0 = 0, v1 = 1;
    int drag = 0;
    juce::ComboBox sampleBox;
    juce::TextButton load { "Eigene laden" }, del { "Loeschen" }, closeB { "Fertig" }, zSel { "Auswahl" }, zAll { "Ganz" }, zIn { "+" }, zOut { "-" },
                     autoB { "Auto-Start" }, resetB { "Reset" }, prev { "Vorhoeren" }, sL { "<" }, sR { ">" }, eL { "<" }, eR { ">" };
    juce::Label title, sVal, eVal, meta;
    std::unique_ptr<juce::FileChooser> chooser;
};

class ShakerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ShakerEditor (ShakerProcessor&);
    ~ShakerEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void select (int t);
    void refreshAll();
    ShakerProcessor& proc;
    ShakerLook look;
    juce::TextButton playB { "Play" }, randB { "Zufall" }, patB { "Pattern" }, smpB { "Samples" }, mutB { "Mutieren" };
    juce::ToggleButton polyDice { "Schritte, Modus & Tempo mitwuerfeln" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> polyAtt;
    std::vector<std::unique_ptr<Knob>> globals;
    juce::Label status, ratio;
    std::unique_ptr<TrackRow> rows[shaker::kTracks];
    DetailPanel detail;
    SampleEditorView sampleEd;
    int lastUi = -1, lastLib = -1;
};
