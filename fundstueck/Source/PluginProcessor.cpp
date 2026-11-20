#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace fs;

namespace fs
{
const Style kStyles[6] = {
    { "Boom Bap", 90, 58, 0.0f,
      {1,0,0,0,0,0,0,.35f,.3f,0,.9f,0,0,0,.3f,0}, {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,.2f},
      {.95f,.1f,.9f,.1f,.95f,.1f,.9f,.2f,.95f,.1f,.9f,.1f,.95f,.2f,.9f,.3f}, {0,0,.2f,0,0,0,.3f,0,0,.2f,0,0,0,0,.3f,0} },
    { "House", 122, 54, 0.0f,
      {1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0}, {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
      {.2f,0,1,0,.2f,0,1,0,.2f,0,1,0,.2f,0,1,.3f}, {0,0,0,.3f,0,0,.25f,0,0,.3f,0,0,0,.4f,0,0} },
    { "Trap", 140, 50, 0.25f,
      {1,0,0,0,0,0,0,.5f,0,0,.7f,0,0,.3f,0,0}, {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
      {1,.7f,1,.7f,1,.7f,1,.7f,1,.7f,1,.7f,1,.7f,1,.7f}, {0,0,0,0,0,.2f,0,0,0,0,0,.3f,0,0,.2f,0} },
    { "Afro", 106, 56, 0.0f,
      {1,0,0,.5f,0,0,.8f,0,1,0,0,.4f,0,0,.7f,0}, {0,0,0,.95f,0,0,.95f,0,0,0,0,.95f,0,0,.95f,0},
      {.9f,.4f,.9f,.4f,.9f,.4f,.9f,.4f,.9f,.4f,.9f,.4f,.9f,.4f,.9f,.4f}, {0,0,.4f,0,.3f,0,0,.4f,0,.3f,.4f,0,.3f,0,0,.4f} },
    { "Lo-Fi", 80, 62, 0.0f,
      {1,0,0,0,0,0,.2f,0,0,.4f,.8f,0,0,0,0,.2f}, {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
      {.9f,0,.7f,0,.9f,0,.7f,.2f,.9f,0,.7f,0,.9f,0,.7f,.3f}, {.15f,0,0,0,0,0,0,.25f,0,0,0,0,0,.2f,0,0} },
    { "Breakbeat", 128, 52, 0.1f,
      {1,0,.4f,0,0,0,0,0,0,0,1,.5f,0,0,0,0}, {0,0,0,0,1,0,0,.25f,0,.3f,0,0,1,0,0,.3f},
      {.9f,.3f,.9f,.3f,.9f,.3f,.9f,.3f,.9f,.3f,.9f,.3f,.9f,.3f,.9f,.3f}, {0,0,0,.3f,0,0,.2f,0,0,0,0,.3f,0,.2f,0,0} },
};
const Scale kScales[5] = {
    { "Moll-Penta", 5, {0,3,5,7,10} }, { "Dur-Penta", 5, {0,2,4,7,9} }, { "Moll", 7, {0,2,3,5,7,8,10} },
    { "Dur", 7, {0,2,4,5,7,9,11} }, { "Dorisch", 7, {0,2,3,5,7,9,10} },
};
}

namespace
{
// pro Spur: Grundstimmung (Halbtoene), Filtertyp (0 aus, 1 Tiefpass, 2 Hochpass), Frequenz
const int   kBase[kLanes]  = { -3, 0, 2, 0, 0 };
const int   kFType[kLanes] = { 1, 2, 2, 0, 1 };
const float kFreq[kLanes]  = { 2600.f, 180.f, 2500.f, 0.f, 7000.f };
const float kDefVol[kLanes] = { .9f, .75f, .55f, .55f, .6f };
const float kDefLen[kLanes] = { .6f, .5f, .25f, .5f, .7f };
const char* kLaneIds[kLanes] = { "kick", "snare", "hat", "perc", "mel" };
}

juce::AudioProcessorValueTreeState::ParameterLayout FundstueckProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; });
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "bpm", 1 }, "Tempo (intern)", NormalisableRange<float> (60.f, 170.f, 1.f), 90.f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " BPM"; })));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "sync", 1 }, "Host-Sync", false));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "swing", 1 }, "Swing", NormalisableRange<float> (50.f, 75.f, 1.f), 58.f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "feel", 1 }, "Feel", NormalisableRange<float> (0.f, 100.f, 1.f), 30.f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "rev", 1 }, "Raum", NormalisableRange<float> (0.f, 100.f, 1.f), 20.f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tape", 1 }, "Tape", NormalisableRange<float> (0.f, 100.f, 1.f), 35.f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "sub", 1 }, "Sub-Kick", NormalisableRange<float> (0.f, 100.f, 1.f), 40.f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "master", 1 }, "Master", NormalisableRange<float> (0.f, 100.f, 1.f), 80.f, pct));
    const char* names[kLanes] = { "Kick", "Snare", "Hat", "Perc", "Melodie" };
    for (int i = 0; i < kLanes; ++i)
    {
        const String id (kLaneIds[i]), nm (names[i]);
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id + "_vol", 1 }, nm + " Vol", NormalisableRange<float> (0.f, 1.f, 0.01f), kDefVol[i],
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100)); })));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id + "_tune", 1 }, nm + " Tune", NormalisableRange<float> (-12.f, 12.f, 1.f), 0.f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { const int n = roundToInt (v); return (n > 0 ? "+" : "") + String (n); })));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id + "_len", 1 }, nm + " Laenge", NormalisableRange<float> (0.05f, 1.f, 0.01f), kDefLen[i],
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100)); })));
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id + "_mute", 1 }, nm + " Stumm", false));
    }
    return { p.begin(), p.end() };
}

FundstueckProcessor::FundstueckProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Eingang", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int l = 0; l < kLanes; ++l)
    {
        laneSlice[l] = -1;
        for (int s = 0; s < kSteps; ++s) steps[l][s] = (l == kMel ? -1 : 0);
        const juce::String id (kLaneIds[l]);
        pVol[l] = apvts.getRawParameterValue (id + "_vol");
        pTune[l] = apvts.getRawParameterValue (id + "_tune");
        pLen[l] = apvts.getRawParameterValue (id + "_len");
        pMute[l] = apvts.getRawParameterValue (id + "_mute");
    }
    pBpm = apvts.getRawParameterValue ("bpm");   pSwing = apvts.getRawParameterValue ("swing");
    pFeel = apvts.getRawParameterValue ("feel"); pRev = apvts.getRawParameterValue ("rev");
    pTape = apvts.getRawParameterValue ("tape"); pSub = apvts.getRawParameterValue ("sub");
    pMaster = apvts.getRawParameterValue ("master");
    pSync = apvts.getRawParameterValue ("sync");
    loadDemo();
}

bool FundstueckProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    if (o != juce::AudioChannelSet::stereo() && o != juce::AudioChannelSet::mono()) return false;
    for (auto& in : l.inputBuses)
        if (! in.isDisabled() && in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    return true;
}

void FundstueckProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    mix.assign ((size_t) juce::jmax (samplesPerBlock, 512) * 2, 0.0f);
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    for (auto& v : voices) v.active = false;
    numEvents = 0;
    expectedPpq = -1.0;
}

// ======================================================================
//  Audio-Thread
// ======================================================================
void FundstueckProcessor::pushEvent (const Event& e)
{
    if (numEvents < kMaxEvents) events[numEvents++] = e;
}

void FundstueckProcessor::scheduleStep (int step, juce::int64 when, double stepS)
{
    const float sw = (pSwing->load() - 50.f) / 50.f, feel = pFeel->load() / 100.f;
    const juce::int64 t = when + (step % 2 ? (juce::int64) (sw * stepS * 0.66) : 0);
    Event mk; mk.when = t; mk.lane = -1; mk.val = step; pushEvent (mk);
    for (int l = 0; l < kLanes; ++l)
    {
        if (pMute[l]->load() > 0.5f) continue;
        const int val = steps[l][step].load();
        if (l == kMel ? val < 0 : val == 0) continue;
        const float acc = step % 4 == 0 ? 1.0f : (step % 2 == 0 ? .85f : .72f);
        const auto jitter = (juce::int64) ((audioRand.nextFloat() - .5f) * .012f * feel * (float) sr);
        Event e; e.lane = l; e.vel = acc; e.val = val;
        if (l == 2 && val > 1)
        {
            for (int r = 0; r < val; ++r)
            {
                e.when = t + jitter + (juce::int64) (r * stepS / val);
                e.vel = acc * (r ? .7f : 1.f); e.val = 1;
                pushEvent (e);
            }
        }
        else { e.when = t + jitter; pushEvent (e); }
    }
}

void FundstueckProcessor::startVoice (const Slice& s, int lane, double rate, float gain, float len, int ftype, float freq)
{
    if (s.data.size() < 2 || rate <= 0) return;
    Voice* v = nullptr; int oldest = -1;
    for (auto& c : voices) if (! c.active) { v = &c; break; }
    if (v == nullptr)
        for (auto& c : voices) if (c.age > oldest) { oldest = c.age; v = &c; }
    *v = Voice();
    v->active = true; v->data = s.data.data(); v->length = (int) s.data.size();
    v->rate = rate; v->gain = gain; v->lane = lane;
    const double full = (double) v->length / rate;
    const double lenS = juce::jmax (0.03 * sr, (double) len * full);
    v->hold = (int) (lenS * 0.7);
    const double tau = lenS * 0.12 / sr + 0.004;
    v->coef = (float) std::exp (-1.0 / (tau * sr));
    v->ftype = ftype;
    if (ftype > 0)
    {
        const double fc = juce::jmin ((double) freq, sr * 0.45);
        const float g = (float) std::tan (kPi * fc / sr);
        v->k = 1.414f;
        v->a1 = 1.0f / (1.0f + g * (g + v->k)); v->a2 = g * v->a1; v->a3 = g * v->a2;
    }
}

void FundstueckProcessor::startSub (float gain)
{
    const float amt = pSub->load() / 100.f;
    if (amt <= 0.01f) return;
    Voice* v = nullptr;
    for (auto& c : voices) if (! c.active) { v = &c; break; }
    if (v == nullptr) return;
    *v = Voice();
    v->active = true; v->isSub = true; v->gain = gain * amt * .9f; v->lane = 0;
    v->hold = (int) (0.05 * sr);
    v->coef = (float) std::exp (-1.0 / (0.09 * sr));
}

void FundstueckProcessor::fire (const Event& e)
{
    if (e.lane == -1) { currentStep = e.val; return; }
    auto* set = audioSet.get();
    if (set == nullptr) return;
    const int si = e.slice >= 0 ? e.slice : (e.lane >= 0 ? laneSlice[e.lane].load() : -1);
    if (si < 0 || si >= (int) set->slices.size()) return;
    const Slice& s = set->slices[(size_t) si];
    const double ratio = set->sampleRate / sr;

    if (e.lane == -2) { startVoice (s, -2, ratio, .8f, 1.f, 0, 0.f); return; }

    const int l = e.lane;
    const float feel = pFeel->load() / 100.f;
    const float vel = e.vel * (1.f - feel * .25f * audioRand.nextFloat());
    const float vol = pVol[l]->load(), tune = pTune[l]->load(), len = pLen[l]->load();

    if (l == kMel)
    {
        const int note = e.val + (e.seq ? melOct.load() : 0);
        const double rate = std::pow (2.0, (note - s.midi + tune) / 12.0) * ratio;
        const float rel = (float) std::exp (-1.0 / (0.012 * sr));
        for (auto& v : voices)
            if (v.active && v.lane == kMel && ! v.releasing) { v.releasing = true; v.relCoef = rel; }
        startVoice (s, l, rate, vol * vel, len, kFType[l], kFreq[l]);
        return;
    }
    const double rate = std::pow (2.0, (kBase[l] + tune) / 12.0) * ratio;
    startVoice (s, l, rate, vol * vel, e.slice >= 0 ? juce::jmax (len, .4f) : len, kFType[l], kFreq[l]);
    if (l == 0) startSub (vol * vel);
}

void FundstueckProcessor::render (int from, int to)
{
    if (to <= from) return;
    const float atk = (float) (0.004 * sr);
    for (auto& v : voices)
    {
        if (! v.active) continue;
        for (int n = from; n < to; ++n)
        {
            float s;
            if (v.isSub)
            {
                const double t = v.age / sr;
                const double f = 48.0 + 92.0 * std::exp (-t * 35.0);
                v.phase += f / sr; if (v.phase >= 1.0) v.phase -= 1.0;
                s = (float) std::sin (2.0 * kPi * v.phase) * juce::jmin (1.0f, (float) v.age / atk);
            }
            else
            {
                const int i0 = (int) v.pos;
                if (i0 + 1 >= v.length) { v.active = false; break; }
                const float fr = (float) (v.pos - i0);
                s = v.data[i0] + fr * (v.data[i0 + 1] - v.data[i0]);
                v.pos += v.rate;
                if (v.ftype > 0)
                {
                    const float v3 = s - v.ic2;
                    const float v1 = v.a1 * v.ic1 + v.a2 * v3;
                    const float v2 = v.ic2 + v.a2 * v.ic1 + v.a3 * v3;
                    v.ic1 = 2 * v1 - v.ic1; v.ic2 = 2 * v2 - v.ic2;
                    s = v.ftype == 1 ? v2 : s - v.k * v1 - v2;
                }
            }
            if (v.releasing) v.env *= v.relCoef;
            else if (v.age >= v.hold) v.env *= v.coef;
            mix[(size_t) n] += s * v.env * v.gain;
            ++v.age;
            if (v.env < 1.0e-4f) { v.active = false; break; }
        }
    }
}

void FundstueckProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int N = buffer.getNumSamples();

    // Eingaenge (Haupt + Sidechain) zu Mono summieren: Pegel + Aufnahme
    {
        float pk = 0.0f;
        const bool rec = recording.load();
        int pos = recPos.load();
        const int cap = (int) recBuf.size();
        int nIn = 0;
        for (int b = 0; b < getBusCount (true); ++b)
            if (auto* bus = getBus (true, b); bus != nullptr && bus->isEnabled()) nIn += bus->getNumberOfChannels();
        nIn = juce::jmin (nIn, buffer.getNumChannels());
        if (nIn > 0)
        {
            for (int n = 0; n < N; ++n)
            {
                float x = 0.0f;
                for (int c = 0; c < nIn; ++c) x += buffer.getSample (c, n);
                x /= (float) juce::jmin (nIn, 2);
                pk = juce::jmax (pk, std::abs (x));
                if (rec && pos < cap) recBuf[(size_t) pos++] = x;
            }
            if (rec) { recPos = pos; if (pos >= cap) { recording = false; recFull = true; } }
        }
        inLevel = juce::jmax (pk, inLevel.load() * 0.85f);
    }
    buffer.clear();
    if ((int) mix.size() < N) mix.resize ((size_t) N * 2);
    std::fill (mix.begin(), mix.begin() + N, 0.0f);

    // neues Schnipsel-Set uebernehmen (Freigabe passiert im Message-Thread)
    if (setPending.load())
    {
        const juce::SpinLock::ScopedTryLockType tl (setLock);
        if (tl.isLocked())
        {
            std::swap (audioSet, nextSet);
            setPending = false;
            for (auto& v : voices) v.active = false;
            numEvents = 0;
        }
    }

    // Vorhoeren aus der Oberflaeche
    {
        const auto scope = uiFifo.read (uiFifo.getNumReady());
        for (int i = 0; i < scope.blockSize1; ++i) { auto e = uiEvents[scope.startIndex1 + i]; e.when = clock; pushEvent (e); }
        for (int i = 0; i < scope.blockSize2; ++i) { auto e = uiEvents[scope.startIndex2 + i]; e.when = clock; pushEvent (e); }
    }

    // Transport
    const bool sync = pSync->load() > 0.5f;
    syncOn = sync;
    bool hp = false; double ppq = 0, bpm = pBpm->load(), hb = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            hp = pos->getIsPlaying();
            if (auto b = pos->getBpm()) hb = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p;
        }
    hostBpm = hb;
    if (! sync) hp = false;            // ohne Sync: Host-Transport ignorieren
    else if (hb > 0) bpm = hb;         // mit Sync: Tempo vom Host
    hostPlaying = hp;
    bpm = juce::jlimit (20.0, 400.0, bpm);
    curBpm = bpm;

    bool run = false; double ppqStart = 0;
    if (hp) { run = true; ppqStart = ppq; wasInternal = false; }
    else if (! sync && internalPlay.load())
    {
        if (! wasInternal) { internalPpq = 0; wasInternal = true; expectedPpq = -1; }
        run = true; ppqStart = internalPpq;
    }
    else wasInternal = false;

    const double spq = sr * 60.0 / bpm, stepS = spq / 4.0;
    if (run)
    {
        const double ppqEnd = ppqStart + N / spq;
        if (! wasRunning || std::abs (ppqStart - expectedPpq) > 0.01)
            nextK = juce::jmax ((juce::int64) 0, (juce::int64) std::ceil (ppqStart * 4.0 - 1.0e-6));
        while ((double) nextK * 0.25 < ppqEnd)
        {
            const double off = juce::jmax (0.0, ((double) nextK * 0.25 - ppqStart) * spq);
            scheduleStep ((int) (nextK % kSteps), clock + (juce::int64) off, stepS);
            ++nextK;
        }
        expectedPpq = ppqEnd;
        if (! hp) internalPpq = ppqEnd;
    }
    else if (wasRunning) currentStep = -1;
    wasRunning = run;

    // MIDI: 36 Kick, 38/40 Snare, 42/44/46 Hat, 37/39 Perc, ab 48 Melodie
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (! msg.isNoteOn()) continue;
        const int nn = msg.getNoteNumber();
        Event e; e.when = clock + m.samplePosition; e.vel = msg.getFloatVelocity(); e.seq = false;
        if (nn >= 48) { e.lane = kMel; e.val = nn; }
        else if (nn == 36 || nn == 35) e.lane = 0;
        else if (nn == 38 || nn == 40) e.lane = 1;
        else if (nn == 42 || nn == 44 || nn == 46) e.lane = 2;
        else if (nn == 37 || nn == 39 || nn == 41 || nn == 43 || nn == 45 || nn == 47) e.lane = 3;
        else continue;
        pushEvent (e);
    }
    midi.clear();

    // faellige Ereignisse sortieren und sampelgenau ausloesen
    int due[kMaxEvents]; int nd = 0;
    for (int i = 0; i < numEvents; ++i) if (events[i].when < clock + N) due[nd++] = i;
    std::sort (due, due + nd, [this] (int a, int b) { return events[a].when < events[b].when; });
    int pos = 0;
    for (int d = 0; d < nd; ++d)
    {
        const auto& e = events[due[d]];
        const int off = (int) juce::jlimit ((juce::int64) 0, (juce::int64) N, e.when - clock);
        if (off > pos) { render (pos, off); pos = off; }
        fire (e);
    }
    render (pos, N);
    int w = 0;
    for (int i = 0; i < numEvents; ++i) if (events[i].when >= clock + N) events[w++] = events[i];
    numEvents = w;

    // Klangkette: Tape -> Master -> Raum -> Begrenzer
    // Tape: Kleinsignal-Verstaerkung 1, Spitzen werden weich gerundet
    const float drive = 1.f + pTape->load() / 100.f * 4.f;
    const float mg = pMaster->load() / 100.f;
    const int nch = buffer.getNumChannels();
    for (int n = 0; n < N; ++n)
    {
        float x = mix[(size_t) n] * 0.5f;
        if (drive > 1.05f) x = std::tanh (x * drive) / drive * (1.f + (drive - 1.f) * 0.35f);
        x *= mg;
        for (int c = 0; c < nch; ++c) buffer.setSample (c, n, x);
    }
    juce::Reverb::Parameters rp;
    rp.roomSize = .72f; rp.damping = .45f; rp.width = 1.f; rp.dryLevel = 1.f;
    rp.wetLevel = pRev->load() / 100.f * .45f;
    reverb.setParameters (rp);
    if (nch >= 2) reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), N);
    else if (nch == 1) reverb.processMono (buffer.getWritePointer (0), N);
    for (int c = 0; c < nch; ++c)
    {
        auto* d = buffer.getWritePointer (c);
        for (int n = 0; n < N; ++n)
        {
            const float a = std::abs (d[n]);
            if (a > 0.8f) d[n] = std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), d[n]);
        }
    }
    clock += N;
}

// ======================================================================
//  Message-Thread
// ======================================================================
void FundstueckProcessor::loadDemo()
{
    sourceSR = 44100.0;
    source = makeDemo (sourceSR);
    sourceLabel = juce::String (juce::CharPointer_UTF8 ("Demo-Ger\xc3\xa4usche"));
    reanalyze (Mode::Fresh);
}

bool FundstueckProcessor::loadFiles (const juce::StringArray& paths)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::vector<float> out;
    double outSR = 0;
    int loaded = 0;
    const double maxSec = 120.0;
    for (auto& p : paths)
    {
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File (p)));
        if (r == nullptr || r->lengthInSamples <= 0) continue;
        if (outSR == 0) outSR = r->sampleRate;
        const auto len = (int) juce::jmin<juce::int64> (r->lengthInSamples, (juce::int64) (maxSec * r->sampleRate));
        juce::AudioBuffer<float> b ((int) r->numChannels, len);
        r->read (&b, 0, len, 0, true, true);
        std::vector<float> mono ((size_t) len, 0.f);
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < len; ++i) mono[(size_t) i] += b.getSample (c, i) / (float) b.getNumChannels();
        if (r->sampleRate != outSR) // einfach linear umrechnen
        {
            const double ratio = r->sampleRate / outSR;
            std::vector<float> rs ((size_t) (len / ratio));
            for (size_t i = 0; i < rs.size(); ++i)
            {
                const double x = i * ratio; const size_t i0 = (size_t) x; const float fr = (float) (x - i0);
                rs[i] = mono[i0] + fr * ((i0 + 1 < mono.size() ? mono[i0 + 1] : mono[i0]) - mono[i0]);
            }
            mono.swap (rs);
        }
        if (! out.empty()) out.insert (out.end(), (size_t) (0.25 * outSR), 0.f);
        out.insert (out.end(), mono.begin(), mono.end());
        ++loaded;
        if ((double) out.size() > maxSec * outSR) { out.resize ((size_t) (maxSec * outSR)); break; }
    }
    if (loaded == 0 || out.empty())
    {
        statusText = juce::String (juce::CharPointer_UTF8 ("Datei konnte nicht gelesen werden \xe2\x80\x93 probier WAV, AIFF, MP3 oder M4A."));
        bump();
        return false;
    }
    float pk = 1.0e-9f; for (auto v : out) pk = juce::jmax (pk, std::abs (v));
    for (auto& v : out) v /= pk;
    source.swap (out);
    sourceSR = outSR;
    sourceLabel = loaded == 1 ? juce::File (paths[0]).getFileName() : juce::String (loaded) + " Dateien";
    reanalyze (Mode::Fresh);
    return true;
}

void FundstueckProcessor::startRecording()
{
    if (recording.load()) return;
    const size_t need = (size_t) (120.0 * juce::jmax (sr, 44100.0));
    if (recBuf.size() < need) recBuf.assign (need, 0.0f);   // nur wachsen, nie waehrend der Aufnahme
    recFull = false;
    recPos = 0;
    recording = true;
    statusText = juce::String (juce::CharPointer_UTF8 ("Nimmt auf\xe2\x80\xa6 klopf, klapper, summ!"));
    bump();
}

void FundstueckProcessor::stopRecording()
{
    recording = false;
    recFull = false;
    const int n = juce::jmin (recPos.load(), (int) recBuf.size());
    // Stille am Anfang/Ende abschneiden
    float pk = 0; for (int i = 0; i < n; ++i) pk = juce::jmax (pk, std::abs (recBuf[(size_t) i]));
    if (n < (int) (0.2 * sr) || pk < 1.0e-3f)
    {
        statusText = juce::String (juce::CharPointer_UTF8 ("Kein Signal aufgenommen \xe2\x80\x93 Eingang/Sidechain pr\xc3\xbc" "fen (siehe Pegel)."));
        bump();
        return;
    }
    const float thr = pk * 0.02f;
    int a = 0, b = n;
    while (a < n && std::abs (recBuf[(size_t) a]) < thr) ++a;
    while (b > a && std::abs (recBuf[(size_t) b - 1]) < thr) --b;
    a = juce::jmax (0, a - (int) (0.02 * sr));
    b = juce::jmin (n, b + (int) (0.3 * sr));
    std::vector<float> out (recBuf.begin() + a, recBuf.begin() + b);
    for (auto& v : out) v /= pk;
    source.swap (out);
    sourceSR = sr;
    sourceLabel = "Aufnahme";
    reanalyze (Mode::Fresh);
}

void FundstueckProcessor::reanalyze (Mode mode)
{
    auto set = std::make_shared<SliceSet>();
    set->sampleRate = sourceSR;
    if (! source.empty()) set->slices = analyze (source, sourceSR, sens);
    msgSet = set;
    {
        const juce::SpinLock::ScopedLockType sl (setLock);
        nextSet = set;
        setPending = true;
    }
    const auto secs = juce::String ((double) source.size() / sourceSR, 1);
    if (set->slices.empty())
        statusText = sourceLabel + juce::String (juce::CharPointer_UTF8 (": keine klaren Treffer \xe2\x80\x93 Empfindlichkeit erh\xc3\xb6hen."));
    else
        statusText = sourceLabel + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + secs + juce::String (juce::CharPointer_UTF8 (" s \xc2\xb7 "))
                     + juce::String ((int) set->slices.size()) + " Schnipsel";
    if (mode != Mode::Restore) autoAssign (false);
    if (mode == Mode::Fresh) { rollDrums(); rollMel(); }
    bump();
}

void FundstueckProcessor::autoAssign (bool random)
{
    const auto& S = msgSet->slices;
    if (S.empty()) { for (auto& l : laneSlice) l = -1; bump(); return; }
    std::vector<bool> used (S.size(), false); size_t nUsed = 0;
    auto pick = [&] (auto fn, bool avoid)
    {
        int best = 0; float bs = -1.0e30f;
        for (size_t i = 0; i < S.size(); ++i)
        {
            if (avoid && used[i] && nUsed < S.size()) continue;
            const float sc = fn (S[i]) + (random ? msgRand.nextFloat() * 1.2f : 0.f);
            if (sc > bs) { bs = sc; best = (int) i; }
        }
        if (! used[(size_t) best]) { used[(size_t) best] = true; ++nUsed; }
        return best;
    };
    laneSlice[0] = pick ([] (const Slice& s) { return s.low * 3 - s.centroid / 2500.f + s.rmsN * .6f; }, true);
    laneSlice[2] = pick ([] (const Slice& s) { return s.centroid / 2000.f - s.decay * 2; }, true);
    laneSlice[1] = pick ([] (const Slice& s) { return -std::abs (std::log ((s.centroid + 1) / 1800.f)) + s.zcr * 3 + s.rmsN * .5f; }, true);
    laneSlice[3] = pick ([this] (const Slice& s) { return s.rmsN + msgRand.nextFloat() * .4f; }, true);
    laneSlice[4] = pick ([] (const Slice& s) { return s.pconf * 2 + juce::jmin (s.decay, .6f) - (s.f0 > 0 ? 0.f : 2.f); }, false);
    updateMelOct();
    bump();
}

void FundstueckProcessor::updateMelOct()
{
    const int si = laneSlice[kMel].load();
    if (msgSet == nullptr || si < 0 || si >= (int) msgSet->slices.size()) { melOct = 0; return; }
    const float midi = msgSet->slices[(size_t) si].midi;
    melOct = (int) std::round ((midi - (60 + root) - 4) / 12.f) * 12;
}

void FundstueckProcessor::rollDrums()
{
    const auto& st = kStyles[style];
    const float sc = .4f + density * 1.2f;
    auto f = [&] (float p) { return p >= .95f ? 1 : (msgRand.nextFloat() < p * sc ? 1 : 0); };
    for (int s = 0; s < kSteps; ++s)
    {
        steps[0][s] = f (st.kick[s]);
        steps[1][s] = f (st.snare[s]);
        int h = f (st.hat[s]);
        if (h && msgRand.nextFloat() < st.ratchet * (.5f + density)) h = msgRand.nextFloat() < .6f ? 2 : 3;
        steps[2][s] = h;
        steps[3][s] = f (st.perc[s] * (.6f + density));
    }
    steps[0][0] = 1;
    bump();
}

std::vector<int> FundstueckProcessor::scaleNotes() const
{
    std::vector<int> n;
    const auto& sc = kScales[scale];
    for (int oc : { -12, 0, 12 })
        for (int i = 0; i < sc.n; ++i) { const int m = 60 + root + oc + sc.deg[i]; if (m >= 52 && m <= 86) n.push_back (m); }
    std::sort (n.begin(), n.end());
    return n;
}

void FundstueckProcessor::rollMel()
{
    const auto N = scaleNotes();
    const int rootNote = 60 + root, last = (int) N.size() - 1;
    auto idxOf = [&] (int note) { for (size_t i = 0; i < N.size(); ++i) if (N[i] == note) return (int) i; return 0; };
    int deg = idxOf (rootNote);
    int motif[8];
    const int stepsDeg[7] = { -2, -1, -1, 0, 1, 1, 2 };
    for (int i = 0; i < 8; ++i)
    {
        const bool on = i == 0 || msgRand.nextFloat() < (i % 2 == 0 ? .6f : .28f) * (.5f + density);
        if (on)
        {
            if (i > 0) deg += stepsDeg[msgRand.nextInt (7)];
            deg = juce::jlimit (0, last, deg);
            motif[i] = N[(size_t) deg];
        }
        else motif[i] = -1;
    }
    for (int i = 0; i < 8; ++i) { steps[kMel][i] = motif[i]; steps[kMel][i + 8] = motif[i]; }
    for (int i = 13; i < 16; ++i)
        if (msgRand.nextFloat() < .6f)
            steps[kMel][i] = msgRand.nextFloat() < .5f ? N[(size_t) juce::jlimit (0, last, idxOf (rootNote) + msgRand.nextInt (5) - 1)] : -1;
    updateMelOct();
    bump();
}

void FundstueckProcessor::clearPattern()
{
    for (int l = 0; l < kLanes; ++l) for (int s = 0; s < kSteps; ++s) steps[l][s] = (l == kMel ? -1 : 0);
    bump();
}

void FundstueckProcessor::setStyle (int s)
{
    style = juce::jlimit (0, 5, s);
    auto setP = [this] (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    setP ("bpm", kStyles[style].bpm);
    setP ("swing", kStyles[style].swing);
    rollDrums();
}

void FundstueckProcessor::setLaneSlice (int lane, int idx)
{
    laneSlice[lane] = idx;
    if (lane == kMel) updateMelOct();
    bump();
}

void FundstueckProcessor::cycleSlice (int lane, int dir)
{
    const int n = (int) msgSet->slices.size();
    if (n == 0) return;
    const int cur = juce::jmax (0, laneSlice[lane].load());
    setLaneSlice (lane, ((cur + dir) % n + n) % n);
    audition (laneSlice[lane].load(), lane);
}

void FundstueckProcessor::audition (int slice, int lane)
{
    if (msgSet == nullptr || slice < 0 || slice >= (int) msgSet->slices.size()) return;
    Event e; e.lane = lane < 0 ? -2 : lane; e.slice = slice; e.vel = .9f; e.seq = false;
    if (lane == kMel) e.val = (int) std::round (msgSet->slices[(size_t) slice].midi);
    const auto scope = uiFifo.write (1);
    if (scope.blockSize1 > 0) uiEvents[scope.startIndex1] = e;
    else if (scope.blockSize2 > 0) uiEvents[scope.startIndex2] = e;
}

void FundstueckProcessor::stepClick (int lane, int step, int mod)
{
    auto& cell = steps[lane][step];
    if (lane == kMel)
    {
        if (cell.load() >= 0)
        {
            if (mod != 0) { shiftNote (lane, step, mod); return; }
            cell = -1;
        }
        else
        {
            int prev = -1;
            for (int k = step - 1; k >= 0; --k) if (steps[kMel][k].load() >= 0) { prev = steps[kMel][k].load(); break; }
            cell = prev >= 0 ? prev : 60 + root;
            Event e; e.lane = kMel; e.val = cell.load(); e.vel = .9f; e.seq = true;
            const auto scope = uiFifo.write (1);
            if (scope.blockSize1 > 0) uiEvents[scope.startIndex1] = e;
        }
    }
    else if (lane == 2) cell = (cell.load() + 1) % 4;
    else cell = cell.load() ? 0 : 1;
    bump();
}

void FundstueckProcessor::shiftNote (int lane, int step, int dir)
{
    auto& cell = steps[lane][step];
    if (cell.load() < 0) return;
    const auto N = scaleNotes();
    int k = (int) N.size() - 1;
    for (size_t i = 0; i < N.size(); ++i) if (N[i] >= cell.load()) { k = (int) i; break; }
    k = juce::jlimit (0, (int) N.size() - 1, k + dir);
    cell = N[(size_t) k];
    Event e; e.lane = kMel; e.val = N[(size_t) k]; e.vel = .9f; e.seq = true;
    const auto scope = uiFifo.write (1);
    if (scope.blockSize1 > 0) uiEvents[scope.startIndex1] = e;
    bump();
}

// ======================================================================
//  Zustand speichern / laden (inkl. Quell-Audio, damit Projekte wieder klingen)
// ======================================================================
void FundstueckProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree st ("FundstueckState");
    st.appendChild (apvts.copyState(), nullptr);
    st.setProperty ("style", style, nullptr);
    st.setProperty ("density", density, nullptr);
    st.setProperty ("root", root, nullptr);
    st.setProperty ("scale", scale, nullptr);
    st.setProperty ("sens", sens, nullptr);
    st.setProperty ("label", sourceLabel, nullptr);
    juce::String pat, sl;
    for (int l = 0; l < kLanes; ++l)
    {
        for (int s = 0; s < kSteps; ++s) pat << steps[l][s].load() << ",";
        sl << laneSlice[l].load() << ",";
    }
    st.setProperty ("pattern", pat, nullptr);
    st.setProperty ("slices", sl, nullptr);
    if (! source.empty())
    {
        juce::MemoryBlock mb (source.size() * sizeof (juce::int16));
        auto* p = static_cast<juce::int16*> (mb.getData());
        for (size_t i = 0; i < source.size(); ++i)
            p[i] = (juce::int16) juce::jlimit (-32767, 32767, juce::roundToInt (source[i] * 32767.f));
        st.setProperty ("audio", juce::var (mb), nullptr);
        st.setProperty ("audioSR", sourceSR, nullptr);
    }
    juce::MemoryOutputStream mos (dest, false);
    st.writeToStream (mos);
}

void FundstueckProcessor::setStateInformation (const void* data, int size)
{
    auto st = juce::ValueTree::readFromData (data, (size_t) size);
    if (! st.isValid() || ! st.hasType ("FundstueckState")) return;
    auto params = st.getChildWithName (apvts.state.getType());
    if (params.isValid()) apvts.replaceState (params);
    style = juce::jlimit (0, 5, (int) st.getProperty ("style", 0));
    density = (float) st.getProperty ("density", .5f);
    root = juce::jlimit (0, 11, (int) st.getProperty ("root", 9));
    scale = juce::jlimit (0, 4, (int) st.getProperty ("scale", 0));
    sens = (float) st.getProperty ("sens", .55f);
    sourceLabel = st.getProperty ("label", "").toString();
    if (auto* mb = st.getProperty ("audio").getBinaryData())
    {
        const auto n = mb->getSize() / sizeof (juce::int16);
        const auto* p = static_cast<const juce::int16*> (mb->getData());
        source.assign (n, 0.f);
        for (size_t i = 0; i < n; ++i) source[i] = p[i] / 32767.f;
        sourceSR = (double) st.getProperty ("audioSR", 44100.0);
    }
    reanalyze (Mode::Restore);
    juce::StringArray pat, sl;
    pat.addTokens (st.getProperty ("pattern").toString(), ",", "");
    sl.addTokens (st.getProperty ("slices").toString(), ",", "");
    for (int l = 0; l < kLanes; ++l)
    {
        for (int s = 0; s < kSteps; ++s)
        {
            const int i = l * kSteps + s;
            if (i < pat.size() && pat[i].isNotEmpty()) steps[l][s] = pat[i].getIntValue();
        }
        if (l < sl.size() && sl[l].isNotEmpty()) laneSlice[l] = sl[l].getIntValue();
    }
    updateMelOct();
    bump();
}

juce::AudioProcessorEditor* FundstueckProcessor::createEditor() { return new FundstueckEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FundstueckProcessor(); }
