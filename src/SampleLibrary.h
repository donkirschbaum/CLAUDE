#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <memory>
#include <vector>

// Ein Sample im Speicher plus die Merkmale, die die Zufallsauswahl braucht.
struct SampleData
{
    juce::String key;      // "b:<datei>" für eingebaute, "u:<pfad>" für eigene Samples
    juce::String name;
    juce::String cat;      // Shaker, Cabasa, Maracas, Eigene
    juce::AudioBuffer<float> buf;
    double sr = 44100.0;
    float onset = 0.0f;    // Auto-Start in Sekunden
    float autoDb = 0.0f;   // Lautheitsangleich
    float zcr = 0.0f;      // Helligkeit (Nulldurchgänge)
    float dur = 0.1f;      // hörbare Länge in Sekunden
    float peak = 1.0f;
    int fam = 0;           // 0 akustisch, 1 lofi, 2 elektronisch
    bool custom = false;
    std::atomic<bool> hidden { false };
    juce::File file;

    double lengthSeconds() const { return buf.getNumSamples() / sr; }
};

class SampleLibrary
{
public:
    SampleLibrary();

    int size() const noexcept { return count.load(); }
    SampleData* get (int i) const noexcept { return (i >= 0 && i < size()) ? items[(size_t) i].get() : nullptr; }
    int findKey (const juce::String& key) const;
    int findName (const juce::String& name) const;

    // Lädt eine Audiodatei als eigenes Sample. Gibt den Index zurück oder -1.
    int loadFile (const juce::File& f);

    juce::AudioFormatManager& formats() { return formatManager; }

    static void analyse (SampleData& s);
    static int familyOf (const juce::String& name);

private:
    int add (std::unique_ptr<SampleData> s);

    static constexpr int capacity = 4096;   // fest reserviert: Audio-Thread liest ohne Lock
    std::vector<std::unique_ptr<SampleData>> items;
    std::atomic<int> count { 0 };
    juce::AudioFormatManager formatManager;
};
