#pragma once

#include "PluginProcessor.h"

//==============================================================================
class StepGrid : public juce::Component
{
public:
    explicit StepGrid (PolyRhythmProcessor& p) : proc (p) {}

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

    int rowHeight = 64;

private:
    bool cellAt (juce::Point<int> pos, int& track, int& step) const;
    juce::Rectangle<float> cellBounds (int track, int step) const;

    PolyRhythmProcessor& proc;
    bool paintValue = true;
    int lastTrack = -1, lastStep = -1;
};

//==============================================================================
class MidiDragArea : public juce::Component
{
public:
    explicit MidiDragArea (PolyRhythmProcessor& p) : proc (p) {}

    void paint (juce::Graphics& g) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }

    std::function<void (const juce::String&)> onInfo;

private:
    PolyRhythmProcessor& proc;
    bool dragging = false, hover = false;
};

//==============================================================================
struct TrackControls
{
    juce::Label name;
    juce::Slider note, length, vel, gate;
    juce::ComboBox rate;
    juce::TextButton mute { "M" }, rnd { "RND" }, clr { "CLR" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noteA, lenA, velA, gateA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rateA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteA;
};

//==============================================================================
class PolyRhythmEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit PolyRhythmEditor (PolyRhythmProcessor&);
    ~PolyRhythmEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void exportMidi();

    PolyRhythmProcessor& proc;
    juce::LookAndFeel_V4 laf;

    juce::Label title, portLabel, densityLabel, bpmLabel, syncLabel, channelLabel, infoLabel;
    juce::TextButton randomAll { "RANDOM ALL" }, playButton { "PLAY" }, exportButton { "MIDI exportieren..." };
    juce::ToggleButton randLen { "inkl. Laenge" }, virtOut { "Virt. MIDI-Out" };
    juce::Slider density, bpm, channel;
    juce::ComboBox exportLen;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> densityA, bpmA, channelA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> randLenA, playA, virtOutA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> exportLenA;

    std::array<TrackControls, PolyRhythmProcessor::numTracks> tracks;
    StepGrid grid;
    MidiDragArea dragArea;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PolyRhythmEditor)
};
