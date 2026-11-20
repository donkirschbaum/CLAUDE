#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Looper.h"
#include "TapeDelay.h"

class LoopTapeProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    enum Command : uint32_t
    {
        CmdRecord = 1u << 0,
        CmdPlay   = 1u << 1,
        CmdUndo   = 1u << 2,
        CmdRedo   = 1u << 3,
        CmdClear  = 1u << 4,
    };

    enum Routing { PostDelay = 0, PreDelay = 1 };

    LoopTapeProcessor();
    ~LoopTapeProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // GUI -> audio thread
    void sendCommand (Command c) { pendingCommands.fetch_or (c, std::memory_order_acq_rel); }

    // audio thread -> GUI (snapshot, updated every block)
    struct LooperInfo
    {
        std::atomic<int> state { 0 }, layers { 0 }, stored { 0 };
        std::atomic<bool> pendingDub { false };
        std::atomic<double> loopSeconds { 0.0 }, position { 0.0 }, recSeconds { 0.0 }, recTargetSeconds { 0.0 };
        std::atomic<float> outLevel { 0.0f };
        std::atomic<double> hostBpm { 120.0 };
        std::atomic<bool> hostBpmKnown { false };
        std::atomic<bool> hostWanted { true }, hostActive { false }, hostArmed { false }, transportPlaying { false };
        std::atomic<int> sigNum { 4 }, sigDen { 4 }, fixedBars { 4 };
        std::atomic<double> barSeconds { 2.0 }, loopBars { 0.0 };
    } info;

    static const juce::StringArray& barChoices();
    static int barsForChoice (int index); // 0 = free

    // ---- export (message thread)
    // raw layer data (forward, normal speed). layer = -1: mix of active layers
    bool renderLoop (juce::AudioBuffer<float>& out, int layer);
    // what the looper actually outputs: speed, direction, loop level and, in
    // "Feed Delay" routing, the echoes of the loop (seamless, steady state)
    bool renderOutput (juce::AudioBuffer<float>& out, int layer);
    juce::Result writeWav (const juce::File& file, int layer);
    // writes every active layer + the mix into a folder, returns number of files
    int writeAllLayers (const juce::File& folder);
    // renders the mix into ~/Music/LoopTape and returns the file (for drag & drop)
    juce::File renderToExportFolder();
    juce::String suggestedFileName (const juce::String& suffix) const;
    static juce::File defaultExportFolder();

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void timerCallbackForTest() { timerCallback(); }

private:
    void timerCallback() override { looper.allocateExtraLayers(); }
    void handleTriggers (uint32_t& cmds);
    lt::TapeDelay::Params makeDelayParams() const;

    lt::TapeDelay delay;
    lt::Looper looper;

    std::atomic<uint32_t> pendingCommands { 0 };
    juce::CriticalSection exportLock; // keeps prepareToPlay from reallocating during an export copy
    bool lastTrig[5] {};

    juce::SmoothedValue<float> dryGainSm, wetGainSm, loopGainSm;
    float levelFollower = 0.0f;

    // host sync state (audio thread)
    bool hostArmed = false;          // waiting for the next bar line to start recording
    double armPrevFrac = -1.0;
    double hostLoopQuarters = 0.0;   // loop length in quarter notes (0 = not locked yet)
    double hostStartPpq = 0.0;       // ppq of the loop's first sample
    bool hostStartValid = false;
    int64_t lastLoopLen = 0;
    float transportGain = 1.0f;
    double currentSR = 44100.0;

    // cached parameter pointers
    std::atomic<float>* pTime = nullptr;     std::atomic<float>* pHost = nullptr;
    std::atomic<float>* pBars = nullptr;     std::atomic<float>* pFeedback = nullptr;
    std::atomic<float>* pMix = nullptr;      std::atomic<float>* pKillDry = nullptr;
    std::atomic<float>* pTone = nullptr;     std::atomic<float>* pLowCut = nullptr;
    std::atomic<float>* pWow = nullptr;      std::atomic<float>* pFlutter = nullptr;
    std::atomic<float>* pSat = nullptr;      std::atomic<float>* pHiss = nullptr;
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pLoopLevel = nullptr; std::atomic<float>* pRouting = nullptr;
    std::atomic<float>* pReverse = nullptr;
    std::atomic<float>* pHalf = nullptr;
    std::atomic<float>* pTrig[5] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopTapeProcessor)
};
