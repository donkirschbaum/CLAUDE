#include "Generator.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace shaker
{
namespace Generator
{
const char* roleNames[kTracks] = { "MAIN", "AKZENT", "TEXTUR", "FARBE" };
const RoleMix roleMix[kTracks] = { { 0.85f, 0.0f, 100, 180 }, { 0.6f, 0.35f, 100, 160 }, { 0.45f, -0.4f, 140, 250 }, { 0.5f, 0.2f, 180, 250 } };

namespace
{
    // Rate-Indizes: 2 = /2, 4 = x1, 5 = x1.5
    const std::vector<TrackConfig> roleCfg[kTracks] = {
        { { 16, Polymeter, 4 } },
        { { 16, Polymeter, 4 }, { 8, Polymeter, 4 } },
        { { 12, Polyrhythm, 4 }, { 6, Polyrhythm, 4 }, { 5, Polymeter, 4 }, { 7, Polymeter, 4 }, { 6, Polymeter, 5 }, { 9, Polyrhythm, 2 } },
        { { 3, Polyrhythm, 2 }, { 5, Polyrhythm, 4 }, { 4, Polyrhythm, 2 }, { 7, Polymeter, 2 }, { 3, Polyrhythm, 4 }, { 5, Polymeter, 2 } },
    };

    std::vector<int> euclid (int k, int n)
    {
        std::vector<int> r ((size_t) n);
        for (int i = 0; i < n; ++i) r[(size_t) i] = ((i * k) % n) < k ? 1 : 0;
        return r;
    }
    std::vector<int> rotate (const std::vector<int>& a, int k)
    {
        std::vector<int> r (a.size());
        for (size_t i = 0; i < a.size(); ++i) r[i] = a[(i + (size_t) k) % a.size()];
        return r;
    }
    float r2 (juce::Random& rng) { return rng.nextFloat() * 2.0f - 1.0f; }
}

void genRole (int role, bool withCfg, TrackConfig& cfg, float* pat, float density, float ghost, juce::Random& rng)
{
    if (withCfg)
    {
        const auto& opts = roleCfg[role];
        cfg = opts[(size_t) rng.nextInt ((int) opts.size())];
    }
    const int n = juce::jlimit (1, kMaxSteps, cfg.steps);
    std::vector<float> p ((size_t) n, 0.0f);

    if (role == 0)
    {
        static const float V[5][4] = { { 0.6f, 0.4f, 1.0f, 0.42f }, { 0.6f, 0, 1.0f, 0 }, { 0.6f, 0, 1.0f, 0.45f }, { 0.6f, 0.42f, 1.0f, 0 }, { 0.5f, 0.4f, 1.0f, 0.65f } };
        const int vi = rng.nextInt (5);
        if (n % 4 == 0)
            for (int i = 0; i < n; ++i) p[(size_t) i] = V[vi][i % 4];
        else
        {
            auto e = euclid (juce::jmax (1, (int) std::round (n * 0.6)), n);
            for (int i = 0; i < n; ++i) p[(size_t) i] = e[(size_t) i] ? (i % 2 ? 0.9f : 0.6f) : 0.0f;
        }
    }
    else if (role == 1)
    {
        std::vector<int> hits;
        if (n == 16)
        {
            static const std::vector<std::vector<int>> o = { { 2, 6, 10, 14 }, { 3, 7, 11, 15 }, { 0, 3, 6, 10, 12 }, { 0, 3, 6, 8, 11, 14 }, { 6, 14 }, { 2, 7, 10, 15 } };
            hits = o[(size_t) rng.nextInt ((int) o.size())];
        }
        else if (n == 8)
        {
            static const std::vector<std::vector<int>> o = { { 1, 3, 5, 7 }, { 1, 4, 7 }, { 3, 7 }, { 2, 5, 7 } };
            hits = o[(size_t) rng.nextInt ((int) o.size())];
        }
        if (! hits.empty())
        {
            for (int i : hits) if (i < n) p[(size_t) i] = 0.85f + rng.nextFloat() * 0.15f;
        }
        else
        {
            auto e = rotate (euclid (juce::jmax (1, (int) std::round (n * 0.3)), n), rng.nextInt (n));
            for (int i = 0; i < n; ++i) if (e[(size_t) i]) p[(size_t) i] = 0.9f;
        }
    }
    else if (role == 2)
    {
        const int k = juce::jmax (2, (int) std::round (n * (0.5 + rng.nextFloat() * 0.25)));
        auto e = rotate (euclid (juce::jmin (k, n), n), rng.nextInt (n));
        for (int i = 0; i < n; ++i) if (e[(size_t) i]) p[(size_t) i] = i == 0 ? 0.65f : 0.42f + rng.nextFloat() * 0.12f;
        if (std::none_of (p.begin(), p.end(), [] (float v) { return v > 0; })) p[0] = 0.6f;
    }
    else
    {
        const int k = n <= 4 ? 1 : (rng.nextInt (3) == 0 ? 1 : 2);
        auto e = rotate (euclid (k, n), rng.nextInt (n));
        for (int i = 0; i < n; ++i) if (e[(size_t) i]) p[(size_t) i] = 0.8f + rng.nextFloat() * 0.15f;
    }

    // Dichte und Ghosts ergänzen nur, wenn die Regler über 0 stehen
    for (int i = 0; i < n; ++i)
        if (p[(size_t) i] == 0.0f)
        {
            if (rng.nextFloat() < density * 0.35f) p[(size_t) i] = 0.4f + rng.nextFloat() * 0.3f;
            else if (rng.nextFloat() < ghost * 0.6f) p[(size_t) i] = 0.14f + rng.nextFloat() * 0.14f;
        }

    for (int i = 0; i < kMaxSteps; ++i)
    {
        float v = i < n ? p[(size_t) i] : 0.0f;
        if (v > 0) v = std::round (juce::jlimit (0.05f, 1.0f, v + r2 (rng) * 0.04f) * 100.0f) / 100.0f;
        pat[i] = v;
    }
}

void mutate (float* pat, int steps, float amount, juce::Random& rng)
{
    for (int i = 0; i < steps; ++i)
        if (rng.nextFloat() < amount)
        {
            const float v = pat[i];
            if (v > 0) pat[i] = rng.nextFloat() < 0.4f ? 0.0f : juce::jlimit (0.12f, 1.0f, v + r2 (rng) * 0.35f);
            else pat[i] = rng.nextFloat() < 0.5f ? 0.15f + rng.nextFloat() * 0.75f : 0.0f;
        }
}

std::array<int, kTracks> pickSamples (const SampleLibrary& lib, juce::Random& rng)
{
    std::array<int, kTracks> out { -1, -1, -1, -1 };
    std::vector<int> ids;
    for (int i = 0; i < lib.size(); ++i)
        if (! lib.get (i)->hidden.load()) ids.push_back (i);
    if (ids.empty()) return out;

    std::vector<int> sorted = ids;
    std::sort (sorted.begin(), sorted.end(), [&] (int a, int b) { return lib.get (a)->zcr < lib.get (b)->zcr; });
    std::map<int, float> rank;
    for (size_t i = 0; i < sorted.size(); ++i)
        rank[sorted[i]] = sorted.size() > 1 ? (float) i / (float) (sorted.size() - 1) : 0.5f;

    // Zielhelligkeit (0 dunkel … 1 hell) und hörbare Länge pro Rolle
    static const float prof[kTracks][2] = { { 0.75f, 0.13f }, { 0.9f, 0.08f }, { 0.35f, 0.15f }, { 0.55f, 0.3f } };
    const int fam = rng.nextInt (3);
    std::vector<int> used;
    for (int role = 0; role < kTracks; ++role)
    {
        int best = -1;
        float bs = -1e9f;
        for (int id : ids)
        {
            if (std::find (used.begin(), used.end(), id) != used.end()) continue;
            const auto* s = lib.get (id);
            const float sc = -std::abs (rank[id] - prof[role][0]) - 0.45f * std::abs (std::log (s->dur / prof[role][1]))
                             + (s->fam == fam ? 0.3f : 0.0f) + rng.nextFloat() * 0.75f;
            if (sc > bs) { bs = sc; best = id; }
        }
        if (best < 0) best = ids[(size_t) rng.nextInt ((int) ids.size())];
        used.push_back (best);
        out[(size_t) role] = best;
    }
    return out;
}
} // namespace Generator
} // namespace shaker
