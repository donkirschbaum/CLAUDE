#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace pf
{
    struct Colours
    {
        static inline const juce::Colour bg      { 0xff14161c };
        static inline const juce::Colour panel   { 0xff1e212b };
        static inline const juce::Colour outline { 0xff2e3342 };
        static inline const juce::Colour text    { 0xffd8dbe6 };
        static inline const juce::Colour dim     { 0xff7d8396 };
        static inline const juce::Colour gate    { 0xffff5e7a };
        static inline const juce::Colour filter  { 0xff4fd1c5 };
        static inline const juce::Colour pan     { 0xfff6c344 };
        static inline const juce::Colour fx      { 0xff9b8cff };
    };

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider&) override;
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    };

    // A 16-step bar editor bound to 16 parameters.
    class StepLane : public juce::Component
    {
    public:
        StepLane (PulseForgeProcessor&, Lane, juce::Colour);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void refresh();

    private:
        int stepAt (float x) const;
        void setStep (int step, float value);
        float valueAt (float y) const;

        PulseForgeProcessor& proc;
        Lane lane;
        juce::Colour colour;
        std::array<juce::RangedAudioParameter*, kNumSteps> params {};
        std::array<float, kNumSteps> shown {};
        int playStep = -1, numSteps = kNumSteps;
        float paintValue = 1.0f;
        std::array<bool, kNumSteps> gestureOpen {};
    };

    // Rotary knob with label
    class Knob : public juce::Component
    {
    public:
        Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label, juce::Colour);
        void resized() override;
        juce::Slider slider;
    private:
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
}

class PulseForgeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PulseForgeEditor (PulseForgeProcessor&);
    ~PulseForgeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Section
    {
        juce::String title;
        juce::Colour colour;
        juce::Rectangle<int> bounds;
    };

    PulseForgeProcessor& proc;
    pf::LookAndFeel lnf;

    // Header
    juce::ComboBox presetBox, rateBox;
    std::unique_ptr<CA> rateAtt;
    std::unique_ptr<pf::Knob> lengthKnob, swingKnob;

    // Lanes
    std::array<std::unique_ptr<pf::StepLane>, 3> lanes;
    std::array<juce::ToggleButton, 3> laneOn;
    std::array<std::unique_ptr<BA>, 3> laneOnAtt;
    std::array<juce::TextButton, 3> randomBtn;

    // Gate
    std::unique_ptr<pf::Knob> gateDepth, gateLen, gateSmooth;
    // Filter
    juce::ComboBox filtType;
    std::unique_ptr<CA> filtTypeAtt;
    std::unique_ptr<pf::Knob> cutoff, reso, filtDepth, filtGlide;
    // Pan
    std::unique_ptr<pf::Knob> panDepth;
    // Character
    std::unique_ptr<pf::Knob> drive, bits, downsample;
    // Delay
    juce::ToggleButton delayOn { "On" }, delayPing { "Ping-Pong" };
    std::unique_ptr<BA> delayOnAtt, delayPingAtt;
    juce::ComboBox delayTime;
    std::unique_ptr<CA> delayTimeAtt;
    std::unique_ptr<pf::Knob> delayFb, delayMix;
    // Reverb
    juce::ToggleButton revOn { "On" };
    std::unique_ptr<BA> revOnAtt;
    std::unique_ptr<pf::Knob> revSize, revDamp, revMix;
    // Master
    std::unique_ptr<pf::Knob> mix, out;

    std::vector<Section> sections;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PulseForgeEditor)
};
