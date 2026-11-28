#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class OctoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OctoLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

// Rotary knob with value + name underneath
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label, juce::Colour, int labelH = 28);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
    std::function<juce::String()> valueOverride; // non-empty result replaces the value text

private:
    juce::String name;
    int labelH;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// A row of exclusive buttons bound to a choice parameter
class ChoiceButtons : public juce::Component
{
public:
    ChoiceButtons (juce::RangedAudioParameter&, const juce::StringArray& labels, juce::Colour onColour);
    void resized() override;

private:
    juce::OwnedArray<juce::TextButton> buttons;
    juce::ParameterAttachment att;
};

class Scope : public juce::Component
{
public:
    explicit Scope (Octo8Processor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    juce::String status;

private:
    Octo8Processor& proc;
};

class Strip : public juce::Component
{
public:
    Strip (Octo8Processor&, int ch);
    void resized() override;
    void paint (juce::Graphics&) override;
    void tick();

private:
    Octo8Processor& proc;
    int ch;
    juce::Colour col;
    Knob wave, ntype, gain, tune, fine;
    juce::TextButton noiseBtn { "Noise" }, rnd { "Rnd" }, mute { "M" }, solo { "S" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aNoise, aMute, aSolo;
    int lastHits = 0;
    float flash = 0;
};

class SeqGrid : public juce::Component
{
public:
    explicit SeqGrid (Octo8Processor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> cell (int r, int c) const;
    Octo8Processor& proc;
    int dr = -1, dc = -1, startDeg = 0;
    bool dragging = false;
};

class FilterCard : public juce::Component
{
public:
    FilterCard (Octo8Processor&, int ch);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    int ch;
    juce::Colour col;
    ChoiceButtons type;
    Knob cut, res, a, d, s, r;
};

class Octo8Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Octo8Editor (Octo8Processor&);
    ~Octo8Editor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    static constexpr int W = 1200, H = 950;
    Octo8Processor& proc;
    OctoLookAndFeel lnf;

    struct Content : public juce::Component
    {
        void paint (juce::Graphics&) override;
        int soloMask = 0; // bit i = track i is soloed (channel or sequencer solo)
    } content;

    juce::TextButton playBtn { "Play" }, hostBtn { "Host" }, masterRnd { "Master Random" }, seqRnd { "Random Patterns" }, clearBtn { "Clear" };
    std::unique_ptr<ChoiceButtons> modeBtns;
    juce::ComboBox keyBox, scaleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> aKey, aScale;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aPlay, aHost;
    std::unique_ptr<Knob> master, bpm, gate;
    Scope scope;
    juce::OwnedArray<Strip> strips;
    SeqGrid grid;
    juce::OwnedArray<juce::TextButton> trackRnd, trackMute, trackSolo;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> trackAtt;
    juce::OwnedArray<FilterCard> cards;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Octo8Editor)
};
