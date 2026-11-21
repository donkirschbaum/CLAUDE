// VOLTA·4 – Editor: zeigt die HTML-Oberfläche in einer WebView.
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class VoltaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VoltaEditor (VoltaProcessor&);
    ~VoltaEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0b0c0e)); }

private:
    void timerCallback() override;
    void onUIEvent (const juce::var& m);
    void emit (const juce::var& m);
    void sendFullState();
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    VoltaProcessor& proc;
    std::unique_ptr<juce::WebBrowserComponent> web;
    std::vector<float> last;
    int lastVersion = -1, lastMapVersion = -1, lastLearned = -1;
    bool ready = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoltaEditor)
};
