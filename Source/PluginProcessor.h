#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

namespace pf
{
    constexpr int kNumSteps = 16;

    enum class Lane { Gate = 0, Filter, Pan, Count };

    inline juce::String laneName (Lane l)
    {
        switch (l)
        {
            case Lane::Gate:   return "gate";
            case Lane::Filter: return "filt";
            case Lane::Pan:    return "pan";
            case Lane::Count:
            default:           return {};
        }
    }

    inline juce::String stepId (Lane l, int step) { return laneName (l) + "Step" + juce::String (step + 1); }

    // Simple TPT state-variable filter (Zavalishin), cheap to modulate per sample.
    struct SVF
    {
        float ic1 = 0.0f, ic2 = 0.0f;
        void reset() { ic1 = ic2 = 0.0f; }

        // mode: 0 = LP, 1 = BP, 2 = HP
        inline float process (float x, float g, float k, int mode)
        {
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float a2 = g * a1;
            const float a3 = g * a2;
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            if (mode == 0) return v2;
            if (mode == 1) return v1;
            return x - k * v1 - v2;
        }
    };
}

class PulseForgeProcessor : public juce::AudioProcessor
{
public:
    PulseForgeProcessor();
    ~PulseForgeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PulseForge"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // For the GUI playhead display
    std::atomic<int> currentStep { 0 };

    void randomiseLane (pf::Lane lane);
    void applyPreset (int index);

private:
    double sr = 44100.0;
    double ppqFree = 0.0;      // free-running position when host is stopped
    double lastBpm = 120.0;

    // smoothed per-sample states
    float gateEnv = 1.0f;
    float filtMod = 0.0f;
    float panMod  = 0.0f;

    std::array<pf::SVF, 2> filters;

    // crusher
    std::array<float, 2> crushHold { 0.0f, 0.0f };
    float crushCounter = 0.0f;

    // delay
    juce::AudioBuffer<float> delayBuf;
    int delayWrite = 0;
    juce::SmoothedValue<float> delayTimeSmoothed;
    std::array<float, 2> delayLP { 0.0f, 0.0f };

    juce::Reverb reverb;
    juce::AudioBuffer<float> dryBuf, wetTmp;

    int currentPreset = 0;

    // cached parameter pointers
    std::array<std::array<std::atomic<float>*, pf::kNumSteps>, 3> steps {};
    std::atomic<float>* pRate = nullptr;
    std::atomic<float>* pLength = nullptr;
    std::atomic<float>* pSwing = nullptr;

    std::atomic<float>* pGateOn = nullptr;
    std::atomic<float>* pGateDepth = nullptr;
    std::atomic<float>* pGateLen = nullptr;
    std::atomic<float>* pGateSmooth = nullptr;

    std::atomic<float>* pFiltOn = nullptr;
    std::atomic<float>* pFiltType = nullptr;
    std::atomic<float>* pCutoff = nullptr;
    std::atomic<float>* pReso = nullptr;
    std::atomic<float>* pFiltDepth = nullptr;
    std::atomic<float>* pFiltGlide = nullptr;

    std::atomic<float>* pPanOn = nullptr;
    std::atomic<float>* pPanDepth = nullptr;

    std::atomic<float>* pDrive = nullptr;
    std::atomic<float>* pBits = nullptr;
    std::atomic<float>* pDownsample = nullptr;

    std::atomic<float>* pDelayOn = nullptr;
    std::atomic<float>* pDelayTime = nullptr;
    std::atomic<float>* pDelayFb = nullptr;
    std::atomic<float>* pDelayPing = nullptr;
    std::atomic<float>* pDelayMix = nullptr;

    std::atomic<float>* pRevOn = nullptr;
    std::atomic<float>* pRevSize = nullptr;
    std::atomic<float>* pRevDamp = nullptr;
    std::atomic<float>* pRevMix = nullptr;

    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOut = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PulseForgeProcessor)
};
