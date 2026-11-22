#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
#include <vector>

class PolyRhythmProcessor : public juce::AudioProcessor,
                            private juce::Timer
{
public:
    static constexpr int numTracks = 6;
    static constexpr int maxSteps  = 32;
    static constexpr int ticksPerQuarter = 960;
    static constexpr int maxExportBars = 64;

    PolyRhythmProcessor();
    ~PolyRhythmProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PolyRhythm32"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Pattern access (thread safe, lock free)
    bool getStep (int track, int step) const;
    void setStep (int track, int step, bool on);
    uint32_t getPattern (int track) const { return patterns[(size_t) track].load(); }
    void setPattern (int track, uint32_t bits) { patterns[(size_t) track].store (bits); }

    void randomizeTrack (int track);
    void randomizeAll();
    void clearTrack (int track);

    // Undo / Redo (message thread only)
    void pushUndo();
    void undo();
    void redo();
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }

    int getCurrentStep (int track) const { return currentStep[(size_t) track].load(); }
    bool isHostSynced() const { return hostSynced.load(); }

    int getTrackLength (int track) const;
    double getTrackStepQuarters (int track) const;
    int getTrackNote (int track) const;

    // MIDI export
    juce::MidiFile createMidiLoop (int& totalBarsOut, bool& wasCapped) const;
    bool writeMidiLoop (const juce::File& file, juce::String& infoOut) const;

    static juce::StringArray getRateNames();
    static double rateIndexToQuarters (int index);

    juce::AudioProcessorValueTreeState apvts;

    // Virtual MIDI output port (macOS CoreMIDI): works independently of DAW routing
    juce::String getVirtualPortName() const;
    static bool virtualPortSupported();

    static juce::String id (const char* base, int track) { return juce::String (base) + juce::String (track); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void flushNoteOffs (juce::MidiBuffer& midi, int sampleOffset);
    void setParamValue (const juce::String& paramId, float plainValue);

    std::array<std::atomic<uint32_t>, numTracks> patterns;
    std::array<std::atomic<int>, numTracks> currentStep;

    struct Voice
    {
        int note = -1;
        int channel = 1;
        double offPpq = 0.0;
        double lastTrigPpq = -1.0e12;
    };
    std::array<Voice, numTracks> voices;

    std::atomic<bool> hostSynced { false };
    std::atomic<double> lastBpm { 120.0 };

    double sampleRate = 44100.0;
    double internalPpq = 0.0;
    bool wasPlaying = false;
    double lastPpqEnd = 0.0;

    static constexpr const char* snapshotParams[] = { "len", "rate", "note", "vel", "gate", "mute" };
    struct Snapshot
    {
        std::array<uint32_t, numTracks> patterns {};
        std::array<std::array<float, 6>, numTracks> values {};
    };
    Snapshot captureSnapshot() const;
    void restoreSnapshot (const Snapshot&);
    std::vector<Snapshot> undoStack, redoStack;

    void randomizeTrackInternal (int track, std::vector<int>& usedNotes);
    int pickRandomRate();

    void timerCallback() override;
    void generateBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&);
    void updateVirtualPort();

    std::unique_ptr<juce::MidiOutput> virtualOut;
    juce::SpinLock virtualOutLock;
    juce::String virtualPortName;
    int instanceNumber = 0;

    juce::Random random;
    juce::CriticalSection randomLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PolyRhythmProcessor)
};
