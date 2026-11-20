// Grids 8 – Pattern-Engine (topografische Drum-Karten nach dem Vorbild von Mutable Instruments Grids)
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace g8
{
constexpr double kPi = 3.14159265358979323846;
constexpr int NT = 8;      // Spuren
constexpr int NSTEP = 32;  // Steps pro Karten-Pattern
constexpr int NPARAMS_SNAP = 2 + NT * 3; // Master X/Y + je Spur X/Y/Dichte

struct TrackInfo { const char* name; const char* file; int gm; int role; };
inline const TrackInfo TRACKS[NT] = {
    { "Kick",  "1kick.wav",  36, 0 },
    { "Clap",  "2clap.wav",  39, 1 },
    { "Hat 1", "3hat.wav",   46, 3 }, // langes Sample -> Open-Hat-Vorlagen
    { "Hat 2", "4hat.wav",   42, 2 }, // kurzes Sample -> 16tel-Vorlagen
    { "Snare", "5snare.wav", 38, 4 },
    { "Tom 1", "6tom1.wav",  50, 5 },
    { "Tom 2", "7tom2.wav",  45, 6 },
    { "Conga", "8conga.wav", 63, 7 },
};

// Je Rolle: [0] = gerade (X links), [1] = gebrochen (X rechts); Index 0..4 = sparsam..dicht (Y)
inline const char* const TPL[8][2][5] = {
 {{"9000000000000000","9000000090000000","9000900090009000","9000900090009040","9030900090409060"},
  {"9000000000700000","9000000700900000","9006000000900070","9070006009007050","9075096090759060"}},
 {{"0000000090000000","0000900000009000","0000900000009004","0000900400009050","0050900400539050"},
  {"0000000000009000","0000900000000090","0000900005009000","0030900500609040","0305907040539075"}},
 {{"0090009000900090","9060906090609060","9595959595959595","9696969699696969","9797979797979797"},
  {"0050009000500090","9050905090509050","9370937093709370","9699596995699569","9779977997799779"}},
 {{"0000000000000050","0000000000000090","0090000000900000","0090009000900090","0095009500950095"},
  {"0000000000000900","0000009000000090","0060000900600090","0090060000906009","0596059609560596"}},
 {{"0000000090000000","0000900000009000","0000900000009030","0030903000509060","0050903050309565"},
  {"0000900000000900","0000900300090000","0000900500090050","0050903000590305","5095909559059595"}},
 {{"0000000000000060","0000000000009000","0000000000903090","0000006000906090","0060300600906969"},
  {"0000000000000700","0000000700000070","0060000700600070","0006070006090709","0609060906909696"}},
 {{"0000000000000006","0000000000000090","0000000000000909","0000060000009099","0603060306039999"},
  {"0000000000070000","0000070000000700","0070000600700006","0700600700700990","7007007007007070"}},
 {{"0000009000000000","0000009000900000","0050009000900050","0050609050900560","5959695959695969"},
  {"0000000900000000","0007000000700000","0070070000700700","0070070090700709","7097079079709707"}},
};

inline float clamp01 (float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
inline float lerpf (float a, float b, float t) { return a + (b - a) * t; }
inline int64_t posmod (int64_t a, int64_t m) { auto r = a % m; return r < 0 ? r + m : r; }

struct Mulberry
{
    uint32_t a;
    explicit Mulberry (uint32_t seed) : a (seed) {}
    double next()
    {
        a += 0x6D2B79F5u;
        uint32_t t = a;
        t = (t ^ (t >> 15)) * (1u | t);
        t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
        return (double) (t ^ (t >> 14)) / 4294967296.0;
    }
};

inline double hash3 (uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t h = a * 374761393u + b * 668265263u + c * 1442695041u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (double) h / 4294967296.0;
}

inline float metric (int s)
{
    int q = s % 16;
    return q == 0 ? 1.f : q == 8 ? .85f : (q % 4 == 0) ? .7f : (q % 2 == 0) ? .45f : .28f;
}

struct PatternMap
{
    std::array<std::array<std::array<uint8_t, NSTEP>, 25>, NT> nodes {};

    static void expand (const char* str, Mulberry& rng, float v, float out[NSTEP])
    {
        for (int s = 0; s < NSTEP; ++s)
        {
            int d = str[s % 16] - '0';
            if (s >= 16 && rng.next() < .10 + .22 * v)
            {
                int nb = str[(s + (rng.next() < .5 ? 1 : 15)) % 16] - '0';
                d = rng.next() < .5 ? nb : (int) std::round (d * rng.next());
            }
            if (s >= 28 && v > .5f && rng.next() < .35)
                d = std::max (d, 4 + (int) std::floor (rng.next() * 5));
            out[s] = d / 9.f * 255.f;
        }
    }

    void generate (uint32_t seed)
    {
        Mulberry rng (seed);
        for (int t = 0; t < NT; ++t)
        {
            const int role = TRACKS[t].role;
            for (int j = 0; j < 5; ++j)
                for (int i = 0; i < 5; ++i)
                {
                    const float u = i / 4.f, v = j / 4.f;
                    int ka = (int) std::round (v * 4 + (rng.next() - .5) * 1.3);
                    int kb = (int) std::round (v * 4 + (rng.next() - .5) * 1.3);
                    ka = std::clamp (ka, 0, 4); kb = std::clamp (kb, 0, 4);
                    float A[NSTEP], B[NSTEP];
                    expand (TPL[role][0][ka], rng, v, A);
                    expand (TPL[role][1][kb], rng, v, B);
                    auto& pat = nodes[(size_t) t][(size_t) (j * 5 + i)];
                    for (int s = 0; s < NSTEP; ++s)
                    {
                        float L = A[s] * (1 - u) + B[s] * u + (float) (rng.next() - .5) * (30 + 70 * u);
                        const int sh = (u > .5f && rng.next() < .4) ? 1 : 0;
                        const float ghost = (float) rng.next() * (35 + 120 * v) * metric (s + sh);
                        if (L < ghost) L = ghost;
                        pat[(size_t) s] = (uint8_t) std::clamp ((int) std::round (L), 0, 255);
                    }
                    if (role == 0 && u < .6f) pat[0] = 255; // Kick-Anker
                }
        }
    }

    float level (int t, int s, float x, float y) const
    {
        const float fx = x * 4, fy = y * 4;
        const int i = std::min (3, (int) std::floor (fx)), j = std::min (3, (int) std::floor (fy));
        const float ax = fx - i, ay = fy - j;
        const auto& n = nodes[(size_t) t];
        const float a = n[(size_t) (j * 5 + i)][(size_t) s], b = n[(size_t) (j * 5 + i + 1)][(size_t) s];
        const float c = n[(size_t) ((j + 1) * 5 + i)][(size_t) s], d = n[(size_t) ((j + 1) * 5 + i + 1)][(size_t) s];
        return lerpf (lerpf (a, b, ax), lerpf (c, d, ax), ay);
    }
};

// Modulation: 0 Aus, 1 Sinus, 2 Dreieck, 3 Saege, 4 S&H, 5 Drift
inline const char* const SHAPE_NAMES[6] = { "Aus", "Sinus", "Dreieck", "Saegezahn", "S&H", "Drift" };
inline const float RATE_BARS[7] = { .25f, .5f, 1.f, 2.f, 4.f, 8.f, 16.f };
inline const char* const RATE_NAMES[7] = { "1/4 Takt", "1/2 Takt", "1 Takt", "2 Takte", "4 Takte", "8 Takte", "16 Takte" };

inline float modVal (int shape, int rateIdx, double g, uint32_t seed, int key, int axis)
{
    if (shape <= 0) return 0.f;
    const double rate = RATE_BARS[std::clamp (rateIdx, 0, 6)];
    const double q = g / (rate * 16.0);
    const double p = q + axis * .25, f = p - std::floor (p);
    const auto k3 = (uint32_t) (key * 3 + axis);
    switch (shape)
    {
        case 1: return (float) std::sin (2 * kPi * p);
        case 2: return (float) (1 - 4 * std::abs (f - .5));
        case 3: return (float) (2 * f - 1);
        case 4: return (float) (hash3 (seed, k3, (uint32_t) (int64_t) std::floor (q)) * 2 - 1);
        case 5:
        {
            const auto k = (int64_t) std::floor (q);
            const double t = (1 - std::cos (kPi * (q - (double) k))) / 2;
            const double a = hash3 (seed, k3, (uint32_t) k) * 2 - 1, b = hash3 (seed, k3, (uint32_t) (k + 1)) * 2 - 1;
            return (float) (a + (b - a) * t);
        }
        default: return 0.f;
    }
}

struct EffParams { float x, y, d, c; };

} // namespace g8
