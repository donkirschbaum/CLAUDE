#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DroneEngine.h"

// Zufalls- und Sonderaktionen. Jede lässt sich per Klick oder per MIDI-CC auslösen.
enum Action
{
    actAll = 0, actMutate, actChaos, actTune,
    actMaster, actLfo, actDelay, actQuadA, actQuadB,
    actPair0, actPair1, actPair2, actPair3,
    actReset,
    numActions
};

class DroneAchtProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    DroneAchtProcessor();
    ~DroneAchtProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Drone Acht"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // MIDI-Noten: weiße Tasten ab C3 spielen die Stimmen 1–8
    static constexpr int midiNotes[drone::numVoices] { 60, 62, 64, 65, 67, 69, 71, 72 };

    juce::AudioProcessorValueTreeState apvts;
    drone::Engine engine;

    std::array<std::atomic<bool>, drone::numVoices> padTouch, keyTouch;

    // Oszilloskop-Ringpuffer (wird von der Oberfläche gelesen)
    static constexpr int scopeSize = 8192;
    std::array<float, scopeSize> scopeBuf {};
    std::atomic<int> scopeWrite { 0 };

    // ---- Aktionen ----
    void performAction (int action);                 // nur Message-Thread
    static juce::String actionName (int action);

    // ---- MIDI-Zuordnung ----
    // Ein Ziel ist ein Parameter (0 … numParams-1) oder eine Aktion (actionBase + a).
    static constexpr int actionBase = 1000;
    int  numParams() const                         { return (int) params.size(); }
    int  targetForParam (const juce::String& id) const;
    juce::String targetName (int target) const;
    int  ccForTarget (int target) const;           // -1 wenn keine Zuordnung
    void startLearn (int target)                   { learnTarget.store (target); bumpMap(); }
    void cancelLearn()                             { learnTarget.store (-1); bumpMap(); }
    int  getLearnTarget() const                    { return learnTarget.load(); }
    void clearTarget (int target);
    void loadDefaultMidiMap();
    void clearMidiMap();
    int  getMapVersion() const                     { return mapVersion.load(); }
    static const std::vector<int>& defaultCCs();

private:
    void timerCallback() override;
    void readParams (drone::Params& p) const;
    void setParam (const juce::String& id, float value);
    void setNorm (juce::RangedAudioParameter*, float norm);
    void handleCC (int cc, int value);
    void bumpMap() { mapVersion.fetch_add (1); }

    void randPair (int pair, bool withTune);
    void randQuad (int quad);
    void randTune();
    void randMaster();
    void randLfo();
    void randDelay();
    void mutate();
    void chaos();

    juce::String targetKey (int target) const;
    int targetFromKey (const juce::String& key) const;

    struct Raw
    {
        std::atomic<float> *vol, *drive, *vib, *vibRate, *lfoA, *lfoB, *lfoMode, *lfoWave,
                           *t1, *t2, *fb, *dmod, *dmix;
        std::atomic<float> *hold[2], *pitch[2], *src[2], *mod[4], *sharp[4],
                           *tune[drone::numVoices], *fast[drone::numVoices];
    } raw {};

    std::vector<juce::RangedAudioParameter*> params;
    std::array<std::atomic<int>, 128> ccMap;       // CC -> Ziel, -1 = frei
    std::array<int, 128> lastCC {};
    std::atomic<int> learnTarget { -1 };
    std::atomic<int> mapVersion { 0 };
    std::atomic<uint32_t> pendingActions { 0 };

    std::array<int, drone::numVoices> midiCount {};
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DroneAchtProcessor)
};
