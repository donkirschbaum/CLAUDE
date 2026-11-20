#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

#define U8(x) juce::String (juce::CharPointer_UTF8 (x))

namespace col
{
inline const juce::Colour bg { 0xff15130e }, panel { 0xff1f1c16 }, panel2 { 0xff29251d }, line { 0xff3a3428 },
                          ink { 0xfff3ecdf }, dim { 0xffa19884 }, dark { 0xff1a160f };
inline const juce::Colour lane[5] = { juce::Colour (0xffff6b4a), juce::Colour (0xffffc23d), juce::Colour (0xff5ce1e6),
                                      juce::Colour (0xffb388ff), juce::Colour (0xff8be36b) };
inline juce::Colour cls (int c)
{
    static const juce::Colour m[4] = { lane[0], lane[1], lane[2], lane[4] };
    return m[juce::jlimit (0, 3, c)];
}
}

class FsLook : public juce::LookAndFeel_V4
{
public:
    FsLook();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minP, float maxP,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

class PlayButton : public juce::Button
{
public:
    PlayButton() : juce::Button ("play") {}
    bool on = false;
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

class Content;

class OverviewView : public juce::Component
{
public:
    OverviewView (FundstueckProcessor& p, Content& c) : proc (p), content (c) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh();
private:
    void drawStatus (juce::Graphics&);
    FundstueckProcessor& proc; Content& content;
    std::vector<std::pair<float, float>> peaks;
};

class ChipsView : public juce::Component
{
public:
    ChipsView (FundstueckProcessor& p, Content& c) : proc (p), content (c) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    juce::Rectangle<float> chipRect (int i) const;
    FundstueckProcessor& proc; Content& content;
};

class LaneRow : public juce::Component
{
public:
    LaneRow (FundstueckProcessor&, Content&, int lane);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    juce::Rectangle<float> stepRect (int s) const;
    int stepAt (juce::Point<float>) const;
    FundstueckProcessor& proc; Content& content; const int lane;
    juce::Slider vol, tune, len;
    juce::TextButton mute { "M" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aVol, aTune, aLen;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aMute;
    juce::Rectangle<int> nameR, thumbR, upR, downR, stepsR;
};

class Content : public juce::Component, private juce::Timer
{
public:
    explicit Content (FundstueckProcessor&);
    ~Content() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void chooseSlice (int i);
    void openFileChooser();
    int armed = -1;
    static constexpr int W = 1100, H = 700;
private:
    void timerCallback() override;
    void syncControls();
    void bigSlider (juce::Slider&, const char* paramId, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>&);
    FundstueckProcessor& proc;
    FsLook look;
    PlayButton play;
    juce::Slider bpm, swing, feel, rev, tape, sub, master, sens, dens;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aBpm, aSwing, aFeel, aRev, aTape, aSub, aMaster;
    juce::TextButton demoBtn, loadBtn, recBtn, syncBtn, rollAll, rollDrums, rollMel, rollSounds, clearBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aSync;
    juce::Rectangle<int> meterR { 690, 82, 50, 20 };
    float shownLevel = 0;
    double shownHostBpm = -1;
    bool seenSync = false, seenRec = false;
    int seenRecTenths = -1;
    juce::OwnedArray<juce::TextButton> styleBtns;
    juce::ComboBox rootBox, scaleBox;
    OverviewView overview;
    ChipsView chips;
    juce::OwnedArray<LaneRow> lanes;
    std::unique_ptr<juce::FileChooser> chooser;
    int seenVersion = -1, seenStep = -2;
    bool seenHost = false;
    juce::uint32 sensChangedAt = 0;
    bool sensDirty = false;
};

class FundstueckEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget
{
public:
    explicit FundstueckEditor (FundstueckProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (col::bg); }
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override;
private:
    FundstueckProcessor& proc;
    Content content;
};
