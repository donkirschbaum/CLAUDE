#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

namespace g8
{
static const float DEF_DENS[NT] = { .55f, .35f, .3f, .6f, .3f, .15f, .12f, .2f };
static const float DENS_RANGE[NT][2] = { {.35f,.8f},{.15f,.6f},{.1f,.5f},{.3f,.85f},{.1f,.55f},{0,.4f},{0,.35f},{0,.5f} };
static const int AF_EVERY[3] = { 2, 4, 8 };
static const int EV_EVERY[4] = { 1, 2, 4, 8 };

static juce::StringArray toSA (const char* const* arr, int n) { juce::StringArray s; for (int i = 0; i < n; ++i) s.add (arr[i]); return s; }

static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout L;
    auto F = [&] (const String& id, const String& name, float lo, float hi, float def)
    { L.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi), def)); };
    auto I = [&] (const String& id, const String& name, int lo, int hi, int def)
    { L.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };
    auto B = [&] (const String& id, const String& name, bool def)
    { L.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto C = [&] (const String& id, const String& name, StringArray items, int def)
    { L.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };

    F ("mX", "Master X", 0, 1, .5f);  F ("mY", "Master Y", 0, 1, .5f);
    F ("mDens", "Master Dichte", -.5f, .5f, 0); F ("mChaos", "Master Chaos", 0, 1, 0);
    F ("mVol", "Master Volume", 0, 1, .85f);
    C ("mShape", "Master Mod Form", toSA (SHAPE_NAMES, 6), 0); C ("mRate", "Master Mod Rate", toSA (RATE_NAMES, 7), 5);
    F ("mDx", "Master Mod Tiefe X", 0, 1, .3f); F ("mDy", "Master Mod Tiefe Y", 0, 1, .3f); F ("mDd", "Master Mod Tiefe Dichte", 0, 1, 0);
    F ("swing", "Swing", 50, 75, 50); F ("bpm", "Tempo (ohne Host)", 40, 240, 122);
    F ("hTime", "Humanize Timing", 0, 1, 0); F ("hVel", "Humanize Velocity", 0, 1, .15f);
    B ("afOn", "Auto-Fill", false); C ("afEvery", "Auto-Fill alle", { "2 Takte", "4 Takte", "8 Takte" }, 1);
    B ("evOn", "Evolve", false); C ("evEvery", "Evolve alle", { "1 Takt", "2 Takte", "4 Takte", "8 Takte" }, 2);
    B ("choke", "Hat-Choke 3/4", true); B ("sound", "Interne Sounds", true);
    C ("noteMap", "Notenbelegung", { "Drum Rack C1-G1", "General MIDI" }, 0);
    C ("midiCh", "MIDI-Kanal", { "Kanal 10", "Kanal 1-8 je Spur" }, 0);
    F ("rndAmt", juce::String::fromUTF8 ("Zufall Stärke"), .05f, 1, .35f);

    for (int t = 0; t < NT; ++t)
    {
        auto id = [t] (const char* k) { return Grids8Processor::tid (t, k); };
        const String n = String (t + 1) + " " + TRACKS[t].name + " ";
        F (id ("x"), n + "X", 0, 1, .5f); F (id ("y"), n + "Y", 0, 1, .5f);
        F (id ("dens"), n + "Dichte", 0, 1, DEF_DENS[t]); F (id ("chaos"), n + "Chaos", 0, 1, 0);
        F (id ("vol"), n + "Volume", 0, 1, .8f); I (id ("pitch"), n + "Pitch", -12, 12, 0);
        F (id ("decay"), n + "Decay", .05f, 1, 1); F (id ("pan"), n + "Pan", -1, 1, 0);
        I (id ("len"), n + juce::String::fromUTF8 ("Länge"), 1, 32, 32);
        B (id ("mute"), n + "Mute", false); B (id ("solo"), n + "Solo", false);
        C (id ("mShape"), n + "Mod Form", toSA (SHAPE_NAMES, 6), 0); C (id ("mRate"), n + "Mod Rate", toSA (RATE_NAMES, 7), 4);
        F (id ("mDx"), n + "Mod Tiefe X", 0, 1, .3f); F (id ("mDy"), n + "Mod Tiefe Y", 0, 1, .3f); F (id ("mDd"), n + "Mod Tiefe Dichte", 0, 1, 0);
    }
    return L;
}

Grids8Processor::Grids8Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    auto P = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };
    mX = P ("mX"); mY = P ("mY"); mDens = P ("mDens"); mChaos = P ("mChaos"); mVol = P ("mVol");
    mShape = P ("mShape"); mRate = P ("mRate"); mDx = P ("mDx"); mDy = P ("mDy"); mDd = P ("mDd");
    swing = P ("swing"); intBpm = P ("bpm"); hTime = P ("hTime"); hVel = P ("hVel"); afOn = P ("afOn"); afEvery = P ("afEvery");
    choke = P ("choke"); sound = P ("sound"); noteMap = P ("noteMap"); midiCh = P ("midiCh"); evOn = P ("evOn"); evEvery = P ("evEvery"); rndAmt = P ("rndAmt");
    for (int t = 0; t < NT; ++t)
    {
        auto Q = [&] (const char* k) { return P (tid (t, k)); };
        tp[(size_t) t] = { Q ("x"), Q ("y"), Q ("dens"), Q ("chaos"), Q ("vol"), Q ("pitch"), Q ("decay"), Q ("pan"), Q ("len"),
                           Q ("mute"), Q ("solo"), Q ("mShape"), Q ("mRate"), Q ("mDx"), Q ("mDy"), Q ("mDd") };
    }
    const int lens[8] = { 1, 2, 4, 8, 16, 4, 8, 16 };
    for (int i = 0; i < 8; ++i) cfg[(size_t) i] = { lens[i], false };
    for (int t = 0; t < NT; ++t) userSamplePaths.add ({});
    maps[0].generate (seed.load());
    maps[1] = maps[0];
    for (auto& tr : perturb) for (auto& v : tr) v = rng.nextFloat();
    pending.reserve (2048);
    loadDefaultSamples();
    startTimerHz (30);
}

Grids8Processor::~Grids8Processor() { stopTimer(); }

void Grids8Processor::loadDefaultSamples()
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    for (int t = 0; t < NT; ++t)
    {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            if (juce::String (BinaryData::originalFilenames[i]) != TRACKS[t].file) continue;
            int size = 0;
            auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
            if (data == nullptr) break;
            std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (std::make_unique<juce::MemoryInputStream> (data, (size_t) size, false)));
            if (r == nullptr) break;
            auto sd = std::make_unique<SampleData>();
            sd->sr = r->sampleRate; sd->name = TRACKS[t].file;
            sd->buf.setSize ((int) juce::jmin (2u, r->numChannels), (int) r->lengthInSamples);
            r->read (&sd->buf, 0, (int) r->lengthInSamples, 0, true, sd->buf.getNumChannels() > 1);
            samples[(size_t) t] = std::move (sd);
            break;
        }
    }
}

bool Grids8Processor::loadUserSample (int t, const juce::File& f)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples <= 0) return false;
    auto sd = std::make_unique<SampleData>();
    sd->sr = r->sampleRate; sd->name = f.getFileName();
    const int len = (int) juce::jmin<juce::int64> (r->lengthInSamples, (juce::int64) (r->sampleRate * 20));
    sd->buf.setSize ((int) juce::jmin (2u, r->numChannels), len);
    r->read (&sd->buf, 0, len, 0, true, sd->buf.getNumChannels() > 1);
    {
        const juce::SpinLock::ScopedLockType sl (sampleLock);
        if (samples[(size_t) t]) graveyard.push_back (std::move (samples[(size_t) t]));
        samples[(size_t) t] = std::move (sd);
    }
    userSamplePaths.set (t, f.getFullPathName());
    return true;
}

void Grids8Processor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& v : voices) v.active = false;
    pending.clear();
    lastStep = INT64_MIN; wasPlaying = false;
}

bool Grids8Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

float Grids8Processor::pv (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
void Grids8Processor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void Grids8Processor::newMap (uint32_t s)
{
    const int inactive = 1 - activeMap.load();
    maps[(size_t) inactive].generate (s);
    seed = s;
    activeMap = inactive;
}

// ---------- Parameter-Berechnung ----------
void Grids8Processor::masterEff (int64_t g, float& x, float& y, float& d) const
{
    const int sh = (int) mShape->load(), ra = (int) mRate->load();
    const auto s = seed.load();
    x = clamp01 (mX->load() + modVal (sh, ra, (double) g, s, 99, 0) * mDx->load() * .5f);
    y = clamp01 (mY->load() + modVal (sh, ra, (double) g, s, 99, 1) * mDy->load() * .5f);
    d = modVal (sh, ra, (double) g, s, 99, 2) * mDd->load() * .5f;
}
void Grids8Processor::masterEffUI (int64_t g, float& x, float& y) const { float d; masterEff (g, x, y, d); }

float Grids8Processor::fillBoost (int t, int64_t g) const
{
    float b = fillHeld.load() ? .35f : 0.f;
    if (afOn->load() > .5f)
    {
        const int e = AF_EVERY[juce::jlimit (0, 2, (int) afEvery->load())];
        const auto bar = posmod (g >= 0 ? g / 16 : (g - 15) / 16, e);
        if (bar == e - 1) b += (t == 4 || t == 5 || t == 6 || t == 7 || t == 1) ? .3f : .08f;
    }
    return b;
}

EffParams Grids8Processor::effParams (int t, int64_t g, float mx, float my, float md, bool) const
{
    const auto& p = tp[(size_t) t];
    const int sh = (int) p.mShape->load(), ra = (int) p.mRate->load();
    const auto s = seed.load();
    const float fb = fillBoost (t, g);
    EffParams e;
    e.x = clamp01 (p.x->load() + (mx - .5f) + modVal (sh, ra, (double) g, s, t, 0) * p.mDx->load() * .5f);
    e.y = clamp01 (p.y->load() + (my - .5f) + modVal (sh, ra, (double) g, s, t, 1) * p.mDy->load() * .5f);
    e.d = clamp01 (p.dens->load() + mDens->load() + md + modVal (sh, ra, (double) g, s, t, 2) * p.mDd->load() * .5f + fb);
    e.c = clamp01 (p.chaos->load() + mChaos->load() + fb * .6f);
    return e;
}

EffParams Grids8Processor::effParamsUI (int t, int64_t g) const
{
    float mx, my, md; masterEff (g, mx, my, md);
    return effParams (t, g, mx, my, md, false);
}

int Grids8Processor::noteFor (int t) const { return noteMap->load() < .5f ? 36 + t : TRACKS[t].gm; }

// ---------- Audio ----------
void Grids8Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    buffer.clear();
    midi.clear();

    double bpm = intBpm->load(), ppq = 0;
    bool playing = false, hostHasTransport = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying())
            {
                if (auto q = pos->getPpqPosition()) { ppq = *q; playing = true; hostHasTransport = true; }
            }
        }
    }
    hostBpm = (float) bpm; hostPlaying = hostHasTransport;
    const double spb = sr * 60.0 / juce::jmax (20.0, bpm), stepSamples = spb / 4.0;

    if (! playing && internalPlay.load())
    {
        if (! wasPlaying) internalPpq = 0;
        ppq = internalPpq; playing = true;
        internalPpq += n / spb;
    }

    if (playing)
    {
        if (! wasPlaying)
        {
            lastStep = INT64_MIN; curValid = false;
            for (auto& tr : perturb) for (auto& v : tr) v = rng.nextFloat();
        }
        const double s0 = ppq * 4.0, s1 = (ppq + n / spb) * 4.0;
        auto first = (int64_t) std::ceil (s0 - 1e-9);
        if (lastStep != INT64_MIN && (lastStep >= first + 2 || lastStep < first - 4)) { lastStep = INT64_MIN; curValid = false; } // Sprung/Loop
        if (lastStep != INT64_MIN && first <= lastStep) first = lastStep + 1;
        for (int64_t k = first; (double) k < s1; ++k)
        {
            const double offset = ((double) k - s0) * stepSamples;
            doStep (k, (double) sampleCounter + juce::jmax (0.0, offset), stepSamples);
            lastStep = k;
            uiStep = k;
        }
    }
    else if (wasPlaying)
    {
        curValid = false; lastStep = INT64_MIN; uiStep = -1;
        for (int c = 1; c <= 16; ++c) midi.addEvent (juce::MidiMessage::allNotesOff (c), 0);
        // ausstehende Note-Ons verwerfen, Note-Offs sofort senden
        for (auto& p : pending) if (p.isOff) midi.addEvent (juce::MidiMessage::noteOff (p.ch, p.note), 0);
        pending.clear();
    }
    wasPlaying = playing;

    // Ereignisse sampelgenau abarbeiten
    std::stable_sort (pending.begin(), pending.end(), [] (const Pending& a, const Pending& b) { return a.time < b.time; });
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    float tmpR[4096];
    if (R == nullptr && n <= 4096) R = tmpR;
    int cur = 0; size_t used = 0;
    const double blockEnd = (double) sampleCounter + n;
    for (; used < pending.size() && pending[used].time < blockEnd; ++used)
    {
        const auto& e = pending[used];
        const int off = juce::jlimit (0, n - 1, (int) (e.time - (double) sampleCounter));
        if (off > cur && R != nullptr) { renderVoices (L + cur, R + cur, off - cur); cur = off; }
        if (e.isOff) midi.addEvent (juce::MidiMessage::noteOff (e.ch, e.note), off);
        else
        {
            midi.addEvent (juce::MidiMessage::noteOn (e.ch, e.note, (juce::uint8) e.vel), off);
            if (sound->load() > .5f) startVoice (e.track, e.vel, e.time);
        }
    }
    pending.erase (pending.begin(), pending.begin() + (long) used);
    if (cur < n && R != nullptr) renderVoices (L + cur, R + cur, n - cur);

    const float mv = mVol->load();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            float x = d[i] * mv, a = std::abs (x);
            if (a > .9f) x = std::copysign (.9f + .1f * std::tanh ((a - .9f) / .1f), x);
            d[i] = x;
        }
    }
    sampleCounter += n;
}

void Grids8Processor::doStep (int64_t k, double t0, double sd)
{
    const int pos16 = (int) posmod (k, 16);
    if (pos16 == 0)
    {
        pushBar();
        curBar.nEvents = 0; curBar.nSteps = 0;
        curBar.bpm = hostBpm.load(); curBar.swing = swing->load();
        for (int t = 0; t < NT; ++t)
        {
            const auto& p = tp[(size_t) t];
            curBar.snd[t][0] = p.vol->load(); curBar.snd[t][1] = p.pitch->load(); curBar.snd[t][2] = p.decay->load(); curBar.snd[t][3] = p.pan->load();
        }
        curValid = true;
        ++barCounter;
    }
    const bool rec = curValid && curBar.nSteps < 16;
    const double sw = (k & 1) ? (swing->load() / 50.0 - 1.0) * sd : 0.0;
    bool anySolo = false;
    for (auto& p : tp) anySolo = anySolo || p.solo->load() > .5f;
    auto audible = [&] (int t) { const auto& p = tp[(size_t) t]; return anySolo ? p.solo->load() > .5f : p.mute->load() < .5f; };

    float mx, my, md; masterEff (k, mx, my, md);
    uint8_t* snap = rec ? curBar.params[curBar.nSteps] : nullptr;
    if (snap) { snap[0] = (uint8_t) std::lround (mx * 127); snap[1] = (uint8_t) std::lround (my * 127); }

    const bool midiPer = midiCh->load() > .5f;
    auto trigger = [&] (int t, double time, int vel, float o)
    {
        const int ch = midiPer ? t + 1 : 10, note = midiPer ? 36 : noteFor (t);
        time = juce::jmax (time, (double) sampleCounter);
        pending.push_back ({ time, t, vel, false, note, ch });
        pending.push_back ({ time + sr * .04, t, 0, true, note, ch });
        hitCount[t].fetch_add (1);
        if (rec && curBar.nEvents < NT * 16) curBar.ev[curBar.nEvents++] = { (uint8_t) pos16, (uint8_t) t, (uint8_t) vel, o };
    };

    std::shared_ptr<const PlaySlot> ps;
    { const juce::SpinLock::ScopedTryLockType tl (playLock); if (tl.isLocked()) ps = playSlot; }

    for (int t = 0; t < NT; ++t)
    {
        const auto& p = tp[(size_t) t];
        const auto e = effParams (t, k, mx, my, md, true);
        if (snap) { snap[2 + t * 3] = (uint8_t) std::lround (e.x * 127); snap[3 + t * 3] = (uint8_t) std::lround (e.y * 127); snap[4 + t * 3] = (uint8_t) std::lround (e.d * 127); }
        if (ps) continue;
        const int len = juce::jlimit (1, 32, (int) p.len->load());
        const int ts = (int) posmod (k, len);
        if (ts == 0) for (auto& v : perturb[t]) v = rng.nextFloat();
        float Lv = map().level (t, ts % NSTEP, e.x, e.y);
        if (e.c > 0) Lv += (perturb[t][ts % NSTEP] - .4f) * e.c * 230.f;
        if (! (e.d > .004f && Lv > 255.f * (1.f - e.d)) || ! audible (t)) continue;
        float v = Lv > 205 ? 104 + (Lv - 205) / 50 * 23 : 52 + Lv / 205 * 50;
        v += (rng.nextFloat() * 2 - 1) * hVel->load() * 24;
        const int vel = juce::jlimit (1, 127, (int) std::lround (v));
        const float o = (float) (sw / sd) + (rng.nextFloat() * 2 - 1) * hTime->load() * .18f;
        trigger (t, t0 + o * sd, vel, o);
    }
    if (ps && ps->bars > 0)
    {
        const auto pos = (size_t) posmod (k, ps->bars * 16);
        for (const auto& ev : ps->byStep[pos])
            if (audible (ev.t)) trigger (ev.t, t0 + ev.o * sd, ev.v, ev.o);
    }
    if (rec) ++curBar.nSteps;
}

void Grids8Processor::startVoice (int t, int vel, double)
{
    const juce::SpinLock::ScopedTryLockType tl (sampleLock);
    if (! tl.isLocked()) return;
    const SampleData* s = samples[(size_t) t].get();
    if (s == nullptr || s->buf.getNumSamples() < 2) return;
    const bool ck = choke->load() > .5f;
    const int partner = (t == 2) ? 3 : (t == 3 ? 2 : -1);
    for (auto& v : voices)
        if (v.active && (v.track == t || (ck && v.track == partner)) && ! v.fading)
        { v.fading = true; v.fadeStep = (float) (1.0 / (0.006 * sr)); }
    Voice* nv = nullptr;
    for (auto& v : voices) if (! v.active) { nv = &v; break; }
    if (nv == nullptr) { nv = &voices[0]; for (auto& v : voices) if (v.age > nv->age) nv = &v; }
    const auto& p = tp[(size_t) t];
    const float vol = p.vol->load(), pitch = p.pitch->load(), decay = p.decay->load(), pan = p.pan->load();
    Voice& v = *nv;
    v = Voice();
    v.active = true; v.s = s; v.track = t; v.pos = 0;
    v.inc = s->sr / sr * std::pow (2.0, pitch / 12.0);
    const float amp = std::pow (vel / 127.f, 1.6f) * vol;
    v.gL = amp * juce::jmin (1.f, 1.f - pan); v.gR = amp * juce::jmin (1.f, 1.f + pan);
    const double fullOut = s->buf.getNumSamples() / v.inc;
    if (decay >= .999f) { v.useDecay = false; v.dur = (int) fullOut; }
    else
    {
        const double durSec = juce::jmin (fullOut / sr, .025 + std::pow ((double) decay, 2.2) * fullOut / sr);
        v.dur = juce::jmax (8, (int) (durSec * sr)); v.hold = (int) (v.dur * .35);
        v.useDecay = true; v.envMul = (float) std::pow (0.0008, 1.0 / juce::jmax (1, v.dur - v.hold));
    }
}

void Grids8Processor::renderVoices (float* L, float* R, int n)
{
    for (auto& v : voices)
    {
        if (! v.active) continue;
        const int len = v.s->buf.getNumSamples();
        const float* d0 = v.s->buf.getReadPointer (0);
        const float* d1 = v.s->buf.getNumChannels() > 1 ? v.s->buf.getReadPointer (1) : d0;
        for (int i = 0; i < n; ++i)
        {
            const int i0 = (int) v.pos;
            if (i0 + 1 >= len) { v.active = false; break; }
            const float fr = (float) (v.pos - i0);
            const float l = d0[i0] + (d0[i0 + 1] - d0[i0]) * fr;
            const float r = d1[i0] + (d1[i0 + 1] - d1[i0]) * fr;
            const float g = v.env * v.fade;
            L[i] += l * v.gL * g; R[i] += r * v.gR * g;
            v.pos += v.inc; ++v.age;
            if (v.useDecay && v.age > v.hold) v.env *= v.envMul;
            if (v.age >= v.dur) { v.active = false; break; }
            if (v.fading) { v.fade -= v.fadeStep; if (v.fade <= 0) { v.active = false; break; } }
        }
    }
}

// ---------- Aufnahme & Slots ----------
void Grids8Processor::pushBar()
{
    if (! curValid || curBar.nSteps != 16) return;
    int s1, b1, s2, b2;
    barFifo.prepareToWrite (1, s1, b1, s2, b2);
    if (b1 > 0) barBuf[(size_t) s1] = curBar;
    else if (b2 > 0) barBuf[(size_t) s2] = curBar;
    barFifo.finishedWrite (b1 + b2);
}

void Grids8Processor::timerCallback()
{
    bool got = false;
    int s1, b1, s2, b2;
    barFifo.prepareToRead (barFifo.getNumReady(), s1, b1, s2, b2);
    if (b1 + b2 > 0)
    {
        const juce::ScopedLock sl (slotLock);
        for (int i = 0; i < b1; ++i) history.push_back (barBuf[(size_t) (s1 + i)]);
        for (int i = 0; i < b2; ++i) history.push_back (barBuf[(size_t) (s2 + i)]);
        while ((int) history.size() > HIST_MAX) history.pop_front();
        got = true;
    }
    barFifo.finishedRead (b1 + b2);
    if (got) { updateLiveSlots(); ++slotsVersion; }

    const auto bc = barCounter.load();
    if (bc != lastBarSeen)
    {
        lastBarSeen = bc;
        const int every = EV_EVERY[juce::jlimit (0, 3, (int) evEvery->load())];
        if (evOn->load() > .5f && bc > 1 && (bc - 1) % every == 0) randomize ("mutate");
    }
    if (glide.on)
    {
        const auto g = uiStep.load();
        if (g < 0) glide.on = false;
        else
        {
            const float k = clamp01 ((float) (g - glide.g0) / 32.f);
            setParam ("mX", lerpf (glide.x0, glide.x1, k)); setParam ("mY", lerpf (glide.y0, glide.y1, k));
            if (k >= 1.f) glide.on = false;
        }
    }
}

void Grids8Processor::fillSlot (int i)
{
    if (cfg[(size_t) i].lock || i == loopSlot) return;
    const int nb = juce::jmin (cfg[(size_t) i].len, (int) history.size());
    if (nb <= 0) return;
    SlotData d; d.bars = nb;
    for (int b = 0; b < nb; ++b)
    {
        const Bar& bar = history[history.size() - (size_t) nb + (size_t) b];
        for (int e = 0; e < bar.nEvents; ++e) d.ev.push_back ({ bar.ev[e].s + b * 16, bar.ev[e].t, bar.ev[e].v, bar.ev[e].o });
        for (int s = 0; s < 16; ++s) { std::array<uint8_t, NPARAMS_SNAP> a; std::copy (bar.params[s], bar.params[s] + NPARAMS_SNAP, a.begin()); d.params.push_back (a); }
        d.bpm = bar.bpm; d.swing = bar.swing;
    }
    slots[(size_t) i] = std::move (d);
}

void Grids8Processor::updateLiveSlots()
{
    const juce::ScopedLock sl (slotLock);
    for (int i = 0; i < 8; ++i) fillSlot (i);
}

void Grids8Processor::setSlotLen (int i, int len)
{
    const juce::ScopedLock sl (slotLock);
    cfg[(size_t) i].len = len; fillSlot (i); ++slotsVersion;
}
void Grids8Processor::toggleHold (int i)
{
    const juce::ScopedLock sl (slotLock);
    auto& c = cfg[(size_t) i]; c.lock = ! c.lock;
    if (! c.lock) { if (loopSlot == i) { loopSlot = -1; rebuildPlaySlot(); } fillSlot (i); }
    ++slotsVersion;
}
void Grids8Processor::clearSlot (int i)
{
    const juce::ScopedLock sl (slotLock);
    if (loopSlot == i) { loopSlot = -1; rebuildPlaySlot(); }
    slots[(size_t) i].reset(); cfg[(size_t) i].lock = false; fillSlot (i); ++slotsVersion;
}
void Grids8Processor::toggleLoop (int i)
{
    const juce::ScopedLock sl (slotLock);
    if (! slots[(size_t) i]) return;
    loopSlot = (loopSlot == i) ? -1 : i;
    if (loopSlot == i) cfg[(size_t) i].lock = true;
    rebuildPlaySlot(); ++slotsVersion;
}
void Grids8Processor::holdAll()
{
    const juce::ScopedLock sl (slotLock);
    bool anyLive = false;
    for (int i = 0; i < 8; ++i) anyLive = anyLive || (! cfg[(size_t) i].lock && slots[(size_t) i]);
    for (int i = 0; i < 8; ++i)
    {
        if (anyLive) { if (slots[(size_t) i]) cfg[(size_t) i].lock = true; }
        else { cfg[(size_t) i].lock = false; if (loopSlot == i) { loopSlot = -1; rebuildPlaySlot(); } fillSlot (i); }
    }
    ++slotsVersion;
}

void Grids8Processor::rebuildPlaySlot()
{
    std::shared_ptr<const PlaySlot> np;
    if (loopSlot >= 0 && slots[(size_t) loopSlot])
    {
        auto p = std::make_shared<PlaySlot>();
        const auto& d = *slots[(size_t) loopSlot];
        p->bars = juce::jmax (1, d.bars);
        p->byStep.resize ((size_t) p->bars * 16);
        for (const auto& e : d.ev) if (e.s >= 0 && e.s < p->bars * 16) p->byStep[(size_t) e.s].push_back (e);
        np = p;
    }
    std::shared_ptr<const PlaySlot> old;
    { const juce::SpinLock::ScopedLockType l (playLock); old = playSlot; playSlot = np; }
    // altes Objekt hier (Message-Thread) freigeben – der Audio-Thread hält es höchstens für einen Step
    juce::Thread::sleep (2);
    old.reset();
}

juce::String Grids8Processor::slotMeta (int i)
{
    const juce::ScopedLock sl (slotLock);
    const auto& c = cfg[(size_t) i];
    const auto& s = slots[(size_t) i];
    const juce::String dot = juce::String::fromUTF8 (" · ");
    if (! s) return c.lock ? "gehalten" + dot + "leer" : "live" + dot + "wartet auf Play";
    juce::String m = (c.lock ? "gehalten" : "live");
    m << dot << s->bars << " T";
    if (! c.lock && s->bars < c.len) m << juce::String::fromUTF8 (" (füllt ") << s->bars << "/" << c.len << ")";
    m << dot << juce::String (s->bpm, 0) << " BPM" << dot << (int) s->ev.size() << " Noten";
    return m;
}

juce::File Grids8Processor::writeSlotMidi (int i, bool multi, juce::File target)
{
    SlotData d;
    { const juce::ScopedLock sl (slotLock); if (! slots[(size_t) i]) return {}; d = *slots[(size_t) i]; }
    const int PPQ = 96, ST = 24, end = d.bars * 16 * ST, ch = 10;
    juce::MidiFile mf; mf.setTicksPerQuarterNote (PPQ);
    juce::MidiMessageSequence head;
    head.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60e6 / juce::jmax (20.f, d.bpm))), 0);
    head.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    auto addNotes = [&] (juce::MidiMessageSequence& seq, int onlyT)
    {
        for (const auto& e : d.ev)
        {
            if (onlyT >= 0 && e.t != onlyT) continue;
            const int on = juce::jlimit (0, end - 1, (int) std::lround (e.s * ST + e.o * ST));
            const int note = noteFor (e.t);
            seq.addEvent (juce::MidiMessage::noteOn (ch, note, (juce::uint8) e.v), on);
            seq.addEvent (juce::MidiMessage::noteOff (ch, note), juce::jmin (end, on + 12));
        }
    };
    auto addCC = [&] (juce::MidiMessageSequence& seq, int onlyT, bool master)
    {
        int last[128]; std::fill (std::begin (last), std::end (last), -1);
        auto put = [&] (int k, int cc, int v) { if (last[cc] != v) { last[cc] = v; seq.addEvent (juce::MidiMessage::controllerEvent (ch, cc, juce::jlimit (0, 127, v)), k * ST); } };
        for (size_t k = 0; k < d.params.size(); ++k)
        {
            const auto& p = d.params[k];
            if (master) { put ((int) k, 16, p[0]); put ((int) k, 17, p[1]); }
            for (int t = 0; t < NT; ++t)
            {
                if (onlyT >= 0 && t != onlyT) continue;
                put ((int) k, 20 + t, p[(size_t) (2 + t * 3)]); put ((int) k, 30 + t, p[(size_t) (3 + t * 3)]); put ((int) k, 40 + t, p[(size_t) (4 + t * 3)]);
            }
        }
    };
    const juce::String base = "Grids8_Slot" + juce::String (i + 1) + "_" + juce::String (d.bars) + "T_" + juce::String (d.bpm, 0) + "bpm";
    if (! multi)
    {
        juce::MidiMessageSequence seq (head);
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, base), 0);
        addNotes (seq, -1); addCC (seq, -1, true);
        seq.addEvent (juce::MidiMessage::endOfTrack(), end);
        seq.updateMatchedPairs();
        mf.addTrack (seq);
    }
    else
    {
        juce::MidiMessageSequence t0 (head);
        t0.addEvent (juce::MidiMessage::textMetaEvent (3, "Grids 8"), 0);
        t0.addEvent (juce::MidiMessage::endOfTrack(), end);
        mf.addTrack (t0);
        for (int t = 0; t < NT; ++t)
        {
            juce::MidiMessageSequence seq;
            seq.addEvent (juce::MidiMessage::textMetaEvent (3, juce::String (t + 1) + " " + TRACKS[t].name), 0);
            addNotes (seq, t); addCC (seq, t, t == 0);
            seq.addEvent (juce::MidiMessage::endOfTrack(), end);
            seq.updateMatchedPairs();
            mf.addTrack (seq);
        }
    }
    if (target == juce::File())
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Grids8");
        dir.createDirectory();
        target = dir.getChildFile (base + (multi ? "_8Spuren.mid" : "_DrumRack.mid"));
    }
    target.deleteFile();
    juce::FileOutputStream os (target);
    if (! os.openedOk()) return {};
    mf.writeTo (os, 1);
    os.flush();
    return target;
}

// ---------- Zufall ----------
static float gauss (juce::Random& r) { return (r.nextFloat() + r.nextFloat() + r.nextFloat() - 1.5f) / 1.5f; }

void Grids8Processor::randomize (const juce::String& kind)
{
    auto& r = uiRng;
    const float A = rndAmt->load();
    auto rx = [&] (float v) { return clamp01 (lerpf (v, r.nextFloat(), A * 1.4f)); };
    auto newDens = [&] (int t) { const float a = DENS_RANGE[t][0], b = DENS_RANGE[t][1]; return clamp01 (lerpf (pv (tid (t, "dens")), a + r.nextFloat() * (b - a), juce::jmin (1.f, A * 1.6f))); };
    for (int t = 0; t < NT; ++t)
    {
        if (kind == "all") { setParam (tid (t, "x"), rx (pv (tid (t, "x")))); setParam (tid (t, "y"), rx (pv (tid (t, "y")))); setParam (tid (t, "dens"), newDens (t)); setParam (tid (t, "chaos"), r.nextFloat() < .4f ? r.nextFloat() * .7f * A : 0.f); }
        else if (kind == "mutate") { setParam (tid (t, "x"), clamp01 (pv (tid (t, "x")) + gauss (r) * A * .35f)); setParam (tid (t, "y"), clamp01 (pv (tid (t, "y")) + gauss (r) * A * .35f)); setParam (tid (t, "dens"), clamp01 (pv (tid (t, "dens")) + gauss (r) * A * .18f)); }
        else if (kind == "xy") { setParam (tid (t, "x"), rx (pv (tid (t, "x")))); setParam (tid (t, "y"), rx (pv (tid (t, "y")))); }
        else if (kind == "dens") setParam (tid (t, "dens"), newDens (t));
        else if (kind == "chaos") setParam (tid (t, "chaos"), r.nextFloat() < .5f ? clamp01 (r.nextFloat() * A) : 0.f);
        else if (kind == "sound") { setParam (tid (t, "pitch"), (float) juce::jlimit (-12, 12, (int) std::lround (gauss (r) * 6 * A))); setParam (tid (t, "decay"), r.nextFloat() < .5f ? 1.f : .35f + r.nextFloat() * .65f); setParam (tid (t, "pan"), juce::jlimit (-1.f, 1.f, gauss (r) * .5f * A)); }
        else if (kind == "mods") { setParam (tid (t, "mShape"), r.nextFloat() < .25f ? 0.f : (float) (1 + r.nextInt (5))); setParam (tid (t, "mRate"), (float) (1 + r.nextInt (6))); setParam (tid (t, "mDx"), clamp01 (r.nextFloat() * A * 1.2f)); setParam (tid (t, "mDy"), clamp01 (r.nextFloat() * A * 1.2f)); setParam (tid (t, "mDd"), r.nextFloat() < .4f ? clamp01 (r.nextFloat() * A * .6f) : 0.f); }
        else if (kind == "len") { static const int L[9] = { 12, 16, 20, 24, 28, 31, 7, 10, 14 }; setParam (tid (t, "len"), (t == 0 || r.nextFloat() > A) ? 32.f : (float) L[r.nextInt (9)]); }
    }
    if (kind == "mutate") { setParam ("mX", clamp01 (pv ("mX") + gauss (r) * A * .15f)); setParam ("mY", clamp01 (pv ("mY") + gauss (r) * A * .15f)); }
    else if (kind == "map") newMap ((uint32_t) r.nextInt (0x7fffffff));
    else if (kind == "mutes")
    {
        for (int t = 0; t < NT; ++t) setParam (tid (t, "mute"), 0.f);
        const int k = 1 + (int) std::floor (r.nextFloat() * 3 * A + .5f);
        for (int i = 0; i < k; ++i) setParam (tid (1 + r.nextInt (7), "mute"), 1.f);
    }
    else if (kind == "glide") glide = { true, juce::jmax<int64_t> (0, uiStep.load()), pv ("mX"), pv ("mY"), r.nextFloat(), r.nextFloat() };
    else if (kind == "tempo") { setParam ("bpm", std::round (lerpf (pv ("bpm"), 80 + r.nextFloat() * 95, A))); setParam ("swing", std::round (50 + r.nextFloat() * 16 * A)); }
}

void Grids8Processor::diceTrack (int t, bool onlyXY)
{
    const float A = rndAmt->load();
    setParam (tid (t, "x"), clamp01 (lerpf (pv (tid (t, "x")), uiRng.nextFloat(), A * 1.4f)));
    setParam (tid (t, "y"), clamp01 (lerpf (pv (tid (t, "y")), uiRng.nextFloat(), A * 1.4f)));
    if (! onlyXY)
    {
        const float a = DENS_RANGE[t][0], b = DENS_RANGE[t][1];
        setParam (tid (t, "dens"), clamp01 (lerpf (pv (tid (t, "dens")), a + uiRng.nextFloat() * (b - a), juce::jmin (1.f, A * 1.6f))));
    }
}

// ---------- Zustand ----------
void Grids8Processor::serializeSlots (juce::ValueTree& vt)
{
    const juce::ScopedLock sl (slotLock);
    for (int i = 0; i < 8; ++i)
    {
        juce::ValueTree s ("Slot");
        s.setProperty ("idx", i, nullptr); s.setProperty ("len", cfg[(size_t) i].len, nullptr); s.setProperty ("lock", cfg[(size_t) i].lock, nullptr);
        if (const auto& d = slots[(size_t) i])
        {
            s.setProperty ("bars", d->bars, nullptr); s.setProperty ("bpm", d->bpm, nullptr); s.setProperty ("swing", d->swing, nullptr);
            juce::MemoryOutputStream mo;
            mo.writeInt ((int) d->ev.size());
            for (const auto& e : d->ev) { mo.writeInt (e.s); mo.writeByte ((char) e.t); mo.writeByte ((char) e.v); mo.writeFloat (e.o); }
            mo.writeInt ((int) d->params.size());
            for (const auto& p : d->params) mo.write (p.data(), p.size());
            s.setProperty ("data", mo.getMemoryBlock().toBase64Encoding(), nullptr);
        }
        vt.appendChild (s, nullptr);
    }
}

void Grids8Processor::deserializeSlots (const juce::ValueTree& vt)
{
    const juce::ScopedLock sl (slotLock);
    for (const auto& s : vt)
    {
        if (! s.hasType ("Slot")) continue;
        const int i = s.getProperty ("idx", -1);
        if (i < 0 || i > 7) continue;
        cfg[(size_t) i].len = s.getProperty ("len", 4); cfg[(size_t) i].lock = s.getProperty ("lock", false);
        slots[(size_t) i].reset();
        if (! s.hasProperty ("data")) continue;
        juce::MemoryBlock mb; mb.fromBase64Encoding (s.getProperty ("data").toString());
        juce::MemoryInputStream mi (mb, false);
        SlotData d; d.bars = s.getProperty ("bars", 1); d.bpm = s.getProperty ("bpm", 120.f); d.swing = s.getProperty ("swing", 50.f);
        const int ne = mi.readInt();
        for (int k = 0; k < ne && ! mi.isExhausted(); ++k) { SlotEvent e; e.s = mi.readInt(); e.t = (uint8_t) mi.readByte(); e.v = (uint8_t) mi.readByte(); e.o = mi.readFloat(); if (e.t < NT) d.ev.push_back (e); }
        const int np = mi.readInt();
        for (int k = 0; k < np && ! mi.isExhausted(); ++k) { std::array<uint8_t, NPARAMS_SNAP> a {}; mi.read (a.data(), (int) a.size()); d.params.push_back (a); }
        slots[(size_t) i] = std::move (d);
    }
    ++slotsVersion;
}

void Grids8Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::ValueTree extra ("Extra");
    extra.setProperty ("seed", (juce::int64) seed.load(), nullptr);
    extra.setProperty ("samples", userSamplePaths.joinIntoString ("\n"), nullptr);
    juce::ValueTree sl ("Slots");
    serializeSlots (sl);
    state.appendChild (extra, nullptr);
    state.appendChild (sl, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void Grids8Processor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;
    auto extra = state.getChildWithName ("Extra");
    auto sl = state.getChildWithName ("Slots");
    state.removeChild (extra, nullptr); state.removeChild (sl, nullptr);
    if (state.hasType (apvts.state.getType())) apvts.replaceState (state);
    if (extra.isValid())
    {
        newMap ((uint32_t) (juce::int64) extra.getProperty ("seed", (juce::int64) 20251120));
        juce::StringArray paths; paths.addLines (extra.getProperty ("samples").toString());
        for (int t = 0; t < NT && t < paths.size(); ++t)
            if (paths[t].isNotEmpty()) { juce::File f (paths[t]); if (f.existsAsFile()) loadUserSample (t, f); }
    }
    if (sl.isValid()) deserializeSlots (sl);
    loopSlot = -1; rebuildPlaySlot();
}

} // namespace g8

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new g8::Grids8Processor(); }
juce::AudioProcessorEditor* g8::Grids8Processor::createEditor() { return new Grids8Editor (*this); }
