#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;
using namespace octo;

// ======================================================================== WaveBank
namespace
{
struct Coeffs
{
    std::array<std::array<float, WaveBank::H + 1>, NWAVES> re {}, im {};
    Coeffs()
    {
        constexpr int M = 4096;
        std::vector<double> sinT (M), cosT (M);
        for (int n = 0; n < M; ++n)
        {
            sinT[(size_t) n] = std::sin (MathConstants<double>::twoPi * n / M);
            cosT[(size_t) n] = std::cos (MathConstants<double>::twoPi * n / M);
        }
        std::vector<double> f (M);
        for (int w = 0; w < NWAVES; ++w)
        {
            const int src = (w == SUPERSAW ? SAW : w);
            if (auto* h = harmonicSpec (src))
            {
                for (size_t k = 0; k < h->size() && k < WaveBank::H; ++k)
                    im[(size_t) w][k + 1] = (*h)[k];
                continue;
            }
            for (int n = 0; n < M; ++n)
                f[(size_t) n] = waveFn (src, (double) n / M);
            for (int k = 1; k <= WaveBank::H; ++k)
            {
                double r = 0, i = 0;
                for (int n = 0; n < M; ++n)
                {
                    const size_t idx = (size_t) (((long long) k * n) % M);
                    r += f[(size_t) n] * cosT[idx];
                    i += f[(size_t) n] * sinT[idx];
                }
                re[(size_t) w][(size_t) k] = (float) (r * 2.0 / M);
                im[(size_t) w][(size_t) k] = (float) (i * 2.0 / M);
            }
        }
    }
};

const Coeffs& coeffs()
{
    static const Coeffs c;
    return c;
}
} // namespace

void WaveBank::build (double sr)
{
    if (sr == sampleRate && ! data.empty())
        return;
    sampleRate = sr;
    data.assign ((size_t) NWAVES * BANDS * (N + 1), 0.0f);
    const auto& c = coeffs();

    std::vector<float> sinT (N), cosT (N);
    for (int n = 0; n < N; ++n)
    {
        sinT[(size_t) n] = (float) std::sin (MathConstants<double>::twoPi * n / N);
        cosT[(size_t) n] = (float) std::cos (MathConstants<double>::twoPi * n / N);
    }

    int hmax[BANDS];
    double top = 40.0;
    for (int b = 0; b < BANDS; ++b, top *= 2.0)
        hmax[b] = jlimit (1, H, (int) (sr * 0.45 / top));

    std::vector<double> acc (N);
    for (int w = 0; w < NWAVES; ++w)
    {
        std::fill (acc.begin(), acc.end(), 0.0);
        for (int k = 1; k <= H; ++k)
        {
            const float rk = c.re[(size_t) w][(size_t) k], ik = c.im[(size_t) w][(size_t) k];
            if (rk != 0.0f || ik != 0.0f)
                for (int n = 0; n < N; ++n)
                {
                    const size_t idx = (size_t) ((k * n) % N);
                    acc[(size_t) n] += rk * cosT[idx] + ik * sinT[idx];
                }
            for (int b = 0; b < BANDS; ++b)
                if (hmax[b] == k)
                {
                    float* t = data.data() + ((size_t) w * BANDS + (size_t) b) * (N + 1);
                    for (int n = 0; n < N; ++n)
                        t[n] = (float) acc[(size_t) n];
                }
        }
        // normalise every band of this wave by the peak of the richest band
        float* t0 = data.data() + ((size_t) w * BANDS) * (N + 1);
        float pk = 0.0f;
        for (int n = 0; n < N; ++n)
            pk = jmax (pk, std::abs (t0[n]));
        const float g = pk > 0 ? 0.9f / pk : 1.0f;
        for (int b = 0; b < BANDS; ++b)
        {
            float* t = data.data() + ((size_t) w * BANDS + (size_t) b) * (N + 1);
            for (int n = 0; n < N; ++n)
                t[n] *= g;
            t[N] = t[0]; // guard point for interpolation
        }
    }
}

// ======================================================================== Processor
Octo8Processor::Octo8Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    keyRaw = apvts.getRawParameterValue ("key");
    pKey = keyRaw;
    pScale = apvts.getRawParameterValue ("scale");
    pMode = apvts.getRawParameterValue ("mode");
    pPlay = apvts.getRawParameterValue ("play");
    pBpm = apvts.getRawParameterValue ("bpm");
    pHost = apvts.getRawParameterValue ("hostsync");
    pGate = apvts.getRawParameterValue ("gate");
    pMaster = apvts.getRawParameterValue ("master");
    for (int i = 0; i < NCH; ++i)
    {
        auto g = [&] (const char* n) { return apvts.getRawParameterValue (pid (i, n)); };
        cp[i] = { g ("wave"), g ("noise"), g ("ntype"), g ("gain"), g ("tune"), g ("fine"), g ("mute"), g ("solo"),
                  g ("ftype"), g ("cutoff"), g ("reso"), g ("attack"), g ("decay"), g ("sustain"), g ("release"),
                  g ("smute"), g ("ssolo") };
        hitCount[i] = 0;
        for (int k = 0; k < NSTEPS; ++k) { stepOn[i][k] = false; stepDeg[i][k] = 0; }
    }
    initDefaultPattern();
}

AudioProcessorValueTreeState::ParameterLayout Octo8Processor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout L;
    StringArray scl;
    for (auto& s : scales()) scl.add (s.name);

    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };
    auto hz = [] (float v, int) { return v < 1000 ? String (roundToInt (v)) + " Hz" : String (v / 1000.0f, v < 10000 ? 2 : 1) + " kHz"; };
    auto sec = [] (float v, int) { return v < 1 ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s"; };

    L.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Tonart", noteNames(), 9));
    L.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Skala", scl, 0));
    L.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Modus", StringArray { "Drone", "Sequencer" }, 1));
    L.add (std::make_unique<AudioParameterBool> (ParameterID { "play", 1 }, "Play (intern)", false));
    L.add (std::make_unique<AudioParameterInt> (ParameterID { "bpm", 1 }, "Master BPM", 40, 220, 118));
    L.add (std::make_unique<AudioParameterBool> (ParameterID { "hostsync", 1 }, "Host Sync", true));
    L.add (std::make_unique<AudioParameterFloat> (ParameterID { "gate", 1 }, "Gate", NormalisableRange<float> (0.05f, 1.0f, 0.01f), 0.5f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    L.add (std::make_unique<AudioParameterFloat> (ParameterID { "master", 1 }, "Master", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.6f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    const int defWave[NCH] { 0, 3, 2, 1, 15, 5, 17, 0 };
    const int defTune[NCH] { -12, 0, 7, 3, 12, 10, 15, 19 };
    const float defCut[NCH] { 500, 1800, 2400, 3000, 2600, 2000, 6000, 7000 };

    for (int i = 0; i < NCH; ++i)
    {
        auto grp = std::make_unique<AudioProcessorParameterGroup> ("ch" + String (i + 1), "Kanal " + String (i + 1), " | ");
        auto id = [i] (const char* n) { return ParameterID { pid (i, n), 1 }; };
        auto nm = [i] (const char* n) { return "K" + String (i + 1) + " " + n; };
        const bool hat = (i == 7);

        grp->addChild (std::make_unique<AudioParameterChoice> (id ("wave"), nm ("Waveform"), waveNames(), defWave[i]));
        grp->addChild (std::make_unique<AudioParameterBool> (id ("noise"), nm ("Noise"), hat));
        grp->addChild (std::make_unique<AudioParameterChoice> (id ("ntype"), nm ("Noise-Typ"), noiseNames(), 0));
        grp->addChild (std::make_unique<AudioParameterFloat> (id ("gain"), nm ("Gain"), NormalisableRange<float> (0, 1, 0.01f), 0.5f,
                                                              AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        grp->addChild (std::make_unique<AudioParameterInt> (id ("tune"), nm ("Tune"), -24, 24, defTune[i],
            AudioParameterIntAttributes().withStringFromValueFunction ([this] (int v, int)
            {
                String s = (v > 0 ? "+" : "") + String (v);
                if (keyRaw != nullptr)
                {
                    const int m = 48 + (int) keyRaw->load() + v;
                    s << " " << noteNames()[m % 12] << String (m / 12 - 1);
                }
                return s;
            })));
        grp->addChild (std::make_unique<AudioParameterInt> (id ("fine"), nm ("Fine"), -100, 100, 0,
            AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return (v > 0 ? "+" : "") + String (v) + " ct"; })));
        grp->addChild (std::make_unique<AudioParameterBool> (id ("mute"), nm ("Mute"), false));
        grp->addChild (std::make_unique<AudioParameterBool> (id ("solo"), nm ("Solo"), false));
        grp->addChild (std::make_unique<AudioParameterChoice> (id ("ftype"), nm ("Filter-Typ"), StringArray { "LP", "HP", "BP" }, hat ? 1 : 0));

        NormalisableRange<float> cutR (20.0f, 20000.0f); cutR.setSkewForCentre (1000.0f);
        NormalisableRange<float> aR (0.001f, 4.0f); aR.setSkewForCentre (0.1f);
        NormalisableRange<float> dR (0.01f, 4.0f); dR.setSkewForCentre (0.3f);
        NormalisableRange<float> rR (0.01f, 6.0f); rR.setSkewForCentre (0.4f);
        grp->addChild (std::make_unique<AudioParameterFloat> (id ("cutoff"), nm ("Cutoff"), cutR, defCut[i], AudioParameterFloatAttributes().withStringFromValueFunction (hz)));
        grp->addChild (std::make_unique<AudioParameterFloat> (id ("reso"), nm ("Reso"), NormalisableRange<float> (0.1f, 24.0f, 0.1f), 2.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); })));
        grp->addChild (std::make_unique<AudioParameterFloat> (id ("attack"), nm ("Attack"), aR, 0.004f, AudioParameterFloatAttributes().withStringFromValueFunction (sec)));
        grp->addChild (std::make_unique<AudioParameterFloat> (id ("decay"), nm ("Decay"), dR, hat ? 0.06f : 0.25f, AudioParameterFloatAttributes().withStringFromValueFunction (sec)));
        grp->addChild (std::make_unique<AudioParameterFloat> (id ("sustain"), nm ("Sustain"), NormalisableRange<float> (0, 1, 0.01f), hat ? 0.0f : 0.35f,
                                                              AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        grp->addChild (std::make_unique<AudioParameterFloat> (id ("release"), nm ("Release"), rR, hat ? 0.05f : 0.3f, AudioParameterFloatAttributes().withStringFromValueFunction (sec)));
        grp->addChild (std::make_unique<AudioParameterBool> (id ("smute"), nm ("Seq Mute"), false));
        grp->addChild (std::make_unique<AudioParameterBool> (id ("ssolo"), nm ("Seq Solo"), false));
        L.add (std::move (grp));
    }
    return L;
}

void Octo8Processor::initDefaultPattern()
{
    Random r (20261127);
    for (int k : { 0, 4, 8, 12 }) stepOn[0][k] = true;
    for (int k : { 2, 6, 10, 14 }) stepOn[7][k] = true;
    const int degs[] { -3, -2, -1, 1, 2, 3, 4, 7 };
    for (int i = 1; i < 7; ++i)
    {
        const float dens = 0.2f + r.nextFloat() * 0.4f;
        for (int k = 0; k < NSTEPS; ++k)
        {
            const bool on = r.nextFloat() < dens * (k % 4 == 0 ? 1.7f : 1.0f);
            stepOn[i][k] = on;
            stepDeg[i][k] = (on && r.nextFloat() < 0.35f) ? degs[r.nextInt (8)] : 0;
        }
    }
}

bool Octo8Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == AudioChannelSet::stereo() || out == AudioChannelSet::mono();
}

void Octo8Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    bank.build (sr);

    // noise buffers (3 s each, looped)
    const int n = (int) (sr * 3.0);
    Random r (12345);
    for (int t = 0; t < NNOISE; ++t)
    {
        auto& d = noiseBuf[t];
        d.assign ((size_t) n + 1, 0.0f);
        double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0, br = 0, prev = 0, hold = 0;
        auto pink = [&] (double w)
        {
            b0 = .99886 * b0 + w * .0555179; b1 = .99332 * b1 + w * .0750759; b2 = .969 * b2 + w * .153852;
            b3 = .8665 * b3 + w * .3104856; b4 = .55 * b4 + w * .5329522; b5 = -.7616 * b5 - w * .016898;
            const double o = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * .5362; b6 = w * .115926;
            return o * .11;
        };
        for (int i = 0; i < n; ++i)
        {
            const double w = r.nextDouble() * 2 - 1;
            double v = 0;
            switch (t)
            {
                case 0: v = w; break;
                case 1: v = pink (w); break;
                case 2: br = (br + .02 * w) / 1.02; v = br * 3.5; break;
                case 3: { const double p = pink (w); v = p - prev; prev = p; break; }
                case 4: v = w - prev; prev = w; break;
                case 5: br = (br + .02 * w) / 1.02; v = pink (w) * .7 + br * 2 + w * .12; break;
                case 6: v = r.nextDouble() < .03 ? (r.nextBool() ? 1.0 : -1.0) : 0.0; break;
                case 7: hold *= .92; if (r.nextDouble() < .0007) hold = w; v = hold; break;
                default: if (i % 12 == 0) hold = w; v = hold; break;
            }
            d[(size_t) i] = (float) v;
        }
        float pk = 0;
        for (int i = 0; i < n; ++i) pk = jmax (pk, std::abs (d[(size_t) i]));
        for (int i = 0; i < n; ++i) d[(size_t) i] *= 0.9f / (pk > 0 ? pk : 1.0f);
        d[(size_t) n] = d[0];
    }

    dsp::ProcessSpec spec { sr, (uint32) jmax (1, samplesPerBlock), 1 };
    for (auto& c : chans)
    {
        c.filter.prepare (spec);
        c.filter.reset();
        c.env = c.envTarget = 0;
        c.decayCnt = c.gateCnt = 0;
        c.vca = 0;
        c.noisePos = 0;
        for (int v = 0; v < 7; ++v) c.phase[v] = Random::getSystemRandom().nextDouble();
    }
    comp.prepare (spec);
    comp.setThreshold (-12.0f);
    comp.setRatio (6.0f);
    comp.setAttack (5.0f);
    comp.setRelease (120.0f);
    comp.reset();
    lastAbsStep = std::numeric_limits<long long>::min();
    wasRunning = droneOn = false;
    lastMode = -1;
}

// ---------------------------------------------------------------- scale helpers
bool Octo8Processor::inScale (int m, int key, int si) const
{
    const int pc = ((m - key) % 12 + 12) % 12;
    for (int iv : scales()[(size_t) si].iv)
        if (iv == pc) return true;
    return false;
}

int Octo8Processor::quant (int m, int key, int si) const
{
    for (int d = 0; d < 12; ++d)
    {
        if (inScale (m - d, key, si)) return m - d;
        if (inScale (m + d, key, si)) return m + d;
    }
    return m;
}

int Octo8Processor::degShift (int m, int deg, int key, int si) const
{
    int q = quant (m, key, si);
    const int dir = deg > 0 ? 1 : -1;
    for (int n = 0; n < std::abs (deg); ++n)
    {
        int x = q + dir;
        while (! inScale (x, key, si) && x > 0 && x < 127) x += dir;
        q = jlimit (12, 119, x);
    }
    return q;
}

double Octo8Processor::freqOf (int ch, int deg, int key, int si) const
{
    int m = 48 + key + (int) cp[ch].tune->load() + transpose;
    if (deg != 0) m = degShift (m, deg, key, si);
    return 440.0 * std::pow (2.0, (m - 69) / 12.0) * std::pow (2.0, cp[ch].fine->load() / 1200.0);
}

bool Octo8Processor::audible (int ch) const
{
    bool any = false;
    for (int i = 0; i < NCH; ++i) any = any || cp[i].solo->load() > 0.5f;
    return cp[ch].mute->load() < 0.5f && (! any || cp[ch].solo->load() > 0.5f);
}

bool Octo8Processor::trackOn (int ch) const
{
    bool any = false;
    for (int i = 0; i < NCH; ++i) any = any || cp[i].ssolo->load() > 0.5f;
    return cp[ch].smute->load() < 0.5f && (! any || cp[ch].ssolo->load() > 0.5f);
}

// ---------------------------------------------------------------- voice control
void Octo8Processor::updatePitch (int i, int key, int si)
{
    auto& c = chans[i];
    const double f = freqOf (i, c.deg, key, si);
    const int w = jlimit (0, NWAVES - 1, (int) cp[i].wave->load());
    c.isNoise = cp[i].noise->load() > 0.5f;
    c.isSuper = (w == SUPERSAW);
    c.ntype = jlimit (0, NNOISE - 1, (int) cp[i].nt->load());
    if (c.isNoise)
    {
        const double base = 440.0 * std::pow (2.0, (48 + key - 69) / 12.0);
        c.noiseRate = jlimit (0.125, 4.0, f / base);
        return;
    }
    if (w == SUPERSAW)
    {
        static const double det[7] { -24, -14, -6, 0, 6, 14, 24 };
        for (int v = 0; v < 7; ++v)
            c.inc[v] = f * std::pow (2.0, det[v] / 1200.0) / sr;
        c.tab = bank.table (SAW, WaveBank::bandFor (f * 1.02));
    }
    else
    {
        c.inc[0] = f / sr;
        c.tab = bank.table (w, WaveBank::bandFor (f));
    }
}

void Octo8Processor::trigger (int i, double gateSec, int deg)
{
    auto& c = chans[i];
    const float a = cp[i].a->load(), d = cp[i].d->load(), r = cp[i].r->load();
    c.deg = deg;
    c.envTarget = 1.0f;
    c.envCoef = envCoef (jmax (0.001f, a) / 4.0f);
    c.decayCnt = (a < gateSec) ? jmax (1, (int) (a * sr)) : 0;
    c.gateCnt = jmax (1, (int) (jmax (gateSec, (double) a + 0.001) * sr));
    ignoreUnused (d, r);
}

void Octo8Processor::releaseAll()
{
    for (int i = 0; i < NCH; ++i)
    {
        auto& c = chans[i];
        c.envTarget = 0.0f;
        c.envCoef = envCoef (jmax (0.01f, cp[i].r->load()) / 4.0f);
        c.decayCnt = c.gateCnt = 0;
        c.deg = 0;
    }
    droneOn = false;
    playStep = -1;
}

void Octo8Processor::startDrone()
{
    for (int i = 0; i < NCH; ++i)
    {
        auto& c = chans[i];
        c.deg = 0;
        c.envTarget = 1.0f;
        c.envCoef = envCoef (jmax (0.01f, cp[i].a->load()) / 4.0f);
        c.decayCnt = c.gateCnt = 0;
    }
    droneOn = true;
}

// ---------------------------------------------------------------- audio
void Octo8Processor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    buffer.clear();
    const int n = buffer.getNumSamples();

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) { transpose = m.getNoteNumber() - 60; ++held; }
        else if (m.isNoteOff()) held = jmax (0, held - 1);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) held = 0;
    }

    bool hp = false, known = false;
    const bool hostSync = pHost->load() > 0.5f;
    double bpm = pBpm->load(), hostPpq = 0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) { hostBpm = (float) *b; known = true; if (hostSync) bpm = *b; }
            hp = hostSync && pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) hostPpq = *p; else hp = false;
        }
    hostBpmKnown = known;
    hostPlaying = hp;

    const int key = (int) pKey->load();
    const int si = jlimit (0, (int) scales().size() - 1, (int) pScale->load());
    const int mode = (int) pMode->load();
    const bool seqMode = mode == 1;
    const bool running = hp || pPlay->load() > 0.5f;

    if (mode != lastMode) { releaseAll(); lastAbsStep = std::numeric_limits<long long>::min(); lastMode = mode; wasRunning = false; }

    if (seqMode)
    {
        if (! running && wasRunning) releaseAll();
        if (running && ! wasRunning) { intPpq = 0; lastAbsStep = std::numeric_limits<long long>::min(); }
    }
    else
    {
        const bool gate = running || held > 0;
        if (gate && ! droneOn) startDrone();
        else if (! gate && droneOn) releaseAll();
    }
    wasRunning = running;

    // per-block channel setup
    float gainT[NCH], sus[NCH], decC[NCH], relC[NCH];
    for (int i = 0; i < NCH; ++i)
    {
        auto& c = chans[i];
        gainT[i] = audible (i) ? cp[i].gain->load() : 0.0f;
        sus[i] = cp[i].s->load();
        decC[i] = envCoef (jmax (0.004f, cp[i].d->load()) / 4.0f);
        relC[i] = envCoef (jmax (0.004f, cp[i].r->load()) / 4.0f);
        const int ft = (int) cp[i].ft->load();
        c.filter.setType (ft == 1 ? dsp::StateVariableTPTFilterType::highpass
                                  : ft == 2 ? dsp::StateVariableTPTFilterType::bandpass
                                            : dsp::StateVariableTPTFilterType::lowpass);
        const float target = jlimit (20.0f, (float) (sr * 0.45), cp[i].cut->load());
        c.cutoff += (target - c.cutoff) * 0.35f;
        c.filter.setCutoffFrequency (c.cutoff);
        c.filter.setResonance (jmax (0.1f, cp[i].q->load()));
        updatePitch (i, key, si);
    }

    const float vcaC = 1.0f - std::exp (-1.0f / (0.012f * (float) sr));
    const float masterT = pMaster->load();
    double ppq = hp ? hostPpq : intPpq;
    const double ppqPerSample = bpm / 60.0 / sr;
    const double stepDur = 60.0 / jmax (1.0, bpm) / 4.0;
    const float gateFrac = pGate->load();

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    int sw = scopeWrite.load();

    for (int s = 0; s < n; ++s)
    {
        if (seqMode && running)
        {
            const long long absStep = (long long) std::floor (ppq * 4.0 + 1e-9);
            if (absStep != lastAbsStep)
            {
                lastAbsStep = absStep;
                const int st = (int) (((absStep % NSTEPS) + NSTEPS) % NSTEPS);
                for (int i = 0; i < NCH; ++i)
                    if (stepOn[i][st].load() && trackOn (i))
                    {
                        trigger (i, stepDur * gateFrac, stepDeg[i][st].load());
                        updatePitch (i, key, si);
                        if (audible (i)) hitCount[i].fetch_add (1);
                    }
                playStep = st;
            }
            ppq += ppqPerSample;
        }

        float mix = 0.0f;
        for (int i = 0; i < NCH; ++i)
        {
            auto& c = chans[i];
            if (c.decayCnt > 0 && --c.decayCnt == 0) { c.envTarget = sus[i]; c.envCoef = decC[i]; }
            if (c.gateCnt > 0 && --c.gateCnt == 0) { c.envTarget = 0.0f; c.envCoef = relC[i]; }
            c.env += (c.envTarget - c.env) * c.envCoef;
            c.vca += (gainT[i] - c.vca) * vcaC;
            if (c.env < 1.0e-5f && c.envTarget == 0.0f && c.vca < 1.0e-5f)
                continue;

            float x;
            if (c.isNoise)
            {
                const auto& d = noiseBuf[c.ntype];
                const double len = (double) d.size() - 1;
                if (len < 2) continue;
                const int ip = (int) c.noisePos;
                const float fr = (float) (c.noisePos - ip);
                x = d[(size_t) ip] + (d[(size_t) ip + 1] - d[(size_t) ip]) * fr;
                c.noisePos += c.noiseRate;
                if (c.noisePos >= len) c.noisePos -= len;
            }
            else
            {
                if (c.tab == nullptr) continue;
                const bool sup = c.isSuper;
                const int voices = sup ? 7 : 1;
                x = 0.0f;
                for (int v = 0; v < voices; ++v)
                {
                    const double p = c.phase[v] * WaveBank::N;
                    const int ip = (int) p;
                    const float fr = (float) (p - ip);
                    x += c.tab[ip] + (c.tab[ip + 1] - c.tab[ip]) * fr;
                    c.phase[v] += c.inc[v];
                    if (c.phase[v] >= 1.0) c.phase[v] -= 1.0;
                }
                if (sup) x *= 0.38f;
            }
            x = c.filter.processSample (0, x);
            mix += x * c.env * c.vca;
        }

        masterSm += (masterT - masterSm) * vcaC;
        float out = comp.processSample (0, mix * masterSm);
        out = std::tanh (out);
        L[s] = out;
        if (R != nullptr) R[s] = out;
        scope[(size_t) sw] = out;
        sw = (sw + 1) & (SCOPE - 1);
    }
    scopeWrite = sw;
    if (! hp && running && seqMode) intPpq = ppq;
    if (! seqMode || ! running) playStep = -1;
}

// ---------------------------------------------------------------- actions
void Octo8Processor::setParam (const String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void Octo8Processor::randomChannel (int i)
{
    if (param (pid (i, "noise")) > 0.5f) setParam (pid (i, "ntype"), (float) rng.nextInt (NNOISE));
    else setParam (pid (i, "wave"), (float) rng.nextInt (NWAVES));
    const int key = (int) param ("key"), si = (int) param ("scale");
    const int b = 48 + key;
    Array<int> opts;
    for (int m = b - 12; m <= b + 19; ++m)
        if (inScale (m, key, si)) opts.add (m);
    if (! opts.isEmpty()) setParam (pid (i, "tune"), (float) (opts[rng.nextInt (opts.size())] - b));
    setParam (pid (i, "fine"), (float) (rng.nextInt (11) - 5));
    setParam (pid (i, "gain"), 0.3f + rng.nextFloat() * 0.45f);
}

void Octo8Processor::masterRandom()
{
    for (int i = 0; i < NCH; ++i) randomChannel (i);
}

void Octo8Processor::randomTrack (int i)
{
    const float dens = 0.2f + rng.nextFloat() * 0.4f;
    const int degs[] { -3, -2, -1, 1, 2, 3, 4, 7 };
    for (int k = 0; k < NSTEPS; ++k)
    {
        const bool on = rng.nextFloat() < dens * (k % 4 == 0 ? 1.7f : 1.0f);
        stepOn[i][k] = on;
        stepDeg[i][k] = (on && rng.nextFloat() < 0.35f) ? degs[rng.nextInt (8)] : 0;
    }
}

void Octo8Processor::randomAllTracks()
{
    for (int i = 0; i < NCH; ++i) randomTrack (i);
}

void Octo8Processor::clearAll()
{
    for (int i = 0; i < NCH; ++i)
        for (int k = 0; k < NSTEPS; ++k) { stepOn[i][k] = false; stepDeg[i][k] = 0; }
}

void Octo8Processor::requantize()
{
    const int key = (int) param ("key"), si = (int) param ("scale");
    for (int i = 0; i < NCH; ++i)
    {
        const int tune = (int) param (pid (i, "tune"));
        int t = quant (48 + key + tune, key, si) - (48 + key);
        while (t > 24) t -= 12;
        while (t < -24) t += 12;
        if (t != tune) setParam (pid (i, "tune"), (float) t);
    }
}

// ---------------------------------------------------------------- state
void Octo8Processor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<XmlElement> xml (state.createXml());
    auto* seq = xml->createNewChildElement ("SEQ");
    for (int i = 0; i < NCH; ++i)
    {
        StringArray parts;
        for (int k = 0; k < NSTEPS; ++k)
            parts.add (String (stepOn[i][k].load() ? 1 : 0) + ":" + String (stepDeg[i][k].load()));
        seq->setAttribute ("t" + String (i), parts.joinIntoString (","));
    }
    copyXmlToBinary (*xml, destData);
}

void Octo8Processor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr) return;
    if (auto* seq = xml->getChildByName ("SEQ"))
    {
        for (int i = 0; i < NCH; ++i)
        {
            StringArray parts;
            parts.addTokens (seq->getStringAttribute ("t" + String (i)), ",", "");
            for (int k = 0; k < NSTEPS && k < parts.size(); ++k)
            {
                stepOn[i][k] = parts[k].upToFirstOccurrenceOf (":", false, false).getIntValue() != 0;
                stepDeg[i][k] = jlimit (-7, 14, parts[k].fromFirstOccurrenceOf (":", false, false).getIntValue());
            }
        }
        xml->removeChildElement (seq, true);
    }
    if (xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* Octo8Processor::createEditor() { return new Octo8Editor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Octo8Processor(); }
