// Shruthi XT editor: hosts the HTML panel in a WebView. GPL v3.0.
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class ShruthiEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ShruthiEditor (ShruthiProcessor&);
    ~ShruthiEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0c0c0d)); }

private:
    void timerCallback() override;
    void onUIEvent (const juce::var& m);
    void emit (const juce::var& m);
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    ShruthiProcessor& proc;
    std::unique_ptr<juce::WebBrowserComponent> web;
    std::unique_ptr<juce::FileChooser> chooser;
    int lastParams[shruthi::kNumParams] {};
    int lastStep = -2, lastVersion = -1; bool lastJam = false, ready = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShruthiEditor)
};
