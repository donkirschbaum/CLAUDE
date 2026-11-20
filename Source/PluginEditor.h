#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class LoopTapeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LoopTapeLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end,
                           juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.0f)); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (14.0f, juce::Font::bold)); }
};

class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& title);
    void resized() override;
    juce::Slider slider;

private:
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class TapeDisplay : public juce::Component
{
public:
    explicit TapeDisplay (LoopTapeProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void tick();

private:
    LoopTapeProcessor& proc;
    float reelAngle = 0.0f;
    int blink = 0;
};

// Drag the loop straight into an Ableton track (renders a WAV first)
class DragTile : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit DragTile (LoopTapeProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragStarted = false; }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    std::function<void (const juce::String&)> onStatus;

private:
    LoopTapeProcessor& proc;
    bool dragStarted = false;
};

class LoopTapeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LoopTapeEditor (LoopTapeProcessor&);
    ~LoopTapeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    LoopTapeProcessor& proc;
    LoopTapeLookAndFeel lnf;

    Knob time, feedback, mix, tone, lowCut, wow, flutter, saturation, hiss, loopLevel;

    juce::ToggleButton hostBtn { "HOST" }, killDryBtn { "KILL DRY" }, reverseBtn { "REVERSE" }, halfBtn { "1/2 SPEED" };
    juce::ComboBox modeBox, routingBox, barsBox;
    juce::Label modeLbl, routingLbl, barsLbl;

    std::unique_ptr<ButtonAttachment> hostAtt, killDryAtt, reverseAtt, halfAtt;
    std::unique_ptr<ComboAttachment> modeAtt, routingAtt, barsAtt;

    juce::TextButton recBtn { "REC" }, playBtn { "PLAY" }, undoBtn { "UNDO" }, redoBtn { "REDO" }, clearBtn { "CLEAR" };

    TapeDisplay display;

    juce::Label hostInfo, exportStatus;
    juce::TextButton exportBtn { "EXPORT WAV" };
    DragTile dragTile;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltips { this, 600 };
    int statusTicks = 0;

    void showExportMenu();
    void setStatus (const juce::String& text);

    juce::Rectangle<int> echoPanel, loopPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopTapeEditor)
};
