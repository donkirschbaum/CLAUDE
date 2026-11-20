#pragma once

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
    } info;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    static const juce::StringArray& divisionNames();

private:
    void timerCallback() override { looper.allocateExtraLayers(); }
    void handleTriggers (uint32_t& cmds);
    float divisionBeats (int index) const;

    lt::TapeDelay delay;
    lt::Looper looper;

    std::atomic<uint32_t> pendingCommands { 0 };
    bool lastTrig[5] {};

    juce::SmoothedValue<float> dryGainSm, wetGainSm, loopGainSm;
    float levelFollower = 0.0f;

    // cached parameter pointers
    std::atomic<float>* pTime = nullptr;     std::atomic<float>* pSync = nullptr;
    std::atomic<float>* pDivision = nullptr; std::atomic<float>* pFeedback = nullptr;
    std::atomic<float>* pMix = nullptr;      std::atomic<float>* pKillDry = nullptr;
    std::atomic<float>* pTone = nullptr;     std::atomic<float>* pLowCut = nullptr;
    std::atomic<float>* pWow = nullptr;      std::atomic<float>* pFlutter = nullptr;
    std::atomic<float>* pSat = nullptr;      std::atomic<float>* pHiss = nullptr;
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pLoopLevel = nullptr; std::atomic<float>* pRouting = nullptr;
    std::atomic<float>* pQuantize = nullptr;  std::atomic<float>* pReverse = nullptr;
    std::atomic<float>* pHalf = nullptr;
    std::atomic<float>* pTrig[5] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopTapeProcessor)
};
