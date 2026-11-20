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
    const juce::ScopedLock sl (exportLock);
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
    bool bpmKnown = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) { bpm = juce::jlimit (20.0, 400.0, *b); bpmKnown = true; }
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
    info.hostBpm.store (bpm);
    info.hostBpmKnown.store (bpmKnown);
    info.delayMs.store (dp.timeMs);

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

//============================================================================== export
bool LoopTapeProcessor::renderLoop (juce::AudioBuffer<float>& out, int layer)
{
    const juce::ScopedLock sl (exportLock);
    const int64_t len = looper.getLoopLength();
    const int layers = info.layers.load();
    if (len <= 0 || layers <= 0 || len > std::numeric_limits<int>::max()) return false;

    out.setSize (2, (int) len, false, true, false);
    out.clear();
    if (layer >= 0)
        return layer < layers && looper.copyLayer (layer, out.getWritePointer (0), out.getWritePointer (1));

    juce::AudioBuffer<float> tmp (2, (int) len);
    bool any = false;
    for (int k = 0; k < layers; ++k)
        if (looper.copyLayer (k, tmp.getWritePointer (0), tmp.getWritePointer (1)))
        {
            for (int c = 0; c < 2; ++c) out.addFrom (c, 0, tmp, c, 0, (int) len);
            any = true;
        }
    return any;
}

juce::Result LoopTapeProcessor::writeWav (const juce::File& file, int layer)
{
    juce::AudioBuffer<float> buf;
    if (! renderLoop (buf, layer)) return juce::Result::fail ("Kein Loop vorhanden.");

    file.getParentDirectory().createDirectory();
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
    if (! static_cast<juce::FileOutputStream*> (stream.get())->openedOk())
        return juce::Result::fail ("Datei kann nicht geschrieben werden: " + file.getFullPathName());

    // 32-bit float: summed layers can exceed 0 dBFS without clipping
    juce::WavAudioFormat wav;
    auto opts = juce::AudioFormatWriterOptions()
                    .withSampleRate (looper.getSampleRate())
                    .withNumChannels (2)
                    .withBitsPerSample (32)
                    .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
    auto writer = wav.createWriterFor (stream, opts);
    if (writer == nullptr) return juce::Result::fail ("WAV-Writer konnte nicht erstellt werden.");
    if (! writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples()))
        return juce::Result::fail ("Fehler beim Schreiben.");
    return juce::Result::ok();
}

int LoopTapeProcessor::writeAllLayers (const juce::File& folder)
{
    folder.createDirectory();
    int written = 0;
    if (writeWav (folder.getChildFile (suggestedFileName ("Mix")), -1).wasOk()) ++written;
    const int layers = info.layers.load();
    for (int k = 0; k < layers; ++k)
        if (writeWav (folder.getChildFile (suggestedFileName ("Layer" + juce::String (k + 1).paddedLeft ('0', 2))), k).wasOk())
            ++written;
    return written;
}

juce::File LoopTapeProcessor::defaultExportFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("LoopTape");
}

juce::String LoopTapeProcessor::suggestedFileName (const juce::String& suffix) const
{
    const auto now = juce::Time::getCurrentTime().formatted ("%Y-%m-%d_%H%M%S");
    const auto bpm = juce::String (info.hostBpm.load(), 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
    return "LoopTape_" + bpm + "bpm_" + now + "_" + suffix + ".wav";
}

juce::File LoopTapeProcessor::renderToExportFolder()
{
    auto f = defaultExportFolder().getChildFile (suggestedFileName ("Mix"));
    return writeWav (f, -1).wasOk() ? f : juce::File();
}

juce::AudioProcessorEditor* LoopTapeProcessor::createEditor() { return new LoopTapeEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LoopTapeProcessor(); }
