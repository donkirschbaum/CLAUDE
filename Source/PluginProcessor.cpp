#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const char* trigIds[5] = { "recTrig", "playTrig", "undoTrig", "redoTrig", "clearTrig" };

const int barOptions[] = { 0, 1, 2, 4, 8, 16 };
} // namespace

const juce::StringArray& LoopTapeProcessor::barChoices()
{
    static juce::StringArray names { "Frei", "1 Takt", "2 Takte", "4 Takte", "8 Takte", "16 Takte" };
    return names;
}

int LoopTapeProcessor::barsForChoice (int index)
{
    return barOptions[juce::jlimit (0, (int) std::size (barOptions) - 1, index)];
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
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "loopBars", 1 }, "Loop Bars", barChoices(), 3));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "hostSync", 1 }, "Looper Host Sync", true));
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
    pHost = apvts.getRawParameterValue ("hostSync");
    pBars = apvts.getRawParameterValue ("loopBars");
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
    currentSR = sampleRate > 0.0 ? sampleRate : 44100.0;
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

lt::TapeDelay::Params LoopTapeProcessor::makeDelayParams() const
{
    lt::TapeDelay::Params dp;
    dp.timeMs = juce::jlimit (20.0f, lt::TapeDelay::kMaxDelayMs, pTime->load());
    dp.feedback = pFeedback->load();
    dp.tone = pTone->load();
    dp.lowCut = pLowCut->load();
    dp.wow = pWow->load();
    dp.flutter = pFlutter->load();
    dp.saturation = pSat->load();
    dp.hiss = pHiss->load();
    dp.mode = (int) pMode->load();
    return dp;
}

void LoopTapeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    using S = lt::Looper::State;
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;

    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    // ---- host transport / tempo
    double bpm = 120.0, ppq0 = 0.0, barStartPpq = 0.0;
    int sigNum = 4, sigDen = 4;
    bool bpmKnown = false, ppqKnown = false, playing = false, barStartKnown = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) { bpm = juce::jlimit (20.0, 400.0, *b); bpmKnown = true; }
            if (auto ts = pos->getTimeSignature()) { sigNum = juce::jmax (1, ts->numerator); sigDen = juce::jmax (1, ts->denominator); }
            if (auto q = pos->getPpqPosition()) { ppq0 = *q; ppqKnown = true; }
            if (auto lb = pos->getPpqPositionOfLastBarStart()) { barStartPpq = *lb; barStartKnown = true; }
            playing = pos->getIsPlaying();
        }
    const double sr = currentSR;
    const double barQ = (double) sigNum * 4.0 / (double) sigDen; // quarter notes per bar
    const double spq = 60.0 / bpm * sr;                           // samples per quarter note
    const double qps = 1.0 / spq;
    if (! barStartKnown) barStartPpq = 0.0;                       // assume bars counted from 0

    const bool hostWanted = pHost->load() > 0.5f;
    const bool hostMode = hostWanted && bpmKnown && ppqKnown;
    const int bars = barsForChoice ((int) pBars->load());
    const bool half = pHalf->load() > 0.5f;
    const bool rev = pReverse->load() > 0.5f;

    // ---- delay
    delay.setParams (makeDelayParams());

    const float mix = pMix->load();
    dryGainSm.setTargetValue (pKillDry->load() > 0.5f ? 0.0f : juce::jmin (1.0f, 2.0f * (1.0f - mix)));
    wetGainSm.setTargetValue (juce::jmin (1.0f, 2.0f * mix));
    loopGainSm.setTargetValue (pLoopLevel->load());
    const bool pre = (int) pRouting->load() == PreDelay;

    // ---- looper settings
    looper.setFixedRecordLength (bars > 0 ? (int64_t) std::llround (bars * barQ * spq) : 0);
    looper.setQuantizeUnit (bars == 0 && hostMode ? barQ * spq : 0.0); // free + host: close on nearest bar
    looper.setReverse (rev);
    looper.setHalfSpeed (half);
    if (! hostMode) { hostArmed = false; looper.setExternalClock (false); }

    // ---- commands
    uint32_t cmds = pendingCommands.exchange (0, std::memory_order_acq_rel);
    handleTriggers (cmds);
    if (cmds & (CmdClear | CmdUndo)) hostArmed = false;
    if (cmds & CmdClear)  looper.pressClear();
    if (cmds & CmdUndo)   looper.pressUndo();
    if (cmds & CmdRedo)   looper.pressRedo();
    if (cmds & CmdRecord)
    {
        if (hostMode && looper.getState() == S::Empty)
        {
            hostArmed = ! hostArmed; // start on the next bar line (press again to cancel)
            armPrevFrac = -1.0;
        }
        else
            looper.pressRecord();
    }
    if (cmds & CmdPlay)
    {
        if (hostArmed) hostArmed = false;
        else looper.pressPlayStop();
    }

    // ---- audio (per sample: the loop can feed the delay and the delay can be
    //      printed into the loop, both sample-accurately)
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    float peak = 0.0f;
    const float tgStep = (float) (1.0 / (0.01 * sr));

    for (int s = 0; s < n; ++s)
    {
        const double ppq = ppq0 + (double) s * qps;

        if (hostMode)
        {
            // armed: start exactly on the next bar line while Ableton plays
            if (hostArmed && playing)
            {
                const double bp = (ppq - barStartPpq) / barQ;
                const double frac = bp - std::floor (bp);
                if (frac * barQ < qps || (armPrevFrac >= 0.0 && frac < armPrevFrac))
                {
                    hostArmed = false;
                    looper.setExternalClock (false);
                    looper.pressRecord();
                    hostStartPpq = ppq - frac * barQ;
                    hostStartValid = true;
                }
                armPrevFrac = frac;
            }

            // locked playhead: loop phase follows Ableton's song position
            const auto st = looper.getState();
            const int64_t len = looper.getLoopLength();
            if (len > 0 && hostLoopQuarters > 0.0 && st != S::Recording && st != S::Empty)
            {
                const double span = hostLoopQuarters * (half ? 2.0 : 1.0);
                const double rel = (ppq - hostStartPpq) / span;
                double p = (rel - std::floor (rel)) * (double) len;
                if (rev) p = (double) (len - 1) - p;
                looper.setExternalClock (true);
                looper.setExternalPosition (p);
            }
            else
                looper.setExternalClock (false);
        }

        // follow Ableton's transport in host mode (mute when stopped)
        const float tgTarget = (! hostMode || playing) ? 1.0f : 0.0f;
        if (transportGain != tgTarget)
            transportGain = tgTarget > transportGain ? juce::jmin (tgTarget, transportGain + tgStep)
                                                     : juce::jmax (tgTarget, transportGain - tgStep);

        const float inL = L[s], inR = R[s];
        const float lg = loopGainSm.getNextValue() * transportGain;
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

        // a new loop was closed (or the loop was cleared) -> (re)lock to host
        const int64_t len = looper.getLoopLength();
        if (len != lastLoopLen)
        {
            lastLoopLen = len;
            hostLoopQuarters = 0.0;
            if (len <= 0) hostStartValid = false;
        }
        if (hostMode && len > 0 && hostLoopQuarters <= 0.0 && looper.getState() != S::Recording)
        {
            const double barsF = (double) len / (barQ * spq);
            hostLoopQuarters = std::abs (barsF - std::round (barsF)) < 0.02 && std::round (barsF) >= 1.0
                                   ? std::round (barsF) * barQ
                                   : juce::jmax (1.0, std::round ((double) len / spq));
            if (! hostStartValid)
            {
                // loop recorded without host: restart it from the current bar
                const double bp = (ppq - barStartPpq) / barQ;
                hostStartPpq = barStartPpq + std::floor (bp) * barQ;
                hostStartValid = true;
            }
        }

        L[s] = inL * dg + wetL + lL;
        R[s] = inR * dg + wetR + lR;
        peak = juce::jmax (peak, std::abs (lL), std::abs (lR));
    }
    if (! hostMode && looper.getLoopLength() > 0) hostStartValid = false; // re-lock when HOST is switched on again

    // ---- publish looper state for the GUI
    info.state.store ((int) looper.getState());
    info.layers.store (looper.getNumLayers());
    info.stored.store (looper.getNumStored());
    info.pendingDub.store (looper.isPendingDub());
    info.loopSeconds.store ((double) looper.getLoopLength() / sr);
    info.position.store (looper.getPosition() / sr);
    info.recSeconds.store ((double) looper.getRecLength() / sr);
    info.recTargetSeconds.store ((double) looper.getRecTarget() / sr);
    info.hostBpm.store (bpm);
    info.hostBpmKnown.store (bpmKnown);
    info.hostWanted.store (hostWanted);
    info.hostActive.store (hostMode);
    info.hostArmed.store (hostArmed);
    info.transportPlaying.store (playing);
    info.sigNum.store (sigNum);
    info.sigDen.store (sigDen);
    info.fixedBars.store (bars);
    info.barSeconds.store (barQ * 60.0 / bpm);
    info.loopBars.store (looper.getLoopLength() > 0 ? (double) looper.getLoopLength() / (barQ * spq) : 0.0);
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

bool LoopTapeProcessor::renderOutput (juce::AudioBuffer<float>& out, int layer)
{
    juce::AudioBuffer<float> raw;
    if (! renderLoop (raw, layer)) return false;

    const double rate = pHalf->load() > 0.5f ? 0.5 : 1.0;
    const int dir = pReverse->load() > 0.5f ? -1 : 1;
    const int64_t len = raw.getNumSamples();
    const int64_t n = lt::Looper::playbackLength (len, rate);
    if (n <= 0 || n > std::numeric_limits<int>::max()) return false;

    out.setSize (2, (int) n, false, true, false);
    for (int c = 0; c < 2; ++c)
        lt::Looper::renderPlayback (raw.getReadPointer (c), len, out.getWritePointer (c), rate, dir);
    out.applyGain (pLoopLevel->load());

    // "Feed Delay": the echoes of the loop are part of the looper output.
    // Run the loop twice through a fresh delay and keep the 2nd (steady
    // state) pass, so the file loops seamlessly including the echo tails.
    if (layer < 0 && (int) pRouting->load() == PreDelay)
    {
        lt::TapeDelay d;
        d.prepare (looper.getSampleRate());
        d.setParams (makeDelayParams());
        const float wg = juce::jmin (1.0f, 2.0f * pMix->load());
        juce::AudioBuffer<float> wet (2, (int) n);
        for (int pass = 0; pass < 2; ++pass)
            d.process (out.getReadPointer (0), out.getReadPointer (1), wet.getWritePointer (0), wet.getWritePointer (1), (int) n);
        for (int c = 0; c < 2; ++c) out.addFrom (c, 0, wet, c, 0, (int) n, wg);
    }
    return true;
}

juce::Result LoopTapeProcessor::writeWav (const juce::File& file, int layer)
{
    juce::AudioBuffer<float> buf;
    if (! renderOutput (buf, layer)) return juce::Result::fail ("Kein Loop vorhanden.");

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
