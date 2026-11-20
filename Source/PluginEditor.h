#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

namespace ui
{
namespace col
{
    const juce::Colour panel  { 0xff171d1b };
    const juce::Colour plate  { 0xff212926 };
    const juce::Colour plate2 { 0xff29332f };
    const juce::Colour line   { 0xff3a4742 };
    const juce::Colour scope  { 0xff0f1312 };
    const juce::Colour ink    { 0xffe8e4d8 };
    const juce::Colour muted  { 0xff8c9892 };
    const juce::Colour copper { 0xffcf7f48 };
    const juce::Colour glow   { 0xfff6b874 };
    const juce::Colour padHi  { 0xff4d5a55 };
}

juce::Font mono (float height, bool bold = false);
juce::Font sans (float height, bool bold = false);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
};

// Verbindung eines Bedienelements zu seinem MIDI-Ziel (Parameter oder Aktion)
struct MidiLink
{
    DroneAchtProcessor& proc;
    int target;

    bool learning() const        { return target >= 0 && proc.getLearnTarget() == target; }
    juce::String ccText() const;
    void showMenu (juce::Component& owner) const;
};

void drawLearnFrame (juce::Graphics&, juce::Rectangle<float>);

// Drehknopf mit Name, Wert und MIDI-Learn per Rechtsklick
class Knob : public juce::Component
{
public:
    using Formatter = std::function<juce::String (float)>;
    Knob (DroneAchtProcessor&, const juce::String& paramID, const juce::String& name,
          Formatter fmt, bool small = false, bool bipolar = false);
    void resized() override;
    void paint (juce::Graphics&) override;
    static int heightFor (bool small) { return small ? 63 : 77; }

private:
    struct Dial : public juce::Slider
    {
        std::function<void()> onMenu, onHover;
        bool menuClick = false;
        void mouseDown (const juce::MouseEvent& e) override
        {
            menuClick = e.mods.isPopupMenu();
            if (menuClick) { if (onMenu) onMenu(); return; }
            Slider::mouseDown (e);
        }
        void mouseDrag (const juce::MouseEvent& e) override { if (! menuClick) Slider::mouseDrag (e); }
        void mouseUp (const juce::MouseEvent& e) override   { if (! menuClick) Slider::mouseUp (e); menuClick = false; }
        void mouseEnter (const juce::MouseEvent& e) override { Slider::mouseEnter (e); if (onHover) onHover(); }
        void mouseExit (const juce::MouseEvent& e) override  { Slider::mouseExit (e);  if (onHover) onHover(); }
    };

    Dial slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    MidiLink link;
    juce::String name;
    Formatter fmt;
    bool small;
};

// Segmentschalter für Auswahl-Parameter
class Segmented : public juce::Component
{
public:
    Segmented (DroneAchtProcessor&, const juce::String& paramID, juce::StringArray labels, const juce::String& title);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> segArea() const;
    MidiLink link;
    juce::StringArray labels;
    juce::String title;
    int index = 0;
    juce::ParameterAttachment attachment;
};

// Schnell/Langsam-Umschalter einer Stimme
class FastToggle : public juce::Component, public juce::SettableTooltipClient
{
public:
    FastToggle (DroneAchtProcessor&, const juce::String& paramID);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    MidiLink link;

private:
    bool on = true;
    juce::ParameterAttachment attachment;
};

// Knopf für eine Aktion: mit Text oder als Würfel
class ActionButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    ActionButton (DroneAchtProcessor&, int action, juce::String label = {});
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }
    void refreshTooltip();
    MidiLink link;

private:
    int action;
    juce::String label;
    bool down = false;
    int pips = 5;
};

// Menüknopf für die MIDI-Zuordnung insgesamt
class MidiMenuButton : public juce::Component
{
public:
    explicit MidiMenuButton (DroneAchtProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

private:
    DroneAchtProcessor& proc;
};

// Berührungsfläche einer Stimme
class Pad : public juce::Component
{
public:
    Pad (DroneAchtProcessor&, int voice, juce::String keyLabel);
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    static constexpr int margin = 12;

private:
    juce::Rectangle<float> circle() const;
    DroneAchtProcessor& proc;
    int voice;
    juce::String keyLabel, noteLabel;
    std::atomic<float>* hold;
};

class Scope : public juce::Component
{
public:
    explicit Scope (DroneAchtProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;

private:
    DroneAchtProcessor& proc;
    std::array<float, 2048> snap {};
};
} // namespace ui

class DroneAchtEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DroneAchtEditor (DroneAchtProcessor&);
    ~DroneAchtEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyStateChanged (bool isKeyDown) override;
    void focusLost (FocusChangeType) override;

private:
    void timerCallback() override;
    ui::Knob* addKnob (const juce::String& id, const juce::String& name, ui::Knob::Formatter fmt,
                       bool small = false, bool bipolar = false);
    ui::Segmented* addSeg (const juce::String& id, juce::StringArray labels, const juce::String& title);
    ui::ActionButton* addAction (int action, juce::String label = {});

    struct PlateInfo { juce::Rectangle<int> r; juce::String left, right; bool inner; int action; };

    DroneAchtProcessor& proc;
    ui::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    std::map<juce::String, std::unique_ptr<ui::Knob>> knobs;
    std::map<juce::String, std::unique_ptr<ui::Segmented>> segs;
    std::map<int, std::unique_ptr<ui::ActionButton>> actions;
    std::vector<std::unique_ptr<ui::Pad>> pads;
    std::vector<std::unique_ptr<ui::FastToggle>> toggles;
    std::unique_ptr<ui::Scope> scope;
    ui::MidiMenuButton midiBtn { proc };
    std::vector<PlateInfo> plates;
    int lastMapVersion = -1;

    static constexpr char keyChars[drone::numVoices] { 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K' };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DroneAchtEditor)
};
