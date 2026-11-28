#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace shaker;

namespace
{
enum TP { gain, tune, fine, attack, decay, sustain, release, lowcut, highcut, reso, bodyf, body, air, drive, comp, width,
          vol, pan, rev, mute, solo, steps, mode, rate, rprob, rtime, rvel, rpitch, rlen, rstart, rrev, rpan, rratchet, TP_COUNT };
const char* tpNames[TP_COUNT] = { "gain", "tune", "fine", "attack", "decay", "sustain", "release", "lowcut", "highcut", "reso", "bodyf",
                                  "body", "air", "drive", "comp", "width", "vol", "pan", "rev", "mute", "solo", "steps", "mode", "rate",
                                  "rprob", "rtime", "rvel", "rpitch", "rlen", "rstart", "rrev", "rpan", "rratchet" };
enum GP { swing, chaos, evolve, drift, density, ghost, room, roomsize, master, tempo, polydice, GP_COUNT };
const char* gpNames[GP_COUNT] = { "swing", "chaos", "evolve", "drift", "density", "ghost", "room", "roomsize", "master", "tempo", "polydice" };

juce::String fmtDb (float v, int) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; }
juce::String fmtMs (float v, int) { return v < 1000 ? juce::String (v < 10 ? v : (float) juce::roundToInt (v), v < 10 ? 1 : 0) + " ms" : juce::String (v / 1000.0f, 2) + " s"; }
juce::String fmtHz (float v, int) { return v < 1000 ? juce::String (juce::roundToInt (v)) + " Hz" : juce::String (v / 1000.0f, v < 10000 ? 2 : 1) + " kHz"; }
juce::String fmtPct (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }

using APF = juce::AudioParameterFloat;
using Attr = juce::AudioParameterFloatAttributes;

std::unique_ptr<APF> fp (const juce::String& id, const juce::String& name, float mn, float mx, float def, std::function<juce::String (float, int)> f,
                         float centre = 0, float interval = 0)
{
    juce::NormalisableRange<float> r (mn, mx, interval);
    if (centre > 0) r.setSkewForCentre (centre);
    return std::make_unique<APF> (juce::ParameterID { id, 1 }, name, r, def, Attr().withStringFromValueFunction (f));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout ShakerProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout L;
    juce::StringArray rates;
    for (auto& r : kRates) rates.add (r.label);

    for (int t = 0; t < kTracks; ++t)
    {
        const juce::String pre = "Spur " + juce::String (t + 1) + " ";
        auto id = [t] (int k) { return tid (t, tpNames[k]); };
        L.add (fp (id (gain), pre + "Gain", -24, 12, 0, fmtDb));
        L.add (fp (id (tune), pre + "Tune", -24, 24, 0, [] (float v, int) { return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v)) + " HT"; }, 0, 1));
        L.add (fp (id (fine), pre + "Finetune", -100, 100, 0, [] (float v, int) { return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v)) + " ct"; }, 0, 1));
        L.add (fp (id (attack), pre + "Attack", 0, 500, 0, fmtMs, 60));
        L.add (fp (id (decay), pre + "Decay", 5, 3000, 180, fmtMs, 250));
        L.add (fp (id (sustain), pre + "Sustain", 0, 1, 0, fmtPct));
        L.add (fp (id (release), pre + "Release", 5, 3000, 150, fmtMs, 250));
        L.add (fp (id (lowcut), pre + "Low Cut", 20, 4000, 20, fmtHz, 300));
        L.add (fp (id (highcut), pre + "High Cut", 1500, 20000, 20000, fmtHz, 6000));
        L.add (fp (id (reso), pre + "Resonanz", 0.5f, 12, 0.707f, [] (float v, int) { return "Q " + juce::String (v, 1); }, 2));
        L.add (fp (id (bodyf), pre + "Korpus-Freq", 120, 1500, 420, fmtHz, 450));
        L.add (fp (id (body), pre + "Korpus", -12, 12, 0, fmtDb));
        L.add (fp (id (air), pre + "Glanz", -12, 12, 0, fmtDb));
        L.add (fp (id (drive), pre + "Saettigung", 0, 1, 0, fmtPct));
        L.add (fp (id (comp), pre + "Kompressor", 0, 1, 0, fmtPct));
        L.add (fp (id (width), pre + "Stereo-Breite", 0, 2, 1, [] (float v, int) { return v < 0.02f ? juce::String ("mono") : fmtPct (v, 0); }));
        L.add (fp (id (vol), pre + "Lautstaerke", 0, 1, 0.8f, fmtPct));
        L.add (fp (id (pan), pre + "Panorama", -1, 1, 0, [] (float v, int) { return std::abs (v) < 0.01f ? juce::String ("C") : (v < 0 ? "L " : "R ") + juce::String (juce::roundToInt (std::abs (v) * 100)); }));
        L.add (fp (id (rev), pre + "Hall-Send", 0, 1, 0.15f, fmtPct));
        L.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id (mute), 1 }, pre + "Mute", false));
        L.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id (solo), 1 }, pre + "Solo", false));
        L.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id (steps), 1 }, pre + "Schritte", 1, kMaxSteps, 16));
        L.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id (mode), 1 }, pre + "Modus", juce::StringArray { "Polymeter", "Polyrhythmus" }, 0));
        L.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id (rate), 1 }, pre + "Tempo", rates, kRateX1));
        L.add (fp (id (rprob), pre + "Wuerfel-Hits", 0, 1, 0, fmtPct));
        L.add (fp (id (rtime), pre + "Timing", 0, 1, 0.15f, [] (float v, int) { return "+" + juce::String (juce::roundToInt (v * 22)) + " ms"; }));
        L.add (fp (id (rvel), pre + "Dynamik", 0, 1, 0.2f, fmtPct));
        L.add (fp (id (rpitch), pre + "Pitch-Zufall", 0, 1, 0, [] (float v, int) { return "+/-" + juce::String (v * 2, 1) + " HT"; }));
        L.add (fp (id (rlen), pre + "Huellkurven-Zufall", 0, 1, 0, fmtPct));
        L.add (fp (id (rstart), pre + "Start-Zufall", 0, 1, 0, [] (float v, int) { return "+" + juce::String (juce::roundToInt (v * 30)) + " ms"; }));
        L.add (fp (id (rrev), pre + "Reverse-Chance", 0, 1, 0, fmtPct));
        L.add (fp (id (rpan), pre + "Stereo-Streuung", 0, 1, 0.1f, fmtPct));
        L.add (fp (id (rratchet), pre + "Ratchets", 0, 1, 0, fmtPct));
    }
    L.add (fp ("swing", "Swing", 0, 1, 0.1f, fmtPct));
    L.add (fp ("chaos", "Chaos", 0, 2, 0, [] (float v, int) { return "x" + juce::String (v, 2); }));
    L.add (fp ("evolve", "Evolution", 0, 1, 0, fmtPct));
    L.add (fp ("drift", "Pitch-Drift", 0, 1, 0, fmtPct));
    L.add (fp ("density", "Dichte", 0, 1, 0, fmtPct));
    L.add (fp ("ghost", "Ghosts", 0, 1, 0, fmtPct));
    L.add (fp ("room", "Raum", 0, 1, 0, fmtPct));
    L.add (fp ("roomsize", "Raumgroesse", 0, 1, 0, fmtPct));
    L.add (fp ("master", "Master", 0, 1, 0.8f, fmtPct));
    L.add (fp ("tempo", "Tempo intern", 60, 180, 96, [] (float v, int) { return juce::String (juce::roundToInt (v)) + " bpm"; }, 0, 1));
    L.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "polydice", 1 }, "Schritte mitwuerfeln", true));
    return L;
}

ShakerProcessor::ShakerProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    cacheParams();
    randomAll (true);
}

void ShakerProcessor::cacheParams()
{
    for (int t = 0; t < kTracks; ++t)
        for (int k = 0; k < TP_COUNT; ++k)
            tptr[t].v[k] = apvts.getRawParameterValue (tid (t, tpNames[k]));
    for (int k = 0; k < GP_COUNT; ++k)
        gptr[k] = apvts.getRawParameterValue (gpNames[k]);
}

bool ShakerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void ShakerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sampleRate, juce::jmax (16, samplesPerBlock));
}

void ShakerProcessor::readParams (TrackParams* out, GlobalParams& g) const
{
    for (int t = 0; t < kTracks; ++t)
    {
        auto& p = out[t];
        auto v = [this, t] (int k) { return tptr[t].v[k]->load(); };
        p.gain = v (gain); p.tune = v (tune); p.fine = v (fine); p.attack = v (attack); p.decay = v (decay); p.sustain = v (sustain);
        p.release = v (release); p.lowcut = v (lowcut); p.highcut = v (highcut); p.reso = v (reso); p.bodyF = v (bodyf); p.body = v (body);
        p.air = v (air); p.drive = v (drive); p.comp = v (comp); p.width = v (width); p.vol = v (vol); p.pan = v (pan); p.rev = v (rev);
        p.mute = v (mute) > 0.5f; p.solo = v (solo) > 0.5f;
        p.steps = juce::roundToInt (v (steps)); p.mode = juce::roundToInt (v (mode)); p.rate = juce::roundToInt (v (rate));
        p.rProb = v (rprob); p.rTime = v (rtime); p.rVel = v (rvel); p.rPitch = v (rpitch); p.rLen = v (rlen); p.rStart = v (rstart);
        p.rRev = v (rrev); p.rPan = v (rpan); p.rRatchet = v (rratchet);
    }
    g.swing = gptr[swing]->load(); g.chaos = gptr[chaos]->load(); g.evolve = gptr[evolve]->load(); g.drift = gptr[drift]->load();
    g.density = gptr[density]->load(); g.ghost = gptr[ghost]->load(); g.room = gptr[room]->load(); g.roomSize = gptr[roomsize]->load();
    g.master = gptr[master]->load(); g.tempo = gptr[tempo]->load();
}

// ---------------- Export ----------------
// Rendert den Loop offline mit einer eigenen Engine-Instanz im Hintergrund.
class ShakerProcessor::ExportJob : public juce::Thread
{
public:
    ExportJob (ShakerProcessor& p, double sampleRate, double bpmIn, double beatsIn, juce::File f)
        : juce::Thread ("Shaker Lab Export"), proc (p), sr (sampleRate), bpm (bpmIn), beats (beatsIn), file (std::move (f))
    {
        proc.readParams (tp, gp);
        for (int t = 0; t < kTracks; ++t)
        {
            auto& s = proc.engine.shared[t];
            sample[t] = s.sample.load(); start[t] = s.start.load(); end[t] = s.end.load();
            for (int i = 0; i < kMaxSteps; ++i) pat[t][i] = s.pat[(size_t) i].load();
        }
    }

    void run() override
    {
        Engine e (proc.lib);
        const int block = 512;
        e.prepare (sr, block);
        for (int t = 0; t < kTracks; ++t)
        {
            e.shared[t].sample.store (sample[t]);
            e.shared[t].start.store (start[t]);
            e.shared[t].end.store (end[t]);
            for (int i = 0; i < kMaxSteps; ++i) e.shared[t].pat[(size_t) i].store (pat[t][i]);
        }
        const double spb = sr * 60.0 / bpm;
        const int total = (int) std::floor (beats * spb);
        const int tail = (int) (sr * 4.0);                 // Ausklang, wird auf den Anfang gefaltet
        juce::AudioBuffer<float> out (2, total + tail);
        out.clear();
        juce::AudioBuffer<float> buf (2, block);

        for (int pos = 0; pos < total + tail;)
        {
            if (threadShouldExit()) { proc.exportState.store (0); return; }
            int n = juce::jmin (block, total + tail - pos);
            if (pos < total && pos + n > total) n = total - pos;   // Block endet genau am Loop-Ende
            juce::AudioBuffer<float> b (buf.getArrayOfWritePointers(), 2, n);
            // minimal früher, damit der erste Schritt der Wiederholung nicht mehr ausgelöst wird
            Transport tr { pos < total, pos / spb - 1e-9, bpm };
            e.process (b, tr, gp, tp);
            for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, b, c, 0, n);
            pos += n;
            proc.exportProgress.store ((float) pos / (float) (total + tail));
        }

        // Ausklang auf den Anfang falten → nahtloser Loop
        juce::AudioBuffer<float> loop (2, total);
        for (int c = 0; c < 2; ++c)
        {
            loop.copyFrom (c, 0, out, c, 0, total);
            for (int i = 0; i < tail; ++i)
                loop.addSample (c, i % juce::jmax (1, total), out.getSample (c, total + i));
        }
        const float pk = loop.getMagnitude (0, total);
        if (pk > 0.99f) loop.applyGain (0.99f / pk);

        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
        auto w = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (2).withBitsPerSample (24));
        const bool ok = w != nullptr && w->writeFromAudioSampleBuffer (loop, 0, total);
        w.reset();
        proc.exportFile = file;
        proc.exportState.store (ok ? 2 : 3);
    }

private:
    ShakerProcessor& proc;
    double sr, bpm, beats;
    juce::File file;
    TrackParams tp[kTracks];
    GlobalParams gp;
    int sample[kTracks] {};
    float start[kTracks] {}, end[kTracks] {};
    float pat[kTracks][kMaxSteps] {};
};

ShakerProcessor::~ShakerProcessor()
{
    if (exportJob) exportJob->stopThread (5000);
}

double ShakerProcessor::exportBpm() const
{
    return hostBpmKnown.load() ? hostBpm.load() : (double) getParam ("tempo");
}

double ShakerProcessor::loopBeats (bool capped) const
{
    long N = 1, D = 0;
    for (int t = 0; t < kTracks; ++t)
    {
        long n, d;
        cycleFraction (juce::roundToInt (getParam (tid (t, "steps"))), juce::roundToInt (getParam (tid (t, "mode"))),
                       juce::roundToInt (getParam (tid (t, "rate"))), n, d);
        N = N / std::gcd (N, n) * n;
        D = D == 0 ? d : std::gcd (D, d);
        if (N > 1000000) break;
    }
    const double b = (double) N / (double) D;
    return capped ? juce::jmin (b, maxExportBars * 4.0) : b;
}

std::unique_ptr<juce::Thread> ShakerProcessor::makeExportJob()
{
    const double bpm = exportBpm();
    const double beats = loopBeats (true);
    const double rate = getSampleRate() > 0 ? getSampleRate() : 48000.0;
    const double bars = beats / 4.0;
    const juce::String barsTxt = std::abs (bars - std::round (bars)) < 1e-6 ? juce::String ((int) std::round (bars)) : juce::String (bars, 2);
    const auto name = "Shaker Lab - " + juce::String (juce::roundToInt (bpm)) + " BPM - " + barsTxt + (bars == 1.0 ? " Takt - " : " Takte - ")
                      + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M%S") + ".wav";
    auto f = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Shaker Lab Exports").getChildFile (name);
    exportInfo = barsTxt + (bars == 1.0 ? " Takt" : " Takte") + " @ " + juce::String (bpm, 1) + " BPM"
                 + (loopBeats (false) > beats + 1e-6 ? " (auf " + juce::String ((int) maxExportBars) + " Takte gekuerzt)" : juce::String());
    exportKey = currentKey();
    exportProgress.store (0.0f);
    exportState.store (1);
    return std::make_unique<ExportJob> (*this, rate, bpm, beats, f);
}

void ShakerProcessor::startExport()
{
    if (exportState.load() == 1) return;
    if (exportJob) exportJob->stopThread (5000);
    exportJob.reset (static_cast<ExportJob*> (makeExportJob().release()));
    exportJob->startThread();
}

bool ShakerProcessor::renderExportNow()
{
    if (exportState.load() == 1 && exportJob) exportJob->stopThread (10000);
    auto job = makeExportJob();
    job->run();   // im aufrufenden Thread rendern
    return exportState.load() == 2;
}

juce::String ShakerProcessor::currentKey() const
{
    auto m = snapshot();
    juce::uint64 h = 1469598103934665603ull;   // FNV-1a
    auto* d = static_cast<const juce::uint8*> (m.getData());
    for (size_t i = 0; i < m.getSize(); ++i) { h ^= d[i]; h *= 1099511628211ull; }
    return juce::String::toHexString ((juce::int64) h) + "@" + juce::String (exportBpm(), 3) + "@" + juce::String (getSampleRate());
}

bool ShakerProcessor::isExportCurrent() const
{
    return exportState.load() == 2 && exportFile.existsAsFile() && exportKey == currentKey();
}

// ---------------- Verlauf ----------------
juce::MemoryBlock ShakerProcessor::snapshot() const
{
    juce::MemoryBlock m;
    const_cast<ShakerProcessor*> (this)->getStateInformation (m);
    return m;
}

void ShakerProcessor::restore (const juce::MemoryBlock& m)
{
    restoring = true;
    setStateInformation (m.getData(), (int) m.getSize());
    restoring = false;
}

bool ShakerProcessor::stateDiffersFrom (int idx) const
{
    return idx < 0 || idx >= (int) hist.size() || snapshot() != hist[(size_t) idx];
}

void ShakerProcessor::pushHistory()
{
    if (restoring) return;
    auto m = snapshot();
    if (histIdx >= 0 && histIdx < (int) hist.size() && hist[(size_t) histIdx] == m) return;
    hist.erase (hist.begin() + (histIdx + 1), hist.end());
    hist.push_back (std::move (m));
    if (hist.size() > 100) hist.erase (hist.begin());
    histIdx = (int) hist.size() - 1;
}

void ShakerProcessor::undo()
{
    if (histIdx < 0) return;
    // ungespeicherte Änderungen (z. B. Regler) vorher sichern, damit "Vor" dorthin zurückführt
    if (stateDiffersFrom (histIdx)) pushHistory();
    if (histIdx <= 0) return;
    --histIdx;
    restore (hist[(size_t) histIdx]);
}

void ShakerProcessor::redo()
{
    if (! canRedo()) return;
    ++histIdx;
    restore (hist[(size_t) histIdx]);
}

void ShakerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();

    readParams (tp, gp);

    // Transport: Host hat Vorrang, sonst interner Play-Knopf
    Transport tr;
    bool hp = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            hp = pos->getIsPlaying();
            tr.bpm = pos->getBpm().orFallback (120.0);
            tr.ppq = pos->getPpqPosition().orFallback (0.0);
            hostBpm.store (tr.bpm);
            if (pos->getBpm().hasValue()) hostBpmKnown.store (true);
        }
    hostPlaying.store (hp);
    if (hp)
    {
        tr.playing = true;
        internalPpq = 0;
    }
    else if (internalPlay.load())
    {
        tr.playing = true;
        tr.bpm = gp.tempo;
        tr.ppq = internalPpq;
        internalPpq += buffer.getNumSamples() / (sr * 60.0 / tr.bpm);
    }
    else
    {
        tr.playing = false;
        internalPpq = 0;
    }

    engine.process (buffer, tr, gp, tp);
}

// ---------------- Aktionen ----------------
void ShakerProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

float ShakerProcessor::getParam (const juce::String& id) const
{
    if (auto* v = apvts.getRawParameterValue (id)) return v->load();
    return 0.0f;
}

void ShakerProcessor::setSample (int t, int libIndex)
{
    auto* s = lib.get (libIndex);
    if (s == nullptr) return;
    auto& sh = engine.shared[t];
    sh.start.store (s->onset);
    sh.end.store ((float) s->lengthSeconds());
    sh.sample.store (libIndex);
    uiVersion++;
}

void ShakerProcessor::setRegion (int t, float start, float end)
{
    engine.shared[t].start.store (start);
    engine.shared[t].end.store (end);
}

void ShakerProcessor::resetSound (int t)
{
    for (int k = gain; k <= width; ++k)
        if (auto* p = apvts.getParameter (tid (t, tpNames[k])))
            p->setValueNotifyingHost (p->getDefaultValue());
}

void ShakerProcessor::applyRoleMix (int t)
{
    const auto& m = Generator::roleMix[t];
    resetSound (t);
    setParam (tid (t, "decay"), (float) juce::roundToInt (m.decMin + rng.nextFloat() * (m.decMax - m.decMin)));
    setParam (tid (t, "sustain"), 0.0f);
    setParam (tid (t, "vol"), m.vol);
    setParam (tid (t, "pan"), t == 3 ? (rng.nextBool() ? -0.25f : 0.25f) : m.pan);
}

void ShakerProcessor::genTrack (int t)
{
    TrackConfig cfg { juce::roundToInt (getParam (tid (t, "steps"))), juce::roundToInt (getParam (tid (t, "mode"))), juce::roundToInt (getParam (tid (t, "rate"))) };
    float pat[kMaxSteps];
    Generator::genRole (t, false, cfg, pat, getParam ("density"), getParam ("ghost"), rng);
    for (int i = 0; i < kMaxSteps; ++i) engine.shared[t].pat[(size_t) i].store (pat[i]);
    uiVersion++;
    pushHistory();
}

void ShakerProcessor::patternsOnly()
{
    const bool withCfg = getParam ("polydice") > 0.5f;
    for (int t = 0; t < kTracks; ++t)
    {
        TrackConfig cfg { juce::roundToInt (getParam (tid (t, "steps"))), juce::roundToInt (getParam (tid (t, "mode"))), juce::roundToInt (getParam (tid (t, "rate"))) };
        float pat[kMaxSteps];
        Generator::genRole (t, withCfg, cfg, pat, getParam ("density"), getParam ("ghost"), rng);
        for (int i = 0; i < kMaxSteps; ++i) engine.shared[t].pat[(size_t) i].store (pat[i]);
        setParam (tid (t, "steps"), (float) cfg.steps);
        setParam (tid (t, "mode"), (float) cfg.mode);
        setParam (tid (t, "rate"), (float) cfg.rate);
    }
    uiVersion++;
    if (! inBatch) pushHistory();
}

void ShakerProcessor::samplesOnly()
{
    auto picks = Generator::pickSamples (lib, rng);
    for (int t = 0; t < kTracks; ++t)
        if (picks[(size_t) t] >= 0)
        {
            setSample (t, picks[(size_t) t]);
            applyRoleMix (t);
        }
    uiVersion++;
    if (! inBatch) pushHistory();
}

void ShakerProcessor::randomAll (bool firstRun)
{
    const juce::ScopedValueSetter<bool> batch (inBatch, true);
    for (auto* id : { "chaos", "evolve", "drift", "density", "ghost", "room", "roomsize" })
        setParam (id, 0.0f);
    samplesOnly();
    if (firstRun) setParam ("polydice", 1.0f);
    patternsOnly();
    pushHistory();
}

void ShakerProcessor::mutateAll()
{
    for (int t = 0; t < kTracks; ++t)
    {
        float pat[kMaxSteps];
        for (int i = 0; i < kMaxSteps; ++i) pat[i] = engine.shared[t].pat[(size_t) i].load();
        Generator::mutate (pat, juce::roundToInt (getParam (tid (t, "steps"))), 0.18f, rng);
        for (int i = 0; i < kMaxSteps; ++i) engine.shared[t].pat[(size_t) i].store (pat[i]);
    }
    uiVersion++;
    pushHistory();
}

void ShakerProcessor::setSteps (int t, int n)
{
    n = juce::jlimit (1, kMaxSteps, n);
    const int old = juce::roundToInt (getParam (tid (t, "steps")));
    // neue Schritte starten leer, bestehende bleiben erhalten
    for (int i = old; i < n; ++i) engine.shared[t].pat[(size_t) i].store (0.0f);
    setParam (tid (t, "steps"), (float) n);
}

int ShakerProcessor::loadCustomFiles (const juce::Array<juce::File>& files)
{
    int last = -1;
    for (auto& f : files)
    {
        const int i = lib.loadFile (f);
        if (i >= 0) last = i;
    }
    libVersion++;
    return last;
}

void ShakerProcessor::removeCustom (int libIndex)
{
    auto* s = lib.get (libIndex);
    if (s == nullptr || ! s->custom) return;
    s->hidden.store (true);
    int fallback = -1;
    for (int i = 0; i < lib.size(); ++i)
        if (! lib.get (i)->custom) { fallback = i; break; }
    for (int t = 0; t < kTracks; ++t)
        if (engine.shared[t].sample.load() == libIndex && fallback >= 0)
            setSample (t, fallback);
    libVersion++;
    uiVersion++;
    pushHistory();
}

// ---------------- Speichern / Laden im Projekt ----------------
void ShakerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto old = state.getChildWithName ("SHAKER"); old.isValid()) state.removeChild (old, nullptr);
    juce::ValueTree sh ("SHAKER");
    juce::ValueTree custom ("CUSTOM");
    for (int i = 0; i < lib.size(); ++i)
    {
        auto* s = lib.get (i);
        if (s->custom && ! s->hidden.load())
        {
            juce::ValueTree f ("FILE");
            f.setProperty ("path", s->file.getFullPathName(), nullptr);
            custom.appendChild (f, nullptr);
        }
    }
    sh.appendChild (custom, nullptr);
    for (int t = 0; t < kTracks; ++t)
    {
        auto& e = engine.shared[t];
        juce::ValueTree tr ("TRACK");
        auto* s = lib.get (e.sample.load());
        tr.setProperty ("sample", s ? s->key : juce::String(), nullptr);
        tr.setProperty ("start", e.start.load(), nullptr);
        tr.setProperty ("end", e.end.load(), nullptr);
        juce::StringArray pat;
        for (int i = 0; i < kMaxSteps; ++i) pat.add (juce::String (e.pat[(size_t) i].load(), 3));
        tr.setProperty ("pattern", pat.joinIntoString (","), nullptr);
        sh.appendChild (tr, nullptr);
    }
    state.appendChild (sh, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ShakerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;
    auto sh = state.getChildWithName ("SHAKER");
    if (sh.isValid()) state.removeChild (sh, nullptr);
    if (state.hasType (apvts.state.getType()))
        apvts.replaceState (state);
    if (! sh.isValid()) return;

    juce::Array<juce::File> files;
    for (auto f : sh.getChildWithName ("CUSTOM"))
        files.add (juce::File (f.getProperty ("path").toString()));
    for (auto& f : files)
        if (f.existsAsFile()) lib.loadFile (f);
    libVersion++;

    int t = 0;
    for (auto tr : sh)
    {
        if (! tr.hasType ("TRACK") || t >= kTracks) continue;
        auto& e = engine.shared[t];
        const int idx = lib.findKey (tr.getProperty ("sample").toString());
        if (idx >= 0)
        {
            e.sample.store (idx);
            e.start.store ((float) tr.getProperty ("start", 0.0));
            e.end.store ((float) tr.getProperty ("end", lib.get (idx)->lengthSeconds()));
        }
        auto pat = juce::StringArray::fromTokens (tr.getProperty ("pattern").toString(), ",", "");
        for (int i = 0; i < kMaxSteps; ++i)
            e.pat[(size_t) i].store (i < pat.size() ? pat[i].getFloatValue() : 0.0f);
        ++t;
    }
    uiVersion++;
    if (! restoring)
    {
        hist.clear();
        histIdx = -1;
        pushHistory();
    }
}

juce::AudioProcessorEditor* ShakerProcessor::createEditor() { return new ShakerEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ShakerProcessor(); }
