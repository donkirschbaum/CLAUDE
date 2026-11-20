#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "GrainEngine.h"
#include "SampleLibrary.h"

class MikrokornProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener,
                           private juce::Timer
{
public:
    static constexpr int numSlots = 6;
    static const juce::StringArray& presetParamIds();   // Parameter, die zu einem Preset/Slot gehören

    MikrokornProcessor();
    ~MikrokornProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MIKROKORN"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- Bedienung (Message-Thread) ----
    void selectSlot (int index);
    int  getCurrentSlot() const { return currentSlot; }
    juce::String getSlotName (int index) const { return slots[(size_t) index].name; }

    void loadPreset (int index);
    void stepPreset (int delta);
    int  getCurrentPreset() const { return slots[(size_t) currentSlot].preset; }
    bool isCurrentEdited() const { return currentEdited.load(); }
    int  saveCurrentAsPreset (const juce::String& name);
    bool deleteCurrentPreset();

    void useSample (const juce::String& ref);
    bool importSamples (const juce::Array<juce::File>& files, juce::String& error);
    bool deleteUserSample (const juce::String& ref);
    SamplePtr getCurrentSample() const;
    juce::String getCurrentSampleRef() const { return slots[(size_t) currentSlot].sampleRef; }

    void randomize();
    void initSlot();

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    SampleLibrary library;

    // Anzeige
    std::atomic<float> uiPos { 0 }, uiEnv { 0 }, uiGrain { 0 };
    std::atomic<bool> uiActive { false };
    std::atomic<int> stateVersion { 0 };

private:
    struct Slot
    {
        juce::String sampleRef { "factory:drums" };
        juce::String name;
        int preset = -1;
        bool edited = false;
        std::map<juce::String, float> values;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String& id, float newValue) override;
    void timerCallback() override;

    std::map<juce::String, float> captureValues() const;
    void applyValues (const std::map<juce::String, float>& values);
    void setActiveSample (SamplePtr s);
    void readParams (GrainParams& p) const;
    void handleMidi (const juce::MidiMessage& m, const GrainParams& p);
    void bump() { ++stateVersion; }

    std::array<Slot, numSlots> slots;
    int currentSlot = 0;
    std::atomic<bool> currentEdited { false };
    std::atomic<bool> suppressEdits { false };

    // Sample-Übergabe an den Audio-Thread
    juce::SpinLock sampleLock;
    SamplePtr activeSample, audioSample;
    std::vector<SamplePtr> graveyard;
    const SampleData* engineSample = nullptr;

    GrainEngine engine;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::SmoothedValue<float> gainSmooth;
    juce::HeapBlock<float> mono;
    int monoSize = 0;

    std::array<int, 128> held {};
    int numHeld = 0;
    float lastVel = 1.0f;
    bool prevHold = false;

    std::atomic<float> *pTune, *pStart, *pEnd, *pGrain, *pShift, *pSmooth, *pCrush, *pSrate,
                       *pAttack, *pRelease, *pCutoff, *pReso, *pDrive, *pVol, *pLoop, *pReverse, *pLegato, *pHold;

    JUCE_DECLARE_WEAK_REFERENCEABLE (MikrokornProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MikrokornProcessor)
};
