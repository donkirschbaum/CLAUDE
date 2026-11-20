// Fundstueck - Analyse: Onset-Erkennung, Merkmale, Tonhoehe, Demo-Klaenge
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <functional>

namespace fs
{
constexpr double kPi = 3.14159265358979323846;

enum SliceClass { clsLow = 0, clsMid = 1, clsHigh = 2, clsTonal = 3 };

struct Slice
{
    std::vector<float> data;   // normalisiert, mit Fades
    int start = 0, end = 0;    // Position in der Quelle
    float centroid = 0, low = 0, zcr = 0, rms = 0, rmsN = 0, decay = 0, f0 = 0, pconf = 0, midi = 60;
    int cls = clsMid;
};

struct SliceSet
{
    std::vector<Slice> slices;
    double sampleRate = 44100.0;
};

inline std::vector<float> fftMag (const std::vector<float>& input)
{
    const size_t N = input.size();
    std::vector<float> re (input), im (N, 0.0f);
    for (size_t i = 1, j = 0; i < N; ++i)
    {
        size_t bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (re[i], re[j]);
    }
    for (size_t len = 2; len <= N; len <<= 1)
    {
        const double ang = -2.0 * kPi / (double) len;
        const float wr = (float) std::cos (ang), wi = (float) std::sin (ang);
        const size_t h = len >> 1;
        for (size_t i = 0; i < N; i += len)
        {
            float cr = 1.0f, ci = 0.0f;
            for (size_t k = 0; k < h; ++k)
            {
                const size_t a = i + k, b = a + h;
                const float xr = re[b] * cr - im[b] * ci, xi = re[b] * ci + im[b] * cr;
                re[b] = re[a] - xr; im[b] = im[a] - xi; re[a] += xr; im[a] += xi;
                const float t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t;
            }
        }
    }
    std::vector<float> m (N / 2);
    for (size_t i = 0; i < N / 2; ++i) m[i] = std::hypot (re[i], im[i]);
    return m;
}

struct Onset { int pos; float strength; };

inline std::vector<Onset> detectOnsets (const std::vector<float>& d, double sr, float sens)
{
    const int hop = 256, win = 512, len = (int) d.size();
    const int n = std::max (0, (len - win) / hop);
    std::vector<float> env ((size_t) n), le ((size_t) n), flux ((size_t) n, 0.0f);
    float maxEnv = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        double s = 0; const int o = i * hop;
        for (int j = 0; j < win; ++j)
        {
            const int k = o + j;
            const float x = d[(size_t) k] - 0.95f * (k > 0 ? d[(size_t) k - 1] : 0.0f);
            s += (double) x * x;
        }
        env[(size_t) i] = (float) std::sqrt (s / win);
        maxEnv = std::max (maxEnv, env[(size_t) i]);
        le[(size_t) i] = std::log (1.0e-4f + env[(size_t) i]);
    }
    for (int i = 2; i < n; ++i) flux[(size_t) i] = std::max (0.0f, le[(size_t) i] - le[(size_t) i - 2]);

    const float delta = 1.3f - sens * 1.0f, gate = maxEnv * (0.06f - sens * 0.05f);
    const int minGap = (int) std::lround (0.07 * sr / hop);
    std::vector<Onset> ons;
    int last = -1000000;
    for (int i = 3; i < n - 3; ++i)
    {
        bool isMax = true;
        for (int k = -3; k <= 3; ++k) if (flux[(size_t) (i + k)] > flux[(size_t) i]) { isMax = false; break; }
        if (! isMax) continue;
        float m = 0; int c = 0;
        for (int k = -12; k <= 12; ++k) { const int q = i + k; if (q >= 0 && q < n) { m += flux[(size_t) q]; ++c; } }
        m /= (float) c;
        const float peakEnv = std::max ({ env[(size_t) i], env[(size_t) i + 1], env[(size_t) i + 2] });
        if (flux[(size_t) i] > m * 1.5f + delta && peakEnv > gate && i - last >= minGap)
        {
            ons.push_back ({ std::max (0, (i - 1) * hop), flux[(size_t) i] });
            last = i;
        }
    }
    return ons;
}

struct Pitch { float f0 = 0, conf = 0; };

inline Pitch detectPitch (const std::vector<float>& d, double sr)
{
    const int off = (int) (0.03 * sr), W = 2048, maxLag = (int) (sr / 60.0), minLag = (int) (sr / 1100.0);
    if ((int) d.size() < off + W + maxLag + 1) return {};
    const float* x = d.data() + off;
    double e0 = 0; for (int i = 0; i < W; ++i) e0 += (double) x[i] * x[i];
    if (e0 < 1.0e-6) return {};
    std::vector<float> ns ((size_t) maxLag + 2, 0.0f);
    float mx = 0;
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double r = 0, el = 0;
        for (int i = 0; i < W; ++i) { const float y = x[i + lag]; r += (double) x[i] * y; el += (double) y * y; }
        ns[(size_t) lag] = (float) (r / std::sqrt (e0 * el + 1.0e-12));
        mx = std::max (mx, ns[(size_t) lag]);
    }
    if (mx < 0.5f) return { 0.0f, mx };
    for (int lag = minLag + 1; lag < maxLag; ++lag)
    {
        const float a = ns[(size_t) lag - 1], b = ns[(size_t) lag], c = ns[(size_t) lag + 1];
        if (b >= 0.9f * mx && b >= a && b >= c)
        {
            const float den = a - 2 * b + c;
            const float p = den != 0.0f ? 0.5f * (a - c) / den : 0.0f;
            return { (float) (sr / (lag + p)), b };
        }
    }
    return {};
}

inline void computeFeatures (Slice& s, const std::vector<float>& seg, double sr)
{
    const size_t N = 2048;
    std::vector<float> w (N, 0.0f);
    for (size_t i = 0; i < N && i < seg.size(); ++i)
        w[i] = seg[i] * (float) (0.5 - 0.5 * std::cos (2.0 * kPi * (double) i / (N - 1)));
    const auto mag = fftMag (w);
    const double bin = sr / (double) N;
    double num = 0, den = 0, low = 0, tot = 0;
    for (size_t i = 1; i < mag.size(); ++i)
    {
        const double f = (double) i * bin, p = (double) mag[i] * mag[i];
        num += f * p; den += p; tot += p; if (f < 160.0) low += p;
    }
    s.centroid = den > 0 ? (float) (num / den) : 0.0f;
    s.low = tot > 0 ? (float) (low / tot) : 0.0f;

    int z = 0, pi = 0; float peak = 0; double rms = 0;
    const int L = std::min ((int) seg.size(), (int) (0.08 * sr));
    for (int i = 1; i < L; ++i) if ((seg[(size_t) i] >= 0) != (seg[(size_t) i - 1] >= 0)) ++z;
    for (size_t i = 0; i < seg.size(); ++i)
    {
        const float a = std::abs (seg[i]); rms += (double) seg[i] * seg[i];
        if (a > peak) { peak = a; pi = (int) i; }
    }
    s.rms = (float) std::sqrt (rms / (double) std::max<size_t> (1, seg.size()));
    s.zcr = (float) z / (float) std::max (1, L);

    const int hop = 256;
    s.decay = (float) ((double) seg.size() / sr);
    for (int i = pi; i < (int) seg.size(); i += hop)
    {
        float m = 0;
        for (int j = i; j < i + hop && j < (int) seg.size(); ++j) m = std::max (m, std::abs (seg[(size_t) j]));
        if (m < peak * 0.1f) { s.decay = (float) ((i - pi) / sr); break; }
    }

    const auto p = detectPitch (seg, sr);
    s.f0 = p.f0; s.pconf = p.conf;
    s.midi = s.f0 > 0 ? 69.0f + 12.0f * std::log2 (s.f0 / 440.0f) : 60.0f;

    if (s.low > 0.35f || s.centroid < 450.0f)                    s.cls = clsLow;
    else if (s.pconf > 0.88f && s.f0 > 0 && s.decay > 0.1f)      s.cls = clsTonal;
    else if (s.centroid > 7000.0f)                               s.cls = clsHigh;
    else                                                         s.cls = clsMid;
}

inline std::vector<Slice> analyze (const std::vector<float>& d, double sr, float sens)
{
    auto ons = detectOnsets (d, sr, sens);
    if (ons.size() > 32)
    {
        std::sort (ons.begin(), ons.end(), [] (auto& a, auto& b) { return a.strength > b.strength; });
        ons.resize (32);
        std::sort (ons.begin(), ons.end(), [] (auto& a, auto& b) { return a.pos < b.pos; });
    }
    const int maxLen = (int) (0.9 * sr), len = (int) d.size();
    std::vector<Slice> out;
    for (size_t i = 0; i < ons.size(); ++i)
    {
        const int s = ons[i].pos;
        const int next = i + 1 < ons.size() ? ons[i + 1].pos : s + maxLen;
        const int e = std::min ({ len, next, s + maxLen });
        if (e - s < (int) (0.03 * sr)) continue;
        std::vector<float> seg (d.begin() + s, d.begin() + e);
        Slice sl;
        computeFeatures (sl, seg, sr);
        sl.start = s; sl.end = e;
        // normalisieren + Fades
        float pk = 0; for (auto v : seg) pk = std::max (pk, std::abs (v));
        const float g = pk > 0 ? 0.9f / pk : 1.0f;
        const int n = (int) seg.size();
        const int fi = std::min (n, (int) (sr * 0.002)), fo = std::min (n, (int) (sr * 0.012));
        for (int k = 0; k < n; ++k)
        {
            float v = seg[(size_t) k] * g;
            if (k < fi) v *= (float) k / (float) fi;
            const int r = n - 1 - k; if (r < fo) v *= (float) r / (float) fo;
            seg[(size_t) k] = v;
        }
        sl.data = std::move (seg);
        out.push_back (std::move (sl));
    }
    float mr = 1.0e-9f; for (auto& o : out) mr = std::max (mr, o.rms);
    for (auto& o : out) o.rmsN = o.rms / mr;
    return out;
}

inline std::vector<float> makeDemo (double sr)
{
    int64_t seed = 1234567;
    auto rnd = [&]() { seed = (seed * 16807) % 2147483647; return (float) ((double) seed / 2147483647.0 * 2.0 - 1.0); };
    std::vector<float> buf ((size_t) (7.2 * sr), 0.0f);
    auto add = [&] (double t0, const std::function<float (double)>& fn, double dur)
    {
        const int o = (int) (t0 * sr), n = (int) (dur * sr);
        for (int i = 0; i < n && o + i < (int) buf.size(); ++i) buf[(size_t) (o + i)] += fn (i / sr);
    };
    const double tp = 2.0 * kPi;
    std::function<float (double)> knock = [&] (double t) { const double f = 95 + 110 * std::exp (-t * 40);
        return (float) (0.9 * std::sin (tp * f * t) * std::exp (-t * 22) + 0.35 * rnd() * std::exp (-t * 260)); };
    float lp = 0;
    std::function<float (double)> tap = [&] (double t) { const float nz = rnd(); lp += 0.25f * (nz - lp);
        return (float) ((0.5 * lp + 0.35 * nz) * std::exp (-t * 28) + 0.4 * std::sin (tp * 240 * t) * std::exp (-t * 45)); };
    float prev = 0;
    std::function<float (double)> keys = [&] (double t) { const float nz = rnd(), hp = nz - prev; prev = nz;
        const double burst = std::exp (-t * 45) * (1 + 0.6 * std::sin (t * 180));
        return (float) (0.45 * hp * burst + 0.12 * (std::sin (tp * 6100 * t) + std::sin (tp * 7450 * t)) * std::exp (-t * 30)); };
    std::function<float (double)> clink = [&] (double t) {
        return (float) ((std::sin (tp * 1318.5 * t) + 0.35 * std::sin (tp * 3311 * t) + 0.2 * std::sin (tp * 5070 * t)) * 0.45 * std::exp (-t * 5)
                        + 0.2 * rnd() * std::exp (-t * 400)); };
    std::function<float (double)> hum = [&] (double t) { const double f = 196;
        const double a = std::min (1.0, t / 0.015) * std::min (1.0, (0.55 - t) / 0.08);
        double s = 0; for (int h = 1; h <= 6; ++h) s += std::sin (tp * f * h * t) / h;
        return (float) (0.35 * s * std::max (0.0, a)); };
    std::function<float (double)> click = [&] (double t) { return (float) (0.7 * rnd() * std::exp (-t * 500)); };

    struct Ev { std::function<float (double)>* fn; double d; };
    const Ev seq[] = { {&knock,.4},{&keys,.35},{&tap,.35},{&clink,.9},{&knock,.4},{&click,.2},{&hum,.56},{&tap,.35},
                       {&keys,.35},{&knock,.4},{&click,.2},{&tap,.35},{&hum,.56},{&knock,.4},{&keys,.35},{&clink,.9} };
    double t = 0.15;
    for (auto& e : seq) { add (t, *e.fn, e.d); t += 0.42; }
    for (auto& v : buf) v += 0.0015f * rnd();
    return buf;
}
} // namespace fs
