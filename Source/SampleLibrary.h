#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <map>
#include <vector>

struct SampleData
{
    juce::String ref;      // "factory:drums" oder "user:Dateiname.wav"
    juce::String name;
    juce::AudioBuffer<float> buffer;   // mono
    double sampleRate = 44100.0;
};
using SamplePtr = std::shared_ptr<const SampleData>;

struct PresetData
{
    juce::String category, name, sampleRef;
    std::map<juce::String, float> values;   // Parameter-ID -> echter Wert
    bool isUser = false;
    juce::File file;
};

// Werks-Samples (eingebaut) + eigene Samples (Ordner ~/Music/MIKROKORN/Samples)
// Werks-Presets (eingebaut) + eigene Presets (Ordner ~/Music/MIKROKORN/Presets)
class SampleLibrary
{
public:
    SampleLibrary();

    static juce::File rootDir();
    static juce::File samplesDir();
    static juce::File presetsDir();

    // Samples
    const std::vector<SamplePtr>& getFactorySamples() const { return factory; }
    juce::StringArray getUserSampleFiles() const;          // Dateinamen
    SamplePtr find (const juce::String& ref);               // nullptr wenn nicht vorhanden
    SamplePtr importFile (const juce::File& file, juce::String& error);
    bool deleteUserSample (const juce::String& ref);
    static juce::String displayName (const juce::String& ref);

    // Presets
    int getNumPresets() const;
    int getNumFactoryPresets() const { return numFactoryPresets; }
    PresetData getPreset (int index) const;
    int saveUserPreset (const juce::String& name, const juce::String& sampleRef, const std::map<juce::String, float>& values);
    bool deleteUserPreset (int index);
    void rescanUserPresets();
    int findUserPresetByFile (const juce::File& f) const;

    static bool isAudioFile (const juce::File& f);

private:
    SamplePtr loadFromReader (std::unique_ptr<juce::AudioFormatReader> reader, const juce::String& ref, const juce::String& name);

    juce::AudioFormatManager formats;
    std::vector<SamplePtr> factory;
    std::map<juce::String, SamplePtr> userCache;
    std::vector<PresetData> presets;
    int numFactoryPresets = 0;
    mutable juce::CriticalSection lock;
};
