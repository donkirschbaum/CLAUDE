// Shruthi XT plugin processor. GPL v3.0.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "ShruthiEngine.h"
#include <atomic>

class ShruthiProcessor : public juce::AudioProcessor
{
public:
    ShruthiProcessor();
    ~ShruthiProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Shruthi XT"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- called on the message thread by the editor ---
    juce::var stateAsVar();                       // full patch + system for the UI
    void applyPatchFromUI (const juce::var& p);   // UI edited the patch
    void applySysFromUI (const juce::var& g);
    void postUIEvent (const juce::var& m);        // notes, jam, bend ...
    void readParamValues (int* out) const;        // current scalar params
    int  currentStep() const { return lastStep.load(); }
    bool jamState() const;
    juce::var loadStore();
    void saveStore (const juce::var& data);
    std::atomic<int> stateVersion { 0 };          // bumps when patch/sys replaced natively

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void pushPatchToAudio();
    void setParamsFromPatch (const shruthi::Patch& p);
    shruthi::Patch patchFromParams() const;
    juce::File storeFile() const;

    std::unique_ptr<shruthi::Engine> engine;
    std::array<std::atomic<float>*, shruthi::kNumParams> paramPtr {};
    std::atomic<float>* volPtr = nullptr;
    std::atomic<float>* jamPtr = nullptr;

    // non-automatable patch parts + system settings (message thread copies)
    shruthi::Patch extra;     // mtx, steps, name live here
    shruthi::Sys sys;
    int chn = 0, lcdc = 0, sync = 1, currentProgram = 0;

    // hand-over to the audio thread
    juce::SpinLock lock;
    shruthi::Patch pendingPatch; shruthi::Sys pendingSys; bool pending = true;
    shruthi::Patch audioPatch;

    struct Msg { int type; int a; float f; };
    juce::AbstractFifo fifo { 1024 };
    std::array<Msg, 1024> msgs;
    void pushMsg (Msg m);

    bool lastJam = false;
    std::atomic<int> lastStep { -1 };
    double hostBpmCache = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShruthiProcessor)
};
