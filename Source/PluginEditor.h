#pragma once
#include "PluginProcessor.h"

class MikrokornContent;
class MikrokornLookAndFeel;

class MikrokornEditor : public juce::AudioProcessorEditor
{
public:
    static constexpr int baseWidth = 1040, baseHeight = 768;

    explicit MikrokornEditor (MikrokornProcessor&);
    ~MikrokornEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MikrokornProcessor& proc;
    std::unique_ptr<MikrokornLookAndFeel> lnf;
    std::unique_ptr<MikrokornContent> content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MikrokornEditor)
};
