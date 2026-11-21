#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "SynthEngine.h"
#include "Effects.h"
#include "PresetManager.h"

class WavesmithProcessor : public juce::AudioProcessor
{
public:
    WavesmithProcessor();
    ~WavesmithProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Wavesmith"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    ws::PresetManager presets;
    ws::SynthEngine engine;

    // MIDI from the on-screen keyboard
    juce::MidiKeyboardState keyboardState;

    std::atomic<int> presetChangeCounter { 0 };

private:
    void updateSettings();
    float get (const char* id) const;
    float get (const juce::String& id) const;

    std::unordered_map<juce::String, std::atomic<float>*> paramCache;

    ws::SynthSettings settings;
    ws::ClockSettings clock;
    ws::FxSettings fx;
    ws::EffectChain effects;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WavesmithProcessor)
};
