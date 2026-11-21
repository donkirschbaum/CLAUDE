#include "Wavetables.h"

#include <complex>
#include <functional>
#include <random>
#include <algorithm>

namespace ws
{
namespace
{
using cpx = std::complex<double>;
constexpr double twoPi = 6.283185307179586;
constexpr double pi    = 3.141592653589793;

void fft (std::vector<cpx>& a, bool inverse)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = twoPi / (double) len * (inverse ? 1.0 : -1.0);
        const cpx wlen (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            cpx w (1.0);
            for (size_t j = 0; j < len / 2; ++j)
            {
                const cpx u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

constexpr int maxHarmonics = 511;
constexpr int osSize = 8192; // oversampled size for time-domain generators

using Spectrum = std::vector<cpx>; // index k = harmonic k (0 unused)

// --- helpers for generators ---------------------------------------------------
double saw (double t)    { t -= std::floor (t); return 2.0 * t - 1.0; }
double square (double t) { t -= std::floor (t); return t < 0.5 ? 1.0 : -1.0; }
double gauss (double x, double centre, double width) { const double d = (x - centre) / width; return std::exp (-0.5 * d * d); }

Spectrum fromTime (const std::function<double (double)>& fn)
{
    std::vector<cpx> buf ((size_t) osSize);
    for (int i = 0; i < osSize; ++i)
        buf[(size_t) i] = cpx (fn ((double) i / osSize), 0.0);
    fft (buf, false);
    Spectrum s ((size_t) maxHarmonics + 1);
    for (int k = 1; k <= maxHarmonics; ++k)
        s[(size_t) k] = buf[(size_t) k] / (double) osSize;
    return s;
}

// amplitudes for sine partials: x(t) = sum a_k sin(2 pi k t + phase_k)
Spectrum fromSines (const std::function<double (int)>& amp, const std::function<double (int)>& phase = nullptr)
{
    Spectrum s ((size_t) maxHarmonics + 1);
    for (int k = 1; k <= maxHarmonics; ++k)
    {
        const double a = amp (k);
        const double ph = phase ? phase (k) : 0.0;
        s[(size_t) k] = std::polar (a * 0.5, ph - pi * 0.5);
    }
    return s;
}

// Classic shapes as exact spectra so that they morph smoothly
double triAmp (int k)    { return (k % 2 == 1) ? (8.0 / (pi * pi)) / (k * k) * ((k / 2) % 2 == 0 ? 1.0 : -1.0) : 0.0; }
double sawAmp (int k)    { return (2.0 / pi) / k * (k % 2 == 1 ? 1.0 : -1.0); }
double squareAmp (int k) { return (k % 2 == 1) ? (4.0 / pi) / k : 0.0; }

Spectrum makeFrame (int table, double p)
{
    switch (table)
    {
        case 0: // Analog Morph: sine -> triangle -> saw -> square
            return fromSines ([p] (int k)
            {
                const double seg = p * 3.0;
                const double w = seg - std::floor (std::min (seg, 2.999));
                const int s = (int) std::min (seg, 2.999);
                auto shape = [k] (int idx)
                {
                    switch (idx)
                    {
                        case 0: return k == 1 ? 1.0 : 0.0;
                        case 1: return triAmp (k);
                        case 2: return sawAmp (k);
                        default: return squareAmp (k);
                    }
                };
                return shape (s) * (1.0 - w) + shape (s + 1) * w;
            });

        case 1: // Pulse Width
        {
            const double duty = 0.5 - 0.47 * p;
            return fromTime ([duty] (double t) { return t < duty ? 1.0 : -1.0; });
        }

        case 2: // Vowel formants (A E I O U), harmonic index relative to 110 Hz
        {
            static const double f[5][3] = { { 800, 1150, 2900 }, { 400, 1700, 2600 }, { 300, 2200, 3000 },
                                            { 450, 800, 2830 }, { 325, 700, 2530 } };
            const double seg = p * 4.0;
            const int s = (int) std::min (seg, 3.999);
            const double w = seg - s;
            double F[3];
            for (int i = 0; i < 3; ++i)
                F[i] = (f[s][i] * (1.0 - w) + f[s + 1][i] * w) / 110.0;
            return fromSines ([F] (int k)
            {
                const double env = gauss (k, F[0], 1.3) + 0.6 * gauss (k, F[1], 2.0) + 0.3 * gauss (k, F[2], 2.6);
                return (0.15 / std::sqrt ((double) k) + env) / std::sqrt ((double) k);
            });
        }

        case 3: // Harmonic Climb: fundamental plus one sweeping partial
        {
            const double centre = 2.0 * std::pow (32.0, p);
            return fromSines ([centre] (int k)
            {
                return (k == 1 ? 1.0 : 0.0) + 0.85 * gauss (k, centre, 0.6 + centre * 0.04);
            });
        }

        case 4: // Hard sync saw
        {
            const double ratio = 1.0 + 7.0 * p;
            return fromTime ([ratio] (double t) { return saw (t * ratio) * (1.0 - t * 0.3); });
        }

        case 5: // FM Bell (ratio 3)
        {
            const double index = 6.0 * p;
            return fromTime ([index] (double t) { return std::sin (twoPi * t + index * std::sin (twoPi * 3.0 * t)); });
        }

        case 6: // Wavefold
        {
            const double gain = 1.0 + 9.0 * p;
            return fromTime ([gain] (double t) { return std::sin (pi * 0.5 * gain * std::sin (twoPi * t)); });
        }

        case 7: // Bitcrush steps
        {
            const double steps = 2.0 + (1.0 - p) * 30.0;
            return fromTime ([steps] (double t)
            {
                const double v = std::sin (twoPi * t) * 0.6 + saw (t) * 0.4;
                return std::round (v * steps * 0.5) / (steps * 0.5);
            });
        }

        case 8: // Drawbar organ registrations
        {
            static const int harm[7] = { 1, 2, 3, 4, 6, 8, 10 };
            static const double reg[4][7] = { { 1.0, 0.8, 0.0, 0.0, 0.0, 0.0, 0.0 },
                                              { 1.0, 0.0, 0.8, 0.6, 0.0, 0.0, 0.0 },
                                              { 1.0, 0.7, 0.7, 0.5, 0.5, 0.4, 0.0 },
                                              { 1.0, 0.9, 0.9, 0.9, 0.8, 0.8, 0.8 } };
            const double seg = p * 3.0;
            const int s = (int) std::min (seg, 2.999);
            const double w = seg - s;
            return fromSines ([s, w] (int k)
            {
                for (int i = 0; i < 7; ++i)
                    if (harm[i] == k) return reg[s][i] * (1.0 - w) + reg[s + 1][i] * w;
                return 0.0;
            });
        }

        case 9: // Odd / Even balance with brightness tilt
            return fromSines ([p] (int k)
            {
                const double tilt = std::pow ((double) k, -(1.4 - 0.5 * p));
                return (k % 2 == 0 ? (1.0 - p) : 1.0) * tilt;
            });

        case 10: // Resonant saw
        {
            const double centre = 2.0 + 60.0 * p * p;
            return fromSines ([centre] (int k)
            {
                return (1.0 / k) * (1.0 + 8.0 * gauss (k, centre, 1.5 + centre * 0.05));
            });
        }

        case 11: // Spectral noise: fixed random partials with moving tilt
        {
            static std::vector<double> amps, phases;
            if (amps.empty())
            {
                std::mt19937 rng (1234);
                std::uniform_real_distribution<double> d (0.0, 1.0);
                for (int k = 0; k <= maxHarmonics; ++k) { amps.push_back (0.2 + d (rng)); phases.push_back (d (rng) * twoPi); }
            }
            const double exponent = 1.5 - 1.2 * p;
            return fromSines ([exponent] (int k) { return amps[(size_t) k] * std::pow ((double) k, -exponent); },
                              [] (int k) { return phases[(size_t) k]; });
        }

        case 12: // Comb saw
        {
            const double d = 0.02 + 0.45 * p;
            return fromTime ([d] (double t) { return 0.5 * (saw (t) - saw (t + d)); });
        }

        case 13: // Phase distortion
        {
            const double c = 0.5 - 0.48 * p;
            return fromTime ([c] (double t)
            {
                const double tp = t < c ? 0.5 * t / c : 0.5 + 0.5 * (t - c) / (1.0 - c);
                return -std::cos (twoPi * tp);
            });
        }

        case 14: // Square sync
        {
            const double ratio = 1.0 + 5.0 * p;
            return fromTime ([ratio] (double t) { return square (t * ratio); });
        }

        default: // 15 Glass partials
            return fromSines ([p] (int k)
            {
                static const int set[8] = { 1, 3, 5, 7, 9, 11, 13, 17 };
                for (int i = 0; i < 8; ++i)
                    if (set[i] == k)
                    {
                        if (k == 1) return 1.0;
                        const double m = 0.5 + 0.5 * std::sin (twoPi * (p * k * 0.15 + k * 0.17));
                        return m * std::pow ((double) k, -0.7);
                    }
                return 0.0;
            });
    }
}

} // namespace

const WavetableBank& WavetableBank::get()
{
    static const WavetableBank bank;
    return bank;
}

WavetableBank::WavetableBank()
{
    data.assign ((size_t) numTables * numFrames * numLevels * stride, 0.0f);
    std::vector<cpx> buf ((size_t) tableSize);
    std::vector<double> level0 ((size_t) tableSize);

    for (int t = 0; t < numTables; ++t)
    {
        for (int f = 0; f < numFrames; ++f)
        {
            const Spectrum spec = makeFrame (t, (double) f / (numFrames - 1));
            double norm = 1.0;

            for (int l = 0; l < numLevels; ++l)
            {
                const int maxH = std::min (maxHarmonics, 512 >> l);
                std::fill (buf.begin(), buf.end(), cpx (0.0));
                for (int k = 1; k <= maxH; ++k)
                {
                    buf[(size_t) k] = spec[(size_t) k];
                    buf[(size_t) (tableSize - k)] = std::conj (spec[(size_t) k]);
                }
                fft (buf, true);

                if (l == 0)
                {
                    double peak = 0.0;
                    for (int i = 0; i < tableSize; ++i)
                        peak = std::max (peak, std::abs (buf[(size_t) i].real()));
                    norm = peak > 1.0e-9 ? 1.0 / peak : 0.0;
                }

                float* dst = data.data() + ((size_t) ((t * numFrames + f) * numLevels + l)) * stride;
                for (int i = 0; i < tableSize; ++i)
                    dst[i] = (float) (buf[(size_t) i].real() * norm);
                dst[tableSize] = dst[0];
            }
        }
    }
}

} // namespace ws
