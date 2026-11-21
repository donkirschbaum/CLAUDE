// VOLTA·4 – Plugin-Prozessor: Parameter, Host-Sync, MIDI, Zustand.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "VoltaEngine.h"
#include "Params.h"
#include <atomic>

class VoltaProcessor : public juce::AudioProcessor
{
public:
    VoltaProcessor();
    ~VoltaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "VOLTA-4"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "VOLTA-4"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- Nachrichten-Thread (Editor) ---
    int numParams() const { return (int) params.size(); }
    juce::RangedAudioParameter* param (int i) const { return params[(size_t) i]; }
    int indexOf (const juce::String& id) const { return idIndex.contains (id) ? (int) idIndex[id] : -1; }
    float rawValue (int i) const { return raw[(size_t) i]->load(); }
    void setFromUI (int i, float naturalValue);
    void setCablesFromUI (const juce::var& list);
    juce::var getCablesVar() const { return cablesVar; }
    int presetIndex = 0;
    void padFromUI (int ch) { const auto sc = fifo.write (1); if (sc.blockSize1 > 0) pads[(size_t) sc.startIndex1] = ch; else if (sc.blockSize2 > 0) pads[(size_t) sc.startIndex2] = ch; }

    // MIDI-Learn
    void setLearn (const juce::String& id) { learnTarget.store (indexOf (id)); }
    void unlearn (const juce::String& id);
    void resetMap();
    juce::var mapAsVar() const;
    std::atomic<int> mapVersion { 0 };
    std::atomic<int> stateVersion { 0 };
    std::atomic<int> lastLearnedCC { -1 };

    // Anzeige
    uint32_t takeLit() { return engine.litMask.exchange (0); }
    int64_t currentStep() const { return engine.curStep.load(); }
    float cvShow (int i) const { return engine.cvShow[(size_t) i].load(); }
    std::atomic<bool> transportPlaying { false };
    std::atomic<double> hostBpm { 0.0 };

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void readParams (volta::Params& p) const;
    void handleMidi (const uint8_t* d, int len);
    void applyCC (int cc, int value);
    void parseCables();

    volta::Engine engine;
    std::vector<juce::RangedAudioParameter*> params;
    std::vector<std::atomic<float>*> raw;
    std::vector<volta::PT> types;
    juce::HashMap<juce::String, int> idIndex;
    int iBpm = 0, iPlay = 0, iHost = 0;

    // Kabel: Nachrichten-Thread -> Audio
    juce::var cablesVar;
    juce::SpinLock cableLock;
    std::vector<volta::Cable> pendingCables;
    bool cablesPending = true;

    // MIDI-Zuordnung (CC -> Parameterindex)
    std::array<std::atomic<int>, 128> ccParam;
    std::atomic<int> learnTarget { -1 };

    // Probeschläge aus der Oberfläche
    juce::AbstractFifo fifo { 256 };
    std::array<int, 256> pads {};

    // Transport
    double sr = 44100, internalPpq = 0, lastPpqEnd = 0;
    int64_t nextStep = 0;
    bool wasPlaying = false;
    struct Ev { int off; int type; int64_t step; uint8_t d[3]; int len; };
    std::vector<Ev> events;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoltaProcessor)
};
