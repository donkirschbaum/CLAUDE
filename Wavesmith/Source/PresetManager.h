#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ws
{

struct Preset
{
    juce::String name, category;
    juce::var params;          // object: paramID -> real value
    juce::File file;           // only for user presets
};

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& state);

    int getNumPresets() const noexcept { return (int) presets.size(); }
    const Preset& getPreset (int index) const { return presets[(size_t) index]; }
    int getCurrentIndex() const noexcept { return currentIndex; }
    juce::StringArray getCategories() const;

    void loadPreset (int index);
    void setCurrentIndexSilently (int index) noexcept { currentIndex = (index >= 0 && index < (int) presets.size()) ? index : -1; }
    bool saveUserPreset (const juce::String& name);   // saves current state
    void rescanUserPresets();

    static juce::File getUserPresetFolder();

    // number of factory presets that failed to map parameters (for tests)
    juce::StringArray unknownParameterIds;

private:
    void loadFactoryPresets();
    void applyParams (const juce::var& params);

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<Preset> presets;
    int numFactory = 0;
    int currentIndex = -1;
};

} // namespace ws
