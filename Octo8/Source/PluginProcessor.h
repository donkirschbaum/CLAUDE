#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "SynthData.h"

// Band-limited wavetables (one table per octave band) for all 21 waveforms.
struct WaveBank
{
    static constexpr int N = 2048, BANDS = 11, H = 256;
    std::vector<float> data;
    double sampleRate = 0;

    void build (double sr);
    const float* table (int w, int band) const { return data.data() + ((size_t) w * BANDS + (size_t) band) * (N + 1); }
    static int bandFor (double f)
    {
        int b = 0; double top = 40.0;
        while (b < BANDS - 1 && f > top) { top *= 2.0; ++b; }
        return b;
    }
};

struct Channel
{
    double phase[7] {};
    double inc[7] {};
    const float* tab = nullptr;
    double noisePos = 0, noiseRate = 1;
    int deg = 0;
    bool isNoise = false, isSuper = false;
    int ntype = 0;
    float env = 0, envTarget = 0, envCoef = 0;
    int decayCnt = 0, gateCnt = 0;
    float vca = 0, cutoff = 2000;
    juce::dsp::StateVariableTPTFilter<float> filter;
};

class Octo8Processor : public juce::AudioProcessor
{
public:
    Octo8Processor();
    ~Octo8Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Octo8"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- shared with the editor ----
    std::atomic<float>* keyRaw = nullptr; // declared before apvts on purpose (used in text lambdas)
    juce::AudioProcessorValueTreeState apvts;

    std::atomic<bool> stepOn[octo::NCH][octo::NSTEPS];
    std::atomic<int> stepDeg[octo::NCH][octo::NSTEPS];
    std::atomic<int> playStep { -1 };
    std::atomic<int> hitCount[octo::NCH];
    std::atomic<bool> hostPlaying { false };
    std::atomic<float> hostBpm { 120.0f };

    static constexpr int SCOPE = 4096;
    std::array<float, SCOPE> scope {};
    std::atomic<int> scopeWrite { 0 };

    // actions (message thread)
    void randomChannel (int ch);
    void masterRandom();
    void randomTrack (int ch);
    void randomAllTracks();
    void clearAll();
    void requantize();

    bool audible (int ch) const;
    bool trackOn (int ch) const;
    float param (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    static juce::String pid (int ch, const char* name) { return "c" + juce::String (ch + 1) + "_" + name; }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParam (const juce::String& id, float value);
    void initDefaultPattern();

    // scale helpers (no allocation, audio-thread safe)
    bool inScale (int midi, int key, int scaleIdx) const;
    int quant (int midi, int key, int scaleIdx) const;
    int degShift (int midi, int deg, int key, int scaleIdx) const;
    double freqOf (int ch, int deg, int key, int scaleIdx) const;

    void trigger (int ch, double gateSec, int deg);
    void releaseAll();
    void startDrone();
    void updatePitch (int ch, int key, int scaleIdx);
    float envCoef (float tau) const { return 1.0f - std::exp (-1.0f / (juce::jmax (tau, 0.0001f) * (float) sr)); }

    struct ChP
    {
        std::atomic<float> *wave, *noise, *nt, *gain, *tune, *fine, *mute, *solo, *ft, *cut, *q, *a, *d, *s, *r, *smute, *ssolo;
    } cp[octo::NCH];
    std::atomic<float> *pKey, *pScale, *pMode, *pPlay, *pBpm, *pGate, *pMaster;

    WaveBank bank;
    std::vector<float> noiseBuf[octo::NNOISE];
    Channel chans[octo::NCH];
    juce::dsp::Compressor<float> comp;

    double sr = 44100.0;
    double intPpq = 0;
    long long lastAbsStep = std::numeric_limits<long long>::min();
    bool wasRunning = false, droneOn = false;
    int lastMode = -1, transpose = 0, held = 0;
    float masterSm = 0.6f;
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Octo8Processor)
};
