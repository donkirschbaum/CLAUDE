#pragma once
#include <cmath>
#include <array>

// Monophone Granular-Engine – 1:1 Port der Engine aus der Browser-Version (web/index.html)
struct GrainParams
{
    float tune = 0, start = 0, end = 1, grain = 0, shift = 1, smooth = 0.3f;
    float bits = 16, srate = 48000, attack = 2, release = 150;
    bool loop = true, reverse = false;
};

class GrainEngine
{
public:
    void prepare (double sampleRate) { sr = sampleRate; }

    void setSample (const float* data, int length, double sampleRate)
    {
        buf = data; len = length; bsr = sampleRate;
        active = false; env = 0; gate = false;
    }

    void setBend (float semis) { bend = semis; }

    void noteOn (int n, float v, bool retrig, const GrainParams& p)
    {
        note = n; vel = v;
        if (retrig || ! active)
        {
            int lo, hi; bounds (p, lo, hi);
            const int d = dir (p);
            raw = win = (d > 0 ? (double) lo : (double) (hi - 1));
            next = 0;
            for (auto& g : grains) g.on = false;
            ended = false;
        }
        gate = true; active = true;
    }

    void noteOff() { gate = false; }
    void kill()    { gate = false; active = false; env = 0; }

    void process (float* out, int n, const GrainParams& p)
    {
        if (buf == nullptr || len < 2) { for (int k = 0; k < n; ++k) out[k] = 0; return; }

        int lo, hi; bounds (p, lo, hi);
        const int d = dir (p);
        const double ratio = std::pow (2.0, (note - 60 + p.tune + bend) / 12.0) * bsr / sr;
        const double aInc = 1.0 / std::max (1.0, p.attack * 0.001 * sr);
        const double rDec = 1.0 / std::max (1.0, p.release * 0.001 * sr);
        const bool gOn = p.grain >= 2.0f;
        const double glen = std::max (16.0, p.grain * 0.001 * sr);
        const double hop  = std::max (8.0, glen * (1.0 - p.smooth * 0.5));
        const double fade = p.smooth * 0.5 * glen;
        const double winInc = p.shift * d * bsr / sr;
        const double hinc = std::min (1.0, (double) p.srate / sr);
        const float q = p.bits < 16 ? std::pow (2.0f, std::round (p.bits) - 1.0f) : 0.0f;

        for (int k = 0; k < n; ++k)
        {
            if (! active) { out[k] = 0; continue; }

            if (gate) { env += aInc; if (env > 1) env = 1; }
            else
            {
                env -= rDec;
                if (env <= 0) { env = 0; active = false; out[k] = 0; continue; }
            }

            float s = 0;
            if (! ended)
            {
                if (! gOn)
                {
                    s = read (raw);
                    raw += ratio * d;
                    if (raw >= hi || raw < lo)
                    {
                        if (p.loop) raw = wrap (raw, lo, hi);
                        else ended = true;
                    }
                }
                else
                {
                    if (--next <= 0)
                    {
                        next = hop;
                        for (auto& g : grains)
                            if (! g.on) { g.on = true; g.pos = win; g.age = 0; g.len = glen; break; }
                    }
                    for (auto& g : grains)
                    {
                        if (! g.on) continue;
                        double w = 1;
                        if (fade > 0)
                        {
                            if (g.age < fade) w = g.age / fade;
                            else if (g.age > g.len - fade) w = (g.len - g.age) / fade;
                        }
                        s += (float) (read (g.pos) * w);
                        g.pos += ratio * d;
                        if (g.pos >= hi || g.pos < lo) g.pos = wrap (g.pos, lo, hi);
                        if (++g.age >= g.len) g.on = false;
                    }
                    win += winInc;
                    if (win >= hi || win < lo)
                    {
                        if (p.loop) win = wrap (win, lo, hi);
                        else ended = true;
                    }
                }
            }

            hp += hinc;
            if (hp >= 1) { hp -= 1; hv = s; }
            s = hv;
            if (q > 0) s = std::round (s * q) / q;
            out[k] = (float) (s * env * vel);
        }
    }

    // Für die Anzeige
    float uiPos (const GrainParams& p) const { return (float) ((p.grain >= 2 ? win : raw) / std::max (1, len)); }
    float uiGrain (const GrainParams& p) const { return p.grain >= 2 ? (float) (p.grain * 0.001 * bsr / std::max (1, len)) : 0.0f; }
    bool  isActive() const { return active && ! ended; }
    float envelope() const { return (float) env; }

private:
    struct Grain { bool on = false; double pos = 0, age = 0, len = 1; };

    void bounds (const GrainParams& p, int& lo, int& hi) const
    {
        lo = (int) std::floor (std::min (p.start, p.end) * len);
        hi = (int) std::floor (std::max (p.start, p.end) * len);
        if (hi - lo < 64) { hi = std::min (len, lo + 64); lo = std::max (0, hi - 64); }
    }
    static int dir (const GrainParams& p) { int d = p.reverse ? -1 : 1; if (p.end < p.start) d = -d; return d; }

    float read (double x) const
    {
        const int i = (int) std::floor (x);
        const float f = (float) (x - i);
        if (i < 0 || i >= len - 1) return (i >= 0 && i < len) ? buf[i] : 0.0f;
        return buf[i] + (buf[i + 1] - buf[i]) * f;
    }
    static double wrap (double x, int lo, int hi)
    {
        const double w = hi - lo;
        if (x >= hi) x = lo + std::fmod (x - lo, w);
        else if (x < lo) { x = hi - std::fmod (lo - x, w); if (x >= hi) x = lo; }
        return x;
    }

    const float* buf = nullptr; int len = 0;
    double sr = 48000, bsr = 44100;
    std::array<Grain, 12> grains;
    double env = 0; bool gate = false, active = false, ended = false;
    float vel = 1, bend = 0; int note = 60;
    double raw = 0, win = 0, next = 0, hp = 1;
    float hv = 0;
};
