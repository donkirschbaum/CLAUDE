#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"
#include "Generator.h"

class ShakerProcessor : public juce::AudioProcessor
{
public:
    ShakerProcessor();
    ~ShakerProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Shaker Lab"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- Aktionen aus der Oberfläche (Message-Thread) ---
    void randomAll (bool firstRun);
    void patternsOnly();
    void samplesOnly();
    void mutateAll();
    void genTrack (int t);
    void setSample (int t, int libIndex);
    void setSteps (int t, int n);
    void resetSound (int t);
    int loadCustomFiles (const juce::Array<juce::File>& files);
    void removeCustom (int libIndex);
    void setRegion (int t, float start, float end);

    void setParam (const juce::String& id, float value);
    float getParam (const juce::String& id) const;
    static juce::String tid (int t, const char* name) { return "t" + juce::String (t) + "_" + name; }

    juce::AudioProcessorValueTreeState apvts;
    SampleLibrary lib;
    shaker::Engine engine { lib };

    std::atomic<bool> internalPlay { false };
    std::atomic<bool> hostPlaying { false };
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<int> uiVersion { 0 };     // erhöht sich, wenn Samples/Pattern von außen geändert wurden
    std::atomic<int> libVersion { 0 };    // erhöht sich, wenn die Sample-Liste sich ändert

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void cacheParams();
    void applyRoleMix (int t);

    struct TrackPtrs
    {
        std::atomic<float>* v[40] {};
    } tptr[shaker::kTracks];
    std::atomic<float>* gptr[16] {};

    shaker::TrackParams tp[shaker::kTracks];
    shaker::GlobalParams gp;
    double internalPpq = 0;
    double sr = 44100.0;
    juce::Random rng;
};
