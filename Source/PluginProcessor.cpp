#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const char* trigIds[5] = { "recTrig", "playTrig", "undoTrig", "redoTrig", "clearTrig" };

// beats per division (quarter note = 1)
struct Div { const char* name; float beats; };
const Div divisions[] = {
    { "1/32", 0.125f }, { "1/16T", 1.0f / 6.0f }, { "1/16", 0.25f }, { "1/16.", 0.375f },
    { "1/8T", 1.0f / 3.0f }, { "1/8", 0.5f }, { "1/8.", 0.75f },
    { "1/4T", 2.0f / 3.0f }, { "1/4", 1.0f }, { "1/4.", 1.5f },
    { "1/2T", 4.0f / 3.0f }, { "1/2", 2.0f }, { "1/2.", 3.0f }, { "1/1", 4.0f },
};
} // namespace

const juce::StringArray& LoopTapeProcessor::divisionNames()
{
    static juce::StringArray names = [] {
        juce::StringArray s;
        for (auto& d : divisions) s.add (d.name);
        return s;
    }();
    return names;
}

float LoopTapeProcessor::divisionBeats (int index) const
{
    return divisions[juce::jlimit (0, (int) std::size (divisions) - 1, index)].beats;
}

juce::AudioProcessorValueTreeState::ParameterLayout LoopTapeProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + " %"; });

    NormalisableRange<float> timeRange (20.0f, 2500.0f);
    timeRange.setSkewForCentre (400.0f);

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "time", 1 }, "Delay Time", timeRange, 400.0f,
        AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction (
            [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : String (roundToInt (v)) + " ms"; })));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "sync", 1 }, "Tempo Sync", false));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "division", 1 }, "Division", divisionNames(), 8));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "feedback", 1 }, "Feedback",
                                                        NormalisableRange<float> (0.0f, 1.1f), 0.45f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
                                                        NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "killDry", 1 }, "Kill Dry", false));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tone", 1 }, "Tone",
                                                        NormalisableRange<float> (0.0f, 1.0f), 0.55f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "lowCut", 1 }, "Low Cut",
                                                        NormalisableRange<float> (0.0f, 1.0f), 0.2f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "wow", 1 }, "Wow",
                                                        NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "flutter", 1 }, "Flutter",
                                                        NormalisableRange<float> (0.0f, 1.0f), 0.25f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "saturation", 1 }, "Saturation",
                                                        NormalisableRange<float> (0.0f, 1.0f), 0.4f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "hiss", 1 }, "Hiss",
                                                        NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Stereo Mode",
                                                         StringArray { "Stereo", "Ping-Pong" }, 0));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "loopLevel", 1 }, "Loop Level",
                                                        NormalisableRange<float> (0.0f, 1.5f), 1.0f, pct));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "routing", 1 }, "Loop Routing",
                                                         StringArray { "Print Delay (Post)", "Feed Delay (Pre)" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "quantize", 1 }, "Loop Quantize",
                                                         StringArray { "Free", "Beat", "Bar" }, 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "reverse", 1 }, "Loop Reverse", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "halfSpeed", 1 }, "Loop Half Speed", false));

    const char* trigNames[5] = { "Looper Rec/Dub", "Looper Play/Stop", "Looper Undo", "Looper Redo", "Looper Clear" };
    for (int i = 0; i < 5; ++i)
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { trigIds[i], 1 }, trigNames[i], false));

    return { p.begin(), p.end() };
}

LoopTapeProcessor::LoopTapeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LoopTape", createLayout())
{
    pTime = apvts.getRawParameterValue ("time");
    pSync = apvts.getRawParameterValue ("sync");
    pDivision = apvts.getRawParameterValue ("division");
    pFeedback = apvts.getRawParameterValue ("feedback");
    pMix = apvts.getRawParameterValue ("mix");
    pKillDry = apvts.getRawParameterValue ("killDry");
    pTone = apvts.getRawParameterValue ("tone");
    pLowCut = apvts.getRawParameterValue ("lowCut");
    pWow = apvts.getRawParameterValue ("wow");
    pFlutter = apvts.getRawParameterValue ("flutter");
    pSat = apvts.getRawParameterValue ("saturation");
    pHiss = apvts.getRawParameterValue ("hiss");
    pMode = apvts.getRawParameterValue ("mode");
    pLoopLevel = apvts.getRawParameterValue ("loopLevel");
    pRouting = apvts.getRawParameterValue ("routing");
    pQuantize = apvts.getRawParameterValue ("quantize");
    pReverse = apvts.getRawParameterValue ("reverse");
    pHalf = apvts.getRawParameterValue ("halfSpeed");
    for (int i = 0; i < 5; ++i) pTrig[i] = apvts.getRawParameterValue (trigIds[i]);

    startTimerHz (30);
}

LoopTapeProcessor::~LoopTapeProcessor() { stopTimer(); }

bool LoopTapeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void LoopTapeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    delay.prepare (sampleRate);
    looper.prepare (sampleRate);
    looper.allocateExtraLayers();
    juce::ignoreUnused (samplesPerBlock);
    for (auto* s : { &dryGainSm, &wetGainSm, &loopGainSm }) s->reset (sampleRate, 0.02);
    for (int i = 0; i < 5; ++i) lastTrig[i] = pTrig[i]->load() > 0.5f;
}

void LoopTapeProcessor::handleTriggers (uint32_t& cmds)
{
    for (int i = 0; i < 5; ++i)
    {
        const bool on = pTrig[i]->load() > 0.5f;
        if (on && ! lastTrig[i]) cmds |= (1u << i); // rising edge (MIDI button pressed)
        lastTrig[i] = on;
    }
}

void LoopTapeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;

    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    // ---- host tempo
    double bpm = 120.0;
    int beatsPerBar = 4;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 400.0, *b);
            if (auto ts = pos->getTimeSignature()) beatsPerBar = juce::jmax (1, ts->numerator);
        }
    const double sr = getSampleRate();

    // ---- delay params
    lt::TapeDelay::Params dp;
    dp.timeMs = pSync->load() > 0.5f ? (float) (divisionBeats ((int) pDivision->load()) * 60000.0 / bpm) : pTime->load();
    dp.timeMs = juce::jlimit (20.0f, lt::TapeDelay::kMaxDelayMs, dp.timeMs);
    dp.feedback = pFeedback->load();
    dp.tone = pTone->load();
    dp.lowCut = pLowCut->load();
    dp.wow = pWow->load();
    dp.flutter = pFlutter->load();
    dp.saturation = pSat->load();
    dp.hiss = pHiss->load();
    dp.mode = (int) pMode->load();
    delay.setParams (dp);

    const float mix = pMix->load();
    dryGainSm.setTargetValue (pKillDry->load() > 0.5f ? 0.0f : juce::jmin (1.0f, 2.0f * (1.0f - mix)));
    wetGainSm.setTargetValue (juce::jmin (1.0f, 2.0f * mix));
    loopGainSm.setTargetValue (pLoopLevel->load());
    const bool pre = (int) pRouting->load() == PreDelay;

    // ---- looper settings + commands
    const int q = (int) pQuantize->load();
    const double beatSamples = 60.0 / bpm * sr;
    looper.setQuantizeUnit (q == 1 ? beatSamples : q == 2 ? beatSamples * beatsPerBar : 0.0);
    looper.setReverse (pReverse->load() > 0.5f);
    looper.setHalfSpeed (pHalf->load() > 0.5f);

    uint32_t cmds = pendingCommands.exchange (0, std::memory_order_acq_rel);
    handleTriggers (cmds);
    if (cmds & CmdClear)  looper.pressClear();
    if (cmds & CmdUndo)   looper.pressUndo();
    if (cmds & CmdRedo)   looper.pressRedo();
    if (cmds & CmdRecord) looper.pressRecord();
    if (cmds & CmdPlay)   looper.pressPlayStop();

    // ---- audio (per sample: the loop can feed the delay and the delay can be
    //      printed into the loop, both sample-accurately)
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    float peak = 0.0f;

    for (int s = 0; s < n; ++s)
    {
        const float inL = L[s], inR = R[s];
        const float lg = loopGainSm.getNextValue();
        const float dg = dryGainSm.getNextValue();
        const float wg = wetGainSm.getNextValue();

        float lL, lR;
        looper.read (lL, lR);
        lL *= lg; lR *= lg;

        float dInL = inL, dInR = inR;
        if (pre) { dInL += lL; dInR += lR; }

        float wetL, wetR;
        delay.process (&dInL, &dInR, &wetL, &wetR, 1);
        wetL *= wg; wetR *= wg;

        // Post: each layer captures input + its echoes ("printed" delay).
        // Pre:  layers capture the clean input; the delay runs over the loop.
        // The input is always recorded, even with Kill Dry (return-track use).
        if (pre) looper.writeAndAdvance (inL, inR);
        else     looper.writeAndAdvance (inL + wetL, inR + wetR);

        L[s] = inL * dg + wetL + lL;
        R[s] = inR * dg + wetR + lR;
        peak = juce::jmax (peak, std::abs (lL), std::abs (lR));
    }

    // ---- publish looper state for the GUI
    info.state.store ((int) looper.getState());
    info.layers.store (looper.getNumLayers());
    info.stored.store (looper.getNumStored());
    info.pendingDub.store (looper.isPendingDub());
    info.loopSeconds.store ((double) looper.getLoopLength() / sr);
    info.position.store (looper.getPosition() / sr);
    info.recSeconds.store ((double) looper.getRecLength() / sr);
    info.recTargetSeconds.store ((double) looper.getRecTarget() / sr);
    levelFollower = juce::jmax (peak, levelFollower * 0.9f);
    info.outLevel.store (levelFollower);
}

void LoopTapeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void LoopTapeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            for (auto* id : trigIds)
                if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (0.0f);
        }
}

juce::AudioProcessorEditor* LoopTapeProcessor::createEditor() { return new LoopTapeEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LoopTapeProcessor(); }
