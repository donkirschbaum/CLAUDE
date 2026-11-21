#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace ws::ui
{

namespace colours
{
    const juce::Colour background { 0xff121418 };
    const juce::Colour panel      { 0xff1b1f26 };
    const juce::Colour panelEdge  { 0xff2a303a };
    const juce::Colour text       { 0xffd8dde6 };
    const juce::Colour dimText    { 0xff7d8796 };
    const juce::Colour accent     { 0xffff9f43 };   // warm amber
    const juce::Colour accent2    { 0xff4fc3f7 };   // cool blue
    const juce::Colour track      { 0xff313844 };
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.0f)); }
    juce::Font getLabelFont (juce::Label&) override { return juce::Font (juce::FontOptions (11.5f)); }
};

// --- small widgets ------------------------------------------------------------
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label, juce::Colour c = colours::accent);
    void resized() override;
    juce::Slider slider;
private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class Choice : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label);
    void resized() override;
    juce::ComboBox box;
private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

class Toggle : public juce::Component
{
public:
    Toggle (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label);
    void resized() override;
    juce::ToggleButton button;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

class WaveDisplay : public juce::Component, private juce::Timer
{
public:
    WaveDisplay (juce::AudioProcessorValueTreeState& s, const juce::String& oscPrefix);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    std::atomic<float>* table; std::atomic<float>* pos; std::atomic<float>* on;
    float lastTable = -1, lastPos = -1, lastOn = -1;
};

class StepSeqView : public juce::Component, private juce::Timer
{
public:
    StepSeqView (WavesmithProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    WavesmithProcessor& proc;
    juce::OwnedArray<juce::Slider> steps;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> attachments;
    int lastStep = -1;
};

// --- layout containers --------------------------------------------------------
// A horizontal row of components with relative widths
class Row : public juce::Component
{
public:
    Row& add (juce::Component* c, float weight = 1.0f) { items.push_back ({ c, weight }); addAndMakeVisible (c); return *this; }
    void resized() override;
private:
    std::vector<std::pair<juce::Component*, float>> items;
};

// A titled panel holding rows stacked vertically
class Section : public juce::Component
{
public:
    explicit Section (const juce::String& t) : title (t) {}
    Row& addRow (float weight = 1.0f);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Component* headerWidget = nullptr;   // optional (e.g. on/off toggle)
    void setHeaderWidget (juce::Component* c) { headerWidget = c; addAndMakeVisible (c); }
private:
    juce::String title;
    juce::OwnedArray<Row> rows;
    std::vector<float> weights;
};

} // namespace ws::ui

//==============================================================================
class WavesmithEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit WavesmithEditor (WavesmithProcessor&);
    ~WavesmithEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseWidth = 1140, baseHeight = 800;

private:
    void timerCallback() override;
    void refreshPresetLists();
    void selectCategory (const juce::String& cat);
    void stepPreset (int delta);
    void showSavePrompt();

    template <typename T, typename... Args> T* make (Args&&... args)
    {
        auto* c = new T (std::forward<Args> (args)...);
        owned.add (c);
        return c;
    }
    ws::ui::Knob* knob (const juce::String& id, const juce::String& label, juce::Colour c = ws::ui::colours::accent);
    ws::ui::Choice* choice (const juce::String& id, const juce::String& label);
    ws::ui::Toggle* toggle (const juce::String& id, const juce::String& label);
    ws::ui::Section* section (const juce::String& title);

    void buildOscSection (ws::ui::Section* s, const juce::String& prefix, bool withFm);

    WavesmithProcessor& proc;
    ws::ui::LookAndFeel lnf;

    juce::Component content;                  // fixed-size content, scaled to window
    juce::OwnedArray<juce::Component> owned;
    juce::OwnedArray<juce::Component> sections;

    // Top bar
    juce::ComboBox categoryBox, presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "Save" };
    std::vector<int> presetIndicesInCategory;
    int lastPresetCounter = -1;

    // Panels
    ws::ui::Section *oscA = nullptr, *oscB = nullptr, *subNoise = nullptr, *filter = nullptr;
    ws::ui::Section *ampEnv = nullptr, *modEnv = nullptr, *lfo1 = nullptr, *lfo2 = nullptr;
    ws::ui::Section *voiceSec = nullptr, *masterSec = nullptr;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    std::vector<std::unique_ptr<juce::ComponentListener>> splitListeners;
    std::unique_ptr<juce::Component> logo;

    juce::MidiKeyboardComponent keyboard;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WavesmithEditor)
};
