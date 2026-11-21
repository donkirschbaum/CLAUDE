#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace ws
{

struct FxSettings
{
    bool distOn = false;   float distDrive = 0.3f, distMix = 1.0f;
    bool chorusOn = false; float chorusRate = 0.6f, chorusDepth = 0.4f, chorusMix = 0.4f;
    bool delayOn = false;  double delayBeats = 0.75; float delayFb = 0.35f, delayMix = 0.25f, delayTone = 0.6f;
    bool reverbOn = false; float reverbSize = 0.6f, reverbDamp = 0.5f, reverbMix = 0.2f, reverbWidth = 1.0f;
    float masterGain = 0.5f;
    double bpm = 120.0;
};

class EffectChain
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const FxSettings& s);

private:
    double sampleRate = 44100.0;

    juce::dsp::Chorus<float> chorus;

    std::vector<float> delayL, delayR;
    int writePos = 0;
    float delaySamplesSmoothed = 0.0f;
    float toneL = 0.0f, toneR = 0.0f;

    juce::Reverb reverb;
    juce::SmoothedValue<float> gain;
    float distMixSmoothed = 0.0f;
};

} // namespace ws
