// Onset detection, feature extraction, role assignment, sound design and groove generation.
#include "fs_internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef FS_DEBUG
#include <stdio.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------- FFT
static void fft(float* re, float* im, int n) {
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { float t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (int len = 2; len <= n; len <<= 1) {
        double ang = -2.0 * M_PI / len;
        float wr = (float)cos(ang), wi = (float)sin(ang);
        for (int i = 0; i < n; i += len) {
            float cr = 1.0f, ci = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                int a = i + k, b = i + k + len / 2;
                float tr = re[b] * cr - im[b] * ci;
                float ti = re[b] * ci + im[b] * cr;
                re[b] = re[a] - tr; im[b] = im[a] - ti;
                re[a] += tr; im[a] += ti;
                float ncr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr; cr = ncr;
            }
        }
    }
}

// ---------------------------------------------------------------- Biquad (RBJ)
typedef struct { float b0, b1, b2, a1, a2, z1, z2; } biquad;
enum { BQ_LP, BQ_HP, BQ_BP, BQ_PEAK };

static void bq_init(biquad* f, int type, double sr, double freq, double q, double gainDb) {
    if (freq > sr * 0.45) freq = sr * 0.45;
    if (freq < 10) freq = 10;
    double w = 2.0 * M_PI * freq / sr, cs = cos(w), sn = sin(w), al = sn / (2.0 * q);
    double A = pow(10.0, gainDb / 40.0);
    double b0 = 0, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
    switch (type) {
        case BQ_LP: b0 = (1 - cs) / 2; b1 = 1 - cs; b2 = (1 - cs) / 2; a0 = 1 + al; a1 = -2 * cs; a2 = 1 - al; break;
        case BQ_HP: b0 = (1 + cs) / 2; b1 = -(1 + cs); b2 = (1 + cs) / 2; a0 = 1 + al; a1 = -2 * cs; a2 = 1 - al; break;
        case BQ_BP: b0 = al; b1 = 0; b2 = -al; a0 = 1 + al; a1 = -2 * cs; a2 = 1 - al; break;
        case BQ_PEAK: b0 = 1 + al * A; b1 = -2 * cs; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * cs; a2 = 1 - al / A; break;
    }
    f->b0 = (float)(b0 / a0); f->b1 = (float)(b1 / a0); f->b2 = (float)(b2 / a0);
    f->a1 = (float)(a1 / a0); f->a2 = (float)(a2 / a0); f->z1 = f->z2 = 0;
}
static inline float bq_run(biquad* f, float x) {
    float y = f->b0 * x + f->z1;
    f->z1 = f->b1 * x - f->a1 * y + f->z2;
    f->z2 = f->b2 * x - f->a2 * y;
    return y;
}
static void bq_apply(float* x, uint32_t n, int type, double sr, double freq, double q, double gainDb) {
    biquad f; bq_init(&f, type, sr, freq, q, gainDb);
    for (uint32_t i = 0; i < n; i++) x[i] = bq_run(&f, x[i]);
}

// ---------------------------------------------------------------- helpers
static float peak_abs(const float* x, uint32_t n) {
    float p = 0; for (uint32_t i = 0; i < n; i++) { float a = fabsf(x[i]); if (a > p) p = a; } return p;
}
static void normalize(float* x, uint32_t n, float target) {
    float p = peak_abs(x, n);
    if (p < 1e-9f) return;
    float g = target / p;
    for (uint32_t i = 0; i < n; i++) x[i] *= g;
}
static float gauss(float x, float mu, float sigma) { float d = (x - mu) / sigma; return expf(-0.5f * d * d); }

// ---------------------------------------------------------------- Onset detection
#define HOP 256
#define WIN 1024

int32_t fs_detect_slices(const float* x, uint32_t n, double sr, float sensitivity, fs_slice* out, int32_t maxOut) {
    if (!x || n < WIN || !out || maxOut <= 0) return 0;
    if (sensitivity < 0) sensitivity = 0;
    if (sensitivity > 1) sensitivity = 1;
    uint32_t frames = (n - WIN) / HOP + 1;
    float* flux = (float*)calloc(frames, sizeof(float));
    float* energy = (float*)calloc(frames, sizeof(float));
    float* prev = (float*)calloc(WIN / 2, sizeof(float));
    float* re = (float*)malloc(sizeof(float) * WIN);
    float* im = (float*)malloc(sizeof(float) * WIN);
    float* win = (float*)malloc(sizeof(float) * WIN);
    if (!flux || !energy || !prev || !re || !im || !win) { free(flux); free(energy); free(prev); free(re); free(im); free(win); return 0; }
    for (int i = 0; i < WIN; i++) win[i] = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * i / (WIN - 1));

    float globalPeak = peak_abs(x, n);
    if (globalPeak < 1e-6f) { free(flux); free(energy); free(prev); free(re); free(im); free(win); return 0; }

    for (uint32_t f = 0; f < frames; f++) {
        const float* s = x + (size_t)f * HOP;
        double e = 0;
        for (int i = 0; i < WIN; i++) { float v = s[i] / globalPeak; re[i] = v * win[i]; im[i] = 0; e += v * v; }
        energy[f] = (float)(e / WIN);
        fft(re, im, WIN);
        double fl = 0;
        for (int k = 1; k < WIN / 2; k++) {
            float m = logf(1.0f + 1000.0f * sqrtf(re[k] * re[k] + im[k] * im[k]) / WIN);
            float d = m - prev[k];
            if (d > 0 && f > 0) fl += d;
            prev[k] = m;
        }
        flux[f] = (float)fl;
    }
    float fmax = 0; for (uint32_t f = 0; f < frames; f++) if (flux[f] > fmax) fmax = flux[f];
    if (fmax > 0) for (uint32_t f = 0; f < frames; f++) flux[f] /= fmax;
    // combine with log-energy rise so bassy hits (few FFT bins) are found too
    {
        float emx = 1e-12f; for (uint32_t f = 0; f < frames; f++) if (energy[f] > emx) emx = energy[f];
        float floorE = emx * 1e-6f;
        for (uint32_t f = 1; f < frames; f++) {
            float d = 10.0f * log10f((energy[f] + floorE) / (energy[f - 1] + floorE));
            float r = d > 0 ? d / 12.0f : 0.0f;
            if (r > 1.0f) r = 1.0f;
            if (r > flux[f]) flux[f] = r;
        }
    }

    // energy gate: hits must reach at least this level (relative to loudest frame)
    float emax = 0; for (uint32_t f = 0; f < frames; f++) if (energy[f] > emax) emax = energy[f];
    float gateDb = -24.0f - 24.0f * sensitivity;            // -24 … -48 dB
    float gate = emax * powf(10.0f, gateDb / 10.0f);
    float delta = 0.04f + (1.0f - sensitivity) * 0.22f;
    uint32_t minGap = (uint32_t)(0.060 * sr / HOP) + 1;

    uint32_t* onsets = (uint32_t*)malloc(sizeof(uint32_t) * (frames + 1));
    int nOn = 0;
    uint32_t last = 0; int haveLast = 0;
    for (uint32_t f = 1; f + 1 < frames; f++) {
        int lo = (int)f - 10, hi = (int)f + 10;
        if (lo < 0) lo = 0;
        if (hi >= (int)frames) hi = frames - 1;
        float mean = 0; for (int j = lo; j <= hi; j++) mean += flux[j]; mean /= (hi - lo + 1);
        float thr = mean * 1.4f + delta;
        if (flux[f] > thr && flux[f] >= flux[f - 1] && flux[f] > flux[f + 1]) {
            if (haveLast && f - last < minGap) continue;
            float eMax = 0;
            for (uint32_t j = f; j < f + 6 && j < frames; j++) if (energy[j] > eMax) eMax = energy[j];
            if (eMax < gate) continue;
            onsets[nOn++] = f; last = f; haveLast = 1;
        }
    }

    // refine onset to sample position and build slices
    int32_t count = 0;
    uint32_t maxLen = (uint32_t)(1.5 * sr);
    uint32_t preroll = (uint32_t)(0.002 * sr);
    uint32_t hold = (uint32_t)(0.005 * sr); if (hold < 16) hold = 16;
    uint32_t* starts = (uint32_t*)malloc(sizeof(uint32_t) * (nOn + 1));
    for (int i = 0; i < nOn; i++) {
        uint32_t a = onsets[i] > 0 ? (onsets[i] - 1) * HOP : 0;
        uint32_t b = onsets[i] * HOP + WIN; if (b > n) b = n;
        float lp = peak_abs(x + a, b - a);
        uint32_t s = a;
        for (uint32_t j = a; j < b; j++) if (fabsf(x[j]) >= lp * 0.12f) { s = j; break; }
        s = s > preroll ? s - preroll : 0;
        if (i > 0 && s <= starts[i - 1]) s = starts[i - 1] + 1;
        starts[i] = s;
    }
    if (nOn == 0) { starts[0] = 0; nOn = 1; } // no transients: use whole recording

    for (int i = 0; i < nOn && count < maxOut; i++) {
        uint32_t s = starts[i];
        uint32_t e = (i + 1 < nOn) ? starts[i + 1] : n;
        if (e - s > maxLen) e = s + maxLen;
        float p = peak_abs(x + s, e - s);
        if (p < globalPeak * 0.02f) continue;
        // trim tail below -45 dB of slice peak
        float tthr = p * 0.0056f;
        uint32_t end = e;
        while (end > s + hold) {
            float hp = peak_abs(x + end - hold, hold);
            if (hp > tthr) break;
            end -= hold;
        }
        if (end - s < (uint32_t)(0.015 * sr)) continue;
        fs_slice* sl = &out[count++];
        memset(sl, 0, sizeof(*sl));
        sl->start = s; sl->end = end; sl->peak = p;
    }
    free(starts); free(onsets);
    free(flux); free(energy); free(prev); free(re); free(im); free(win);

    // features
    const int N = 4096;
    float* fre = (float*)malloc(sizeof(float) * N);
    float* fim = (float*)malloc(sizeof(float) * N);
    for (int32_t i = 0; i < count && fre && fim; i++) {
        fs_slice* sl = &out[i];
        uint32_t len = sl->end - sl->start;
        const float* s = x + sl->start;
        uint32_t m = len < 2048 ? len : 2048;
        double sum2 = 0; uint32_t zc = 0;
        for (uint32_t j = 0; j < len; j++) sum2 += (double)s[j] * s[j];
        for (uint32_t j = 1; j < m; j++) if ((s[j] >= 0) != (s[j - 1] >= 0)) zc++;
        sl->rms = (float)sqrt(sum2 / len);
        sl->zcr = (float)zc / (float)m;
        for (int j = 0; j < N; j++) { fre[j] = 0; fim[j] = 0; }
        for (uint32_t j = 0; j < m; j++) fre[j] = s[j] * (0.5f - 0.5f * cosf(2.0f * (float)M_PI * j / (m - 1)));
        fft(fre, fim, N);
        double tot = 0, wsum = 0, lo = 0, hi = 0, logSum = 0, linSum = 0; int cnt = 0;
        double binHz = sr / N;
        for (int k = 1; k < N / 2; k++) {
            double pw = (double)fre[k] * fre[k] + (double)fim[k] * fim[k];
            double hz = k * binHz;
            tot += pw; wsum += pw * hz;
            if (hz < 180) lo += pw;
            if (hz > 6000) hi += pw;
            if (hz > 100 && hz < 16000) { logSum += log(pw + 1e-12); linSum += pw; cnt++; }
        }
        sl->centroid = tot > 0 ? (float)(wsum / tot) : 1000.0f;
        sl->lowRatio = tot > 0 ? (float)(lo / tot) : 0;
        sl->highRatio = tot > 0 ? (float)(hi / tot) : 0;
        sl->flatness = (cnt > 0 && linSum > 0) ? (float)(exp(logSum / cnt) / (linSum / cnt)) : 0;
        if (sl->flatness > 1) sl->flatness = 1;
        // decay: from peak to -20 dB using 5 ms windows
        uint32_t pk = 0; float pv = 0;
        for (uint32_t j = 0; j < len; j++) if (fabsf(s[j]) > pv) { pv = fabsf(s[j]); pk = j; }
        uint32_t w = (uint32_t)(0.005 * sr); if (w < 8) w = 8;
        uint32_t d = pk;
        while (d + w < len && peak_abs(s + d, w) > pv * 0.1f) d += w;
        sl->decayMs = (float)((d - pk) * 1000.0 / sr);
    }
    free(fre); free(fim);
    return count;
}

// ---------------------------------------------------------------- Role scoring
static float role_score(const fs_slice* s, int role, float peakNorm) {
    float lc = log2f(fmaxf(s->centroid, 30.0f) / 100.0f);  // 100 Hz → 0, 6.4 kHz → 6
    float fl = s->flatness, lo = s->lowRatio, hi = s->highRatio, dec = s->decayMs;
    float sc;
    switch (role) {
        case FS_ROLE_KICK:       sc = gauss(lc, 1.3f, 1.3f) * (0.4f + 1.2f * lo) * gauss(dec, 220, 260); break;
        case FS_ROLE_SNARE:      sc = gauss(lc, 4.8f, 1.3f) * (0.4f + fl) * gauss(dec, 160, 180); break;
        case FS_ROLE_CLAP:       sc = gauss(lc, 5.0f, 1.3f) * (0.3f + fl) * gauss(dec, 110, 140); break;
        case FS_ROLE_HAT_CLOSED: sc = gauss(lc, 6.8f, 1.2f) * (0.3f + hi) * gauss(dec, 40, 90); break;
        case FS_ROLE_HAT_OPEN:   sc = gauss(lc, 6.6f, 1.2f) * (0.3f + hi) * gauss(dec, 320, 300); break;
        case FS_ROLE_TOM_LOW:    sc = gauss(lc, 2.2f, 1.0f) * (1.1f - fl) * gauss(dec, 260, 260) * 0.8f; break;
        case FS_ROLE_TOM_HIGH:   sc = gauss(lc, 3.2f, 1.0f) * (1.1f - fl) * gauss(dec, 200, 240) * 0.8f; break;
        default:                 sc = gauss(lc, 4.0f, 2.2f) * 0.45f; break;
    }
    return sc * (0.35f + 0.65f * peakNorm);
}

// ---------------------------------------------------------------- Sound design per role
static float role_pitch(int role, float lc) {
    float p;
    switch (role) {
        case FS_ROLE_KICK:       p = powf(2.0f, (1.3f - lc) * 0.55f); return fminf(fmaxf(p, 0.42f), 1.0f);
        case FS_ROLE_HAT_CLOSED:
        case FS_ROLE_HAT_OPEN:   p = powf(2.0f, (6.2f - lc) * 0.5f);  return fminf(fmaxf(p, 1.0f), 2.6f);
        case FS_ROLE_TOM_LOW:    p = powf(2.0f, (2.2f - lc) * 0.5f);  return fminf(fmaxf(p, 0.5f), 1.4f);
        case FS_ROLE_TOM_HIGH:   p = powf(2.0f, (3.2f - lc) * 0.5f);  return fminf(fmaxf(p, 0.6f), 1.6f);
        case FS_ROLE_PERC:       p = powf(2.0f, (4.5f - lc) * 0.35f); return fminf(fmaxf(p, 0.8f), 1.8f);
        default:                 p = powf(2.0f, (4.3f - lc) * 0.3f);  return fminf(fmaxf(p, 0.7f), 1.5f);
    }
}

static const float kRoleLengthMs[FS_NUM_PADS] = { 480, 340, 380, 110, 650, 520, 420, 300 };

float* fs_process_for_role(const float* src, uint32_t len, double sr, int32_t role, float character,
                           const fs_slice* ft, uint32_t* seed, uint32_t* outLen) {
    if (character < 0) character = 0;
    if (character > 1) character = 1;
    float lc = log2f(fmaxf(ft ? ft->centroid : 1000.0f, 30.0f) / 100.0f);
    float pitch = role_pitch(role, lc) * powf(2.0f, (fs_randf(seed) - 0.5f) * 2.0f / 12.0f); // ±1 st variation
    uint32_t target = (uint32_t)(kRoleLengthMs[role] * (0.8f + 0.4f * fs_randf(seed)) * 0.001 * sr);
    uint32_t pitchedLen = (uint32_t)(len / pitch);
    uint32_t L = pitchedLen < target ? target : pitchedLen;
    if (L > target * 1.6f) L = (uint32_t)(target * 1.6f);
    if (L < 256) L = 256;

    float* y = (float*)calloc(L, sizeof(float));
    if (!y) return NULL;

    // 1) resample (pitch) with cubic interpolation + DC removal
    {
        float dcx = 0, dcy = 0;
        for (uint32_t i = 0; i < L; i++) {
            double pos = i * (double)pitch;
            int32_t ip = (int32_t)pos;
            if (ip + 2 >= (int32_t)len) break;
            float t = (float)(pos - ip);
            float xm1 = ip > 0 ? src[ip - 1] : src[ip], x0 = src[ip], x1 = src[ip + 1], x2 = src[ip + 2];
            float v = x0 + 0.5f * t * (x1 - xm1 + t * (2 * xm1 - 5 * x0 + 4 * x1 - x2 + t * (3 * (x0 - x1) + x2 - xm1)));
            float o = v - dcx + 0.995f * dcy; dcx = v; dcy = o;
            y[i] = o;
        }
    }
    normalize(y, L, 1.0f);

    // 2) role-specific tone shaping on the source
    switch (role) {
        case FS_ROLE_KICK:
            bq_apply(y, L, BQ_LP, sr, 700 + 1600 * (1 - character), 0.8, 0);
            bq_apply(y, L, BQ_HP, sr, 30, 0.7, 0);
            break;
        case FS_ROLE_SNARE:
            bq_apply(y, L, BQ_HP, sr, 160, 0.7, 0);
            bq_apply(y, L, BQ_PEAK, sr, 220, 1.0, 3);
            bq_apply(y, L, BQ_PEAK, sr, 5000, 0.8, 4);
            break;
        case FS_ROLE_CLAP: {
            bq_apply(y, L, BQ_HP, sr, 450, 0.7, 0);
            bq_apply(y, L, BQ_LP, sr, 9500, 0.7, 0);
            // clap smear: three early copies
            float* tmp = (float*)calloc(L, sizeof(float));
            if (tmp) {
                const float dly[3] = { 0.0f, 0.011f, 0.022f }, g[3] = { 0.55f, 0.75f, 1.0f };
                for (int c = 0; c < 3; c++) {
                    uint32_t d = (uint32_t)(dly[c] * sr);
                    for (uint32_t i = d; i < L; i++) tmp[i] += y[i - d] * g[c];
                }
                memcpy(y, tmp, sizeof(float) * L); free(tmp);
            }
            break;
        }
        case FS_ROLE_HAT_CLOSED: bq_apply(y, L, BQ_HP, sr, 6500, 0.7, 0); bq_apply(y, L, BQ_HP, sr, 5000, 0.7, 0); break;
        case FS_ROLE_HAT_OPEN:   bq_apply(y, L, BQ_HP, sr, 5000, 0.7, 0); bq_apply(y, L, BQ_PEAK, sr, 9000, 0.7, 3); break;
        case FS_ROLE_TOM_LOW:
        case FS_ROLE_TOM_HIGH:   bq_apply(y, L, BQ_LP, sr, 3200, 0.7, 0); bq_apply(y, L, BQ_HP, sr, 55, 0.7, 0); break;
        default:                 bq_apply(y, L, BQ_HP, sr, 280, 0.7, 0); bq_apply(y, L, BQ_PEAK, sr, 2500, 1.2, 4); break;
    }
    normalize(y, L, 1.0f);

    // 3) synthetic reinforcement layer – this is what makes mouth sounds hit like drums
    uint32_t ns = *seed ^ 0xA5A5A5A5u;
    double phase = 0;
    biquad nf; bq_init(&nf, BQ_HP, sr, 2000, 0.7, 0);
    if (role == FS_ROLE_SNARE) { bq_init(&nf, BQ_HP, sr, 1800, 0.7, 0); }
    if (role == FS_ROLE_HAT_CLOSED || role == FS_ROLE_HAT_OPEN) { bq_init(&nf, BQ_HP, sr, 7500, 0.7, 0); }
    float lvl = character;
    for (uint32_t i = 0; i < L; i++) {
        float t = (float)(i / sr);
        float add = 0;
        switch (role) {
            case FS_ROLE_KICK: {
                float f = 44.0f + 120.0f * expf(-t / 0.032f);
                phase += 2.0 * M_PI * f / sr;
                add = sinf((float)phase) * expf(-t / 0.20f) * 0.95f * lvl;
                break;
            }
            case FS_ROLE_SNARE: {
                float nz = fs_randf(&ns) * 2 - 1;
                phase += 2.0 * M_PI * 185.0 / sr;
                add = (bq_run(&nf, nz) * expf(-t / 0.085f) * 0.55f + sinf((float)phase) * expf(-t / 0.045f) * 0.35f) * lvl;
                break;
            }
            case FS_ROLE_HAT_CLOSED:
            case FS_ROLE_HAT_OPEN: {
                float nz = fs_randf(&ns) * 2 - 1;
                float tau = role == FS_ROLE_HAT_CLOSED ? 0.028f : 0.22f;
                add = bq_run(&nf, nz) * expf(-t / tau) * 0.35f * lvl;
                break;
            }
            case FS_ROLE_TOM_LOW:
            case FS_ROLE_TOM_HIGH: {
                float base = role == FS_ROLE_TOM_LOW ? 88.0f : 145.0f;
                float f = base * (1.0f + 0.35f * expf(-t / 0.05f));
                phase += 2.0 * M_PI * f / sr;
                add = sinf((float)phase) * expf(-t / 0.18f) * 0.6f * lvl;
                break;
            }
            default: break;
        }
        y[i] += add;
    }

    // 4) transient emphasis (first 8 ms) + amplitude envelope
    uint32_t attack = (uint32_t)(0.0006 * sr) + 1;
    uint32_t boost = (uint32_t)(0.008 * sr) + 1;
    float boostAmt = (role == FS_ROLE_KICK || role == FS_ROLE_SNARE || role == FS_ROLE_CLAP) ? 0.7f : 0.4f;
    uint32_t fadeStart = (uint32_t)(L * 0.45f);
    for (uint32_t i = 0; i < L; i++) {
        float g = 1.0f;
        if (i < attack) g *= (float)i / attack;
        if (i < boost) g *= 1.0f + boostAmt * character * (1.0f - (float)i / boost);
        if (i > fadeStart) { float r = (float)(i - fadeStart) / (L - fadeStart); g *= (1 - r) * (1 - r); }
        y[i] *= g;
    }

    // 5) saturation
    float drive = 1.0f + character * ((role == FS_ROLE_KICK || role == FS_ROLE_SNARE) ? 2.5f : 1.2f);
    normalize(y, L, 1.0f);
    float norm = tanhf(drive);
    for (uint32_t i = 0; i < L; i++) y[i] = tanhf(y[i] * drive) / norm;
    normalize(y, L, 0.89f);

    *outLen = L;
    return y;
}

// ---------------------------------------------------------------- Kit building
fs_kit* fs_build_kit(const float* x, uint32_t n, double sr, float sensitivity, float character, int32_t style, uint32_t seed) {
    if (!x || n < 2048) return NULL;
    enum { MAXS = 256 };
    fs_slice* sl = (fs_slice*)malloc(sizeof(fs_slice) * MAXS);
    if (!sl) return NULL;
    int32_t ns = fs_detect_slices(x, n, sr, sensitivity, sl, MAXS);
    if (ns <= 0) { free(sl); return NULL; }

    uint32_t rng = seed ? seed : 0x12345u;
    float maxPeak = 0; for (int i = 0; i < ns; i++) if (sl[i].peak > maxPeak) maxPeak = sl[i].peak;

    float* score = (float*)malloc(sizeof(float) * ns * FS_NUM_PADS);
    if (!score) { free(sl); return NULL; }
    for (int i = 0; i < ns; i++)
        for (int r = 0; r < FS_NUM_PADS; r++) {
            float jitter = seed ? (0.7f + 0.6f * fs_randf(&rng)) : 1.0f;
            score[i * FS_NUM_PADS + r] = role_score(&sl[i], r, sl[i].peak / maxPeak) * jitter;
        }

    // Priority-ordered assignment: the most important roles pick first.
    int order[FS_NUM_PADS] = { FS_ROLE_KICK, FS_ROLE_SNARE, FS_ROLE_HAT_CLOSED, FS_ROLE_CLAP,
                               FS_ROLE_HAT_OPEN, FS_ROLE_TOM_LOW, FS_ROLE_TOM_HIGH, FS_ROLE_PERC };
    if (seed) { // vary the order of the secondary roles a little
        for (int i = FS_NUM_PADS - 1; i > 3; i--) {
            int j = 3 + (int)(fs_rand(&rng) % (uint32_t)(i - 2));
            int t = order[i]; order[i] = order[j]; order[j] = t;
        }
    }
    int assign[FS_NUM_PADS]; int used[MAXS] = {0};
    int usedCount = 0;
    for (int oi = 0; oi < FS_NUM_PADS; oi++) {
        int r = order[oi];
        int allowReuse = usedCount >= ns;
        float best = -1; int bi = 0;
        for (int i = 0; i < ns; i++) {
            if (used[i] && !allowReuse) continue;
            float sc = score[i * FS_NUM_PADS + r] * (used[i] ? 0.6f : 1.0f);
            if (sc > best) { best = sc; bi = i; }
        }
        assign[r] = bi;
        if (!used[bi]) { used[bi] = 1; usedCount++; }
#ifdef FS_DEBUG
        printf("  role %-8s <- slice %d (%.2fs) score %.3f\n", fs_role_name(r), bi, sl[bi].start / sr, best);
#endif
    }
    // toms: the darker source becomes the low tom
    if (sl[assign[FS_ROLE_TOM_LOW]].centroid > sl[assign[FS_ROLE_TOM_HIGH]].centroid) {
        int t = assign[FS_ROLE_TOM_LOW]; assign[FS_ROLE_TOM_LOW] = assign[FS_ROLE_TOM_HIGH]; assign[FS_ROLE_TOM_HIGH] = t;
    }

    fs_kit* kit = fs_kit_create(sr);
    if (!kit) { free(score); free(sl); return NULL; }
    for (int r = 0; r < FS_NUM_PADS; r++) {
        int i = assign[r];
        uint32_t outLen = 0;
        float* snd = fs_process_for_role(x + sl[i].start, sl[i].end - sl[i].start, sr, r, character, &sl[i], &rng, &outLen);
        if (snd) { fs_kit_set_pad(kit, r, snd, outLen, fs_role_name(r), r); free(snd); }
    }
    fs_generate_pattern(kit->steps, style, seed ^ 0x5bd1e995u, 0.6f);
    kit->params[FS_PARAM_TEMPO] = fs_style_tempo(style);
    free(score); free(sl);
    return kit;
}

fs_kit* fs_kit_create_demo(double sr, uint32_t seed) {
    fs_kit* kit = fs_kit_create(sr);
    if (!kit) return NULL;
    uint32_t rng = seed ? seed : 7;
    uint32_t len = (uint32_t)(0.03 * sr);
    float* click = (float*)malloc(sizeof(float) * len);
    if (!click) return kit;
    for (int r = 0; r < FS_NUM_PADS; r++) {
        uint32_t ns = rng + (uint32_t)r * 977u;
        for (uint32_t i = 0; i < len; i++) click[i] = (fs_randf(&ns) * 2 - 1) * expf(-(float)i / (0.004f * (float)sr));
        fs_slice ft; memset(&ft, 0, sizeof ft);
        static const float centroids[FS_NUM_PADS] = { 200, 1800, 2400, 8000, 7000, 420, 820, 2000 };
        ft.centroid = centroids[r];
        uint32_t outLen = 0;
        float* snd = fs_process_for_role(click, len, sr, r, 1.0f, &ft, &rng, &outLen);
        if (snd) { fs_kit_set_pad(kit, r, snd, outLen, fs_role_name(r), r); free(snd); }
    }
    free(click);
    fs_generate_pattern(kit->steps, FS_STYLE_BOOMBAP, rng, 0.6f);
    kit->params[FS_PARAM_TEMPO] = fs_style_tempo(FS_STYLE_BOOMBAP);
    return kit;
}

// ---------------------------------------------------------------- Groove generator
// probability (0–100) per role per 16th step. ≥90 = core hit.
static const uint8_t kStyles[FS_NUM_STYLES][FS_NUM_PADS][FS_NUM_STEPS] = {
    { // Boom Bap
        {100,0,0,0, 0,0,0,35, 0,0,95,0, 0,25,0,0},
        {0,0,0,0, 100,0,0,0, 0,0,0,0, 100,0,0,20},
        {0,0,0,0, 30,0,0,0, 0,0,0,0, 40,0,0,0},
        {95,25,90,25, 95,25,90,25, 95,25,90,25, 95,25,90,25},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,35,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,20},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,20,0,0},
        {0,0,0,30, 0,0,20,0, 0,30,0,0, 0,0,20,0},
    },
    { // House
        {100,0,0,0, 100,0,0,0, 100,0,0,0, 100,0,0,15},
        {0,0,0,0, 0,0,0,25, 0,0,0,0, 0,0,0,30},
        {0,0,0,0, 100,0,0,0, 0,0,0,0, 100,0,0,0},
        {0,45,0,45, 0,45,0,45, 0,45,0,45, 0,45,0,45},
        {0,0,100,0, 0,0,100,0, 0,0,100,0, 0,0,100,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,15, 0,0,0,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,15,0},
        {0,0,0,35, 0,0,0,0, 0,0,0,40, 0,25,0,0},
    },
    { // Trap (half-time feel)
        {100,0,0,0, 0,0,0,45, 0,0,75,0, 0,0,30,0},
        {0,0,0,0, 0,0,0,0, 100,0,0,0, 0,0,0,0},
        {0,0,0,0, 0,0,0,0, 100,0,0,0, 0,0,0,0},
        {95,60,95,60, 95,60,95,90, 95,60,95,60, 95,90,95,90},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,25,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,25,0,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 25,0,0,0},
        {0,0,0,0, 0,0,30,0, 0,0,0,0, 0,0,0,25},
    },
    { // Broken / Garage
        {100,0,0,45, 0,0,65,0, 0,55,0,0, 0,0,40,0},
        {0,0,0,0, 100,0,0,35, 0,0,0,0, 100,0,30,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 35,0,0,0},
        {75,0,75,40, 75,0,75,40, 75,0,75,40, 75,0,75,40},
        {0,0,0,0, 0,0,0,0, 0,0,40,0, 0,0,0,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,25, 0,0,0,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,25,0,25},
        {0,30,0,0, 0,0,0,30, 0,0,30,0, 0,0,0,30},
    },
};
static const float kStyleTempo[FS_NUM_STYLES] = { 90, 124, 140, 132 };

float fs_style_tempo(int32_t style) {
    if (style < 0 || style >= FS_NUM_STYLES) style = 0;
    return kStyleTempo[style];
}

void fs_generate_pattern(uint8_t steps[FS_NUM_PADS][FS_NUM_STEPS], int32_t style, uint32_t seed, float density) {
    if (style < 0 || style >= FS_NUM_STYLES) style = 0;
    if (density < 0) density = 0;
    if (density > 1) density = 1;
    uint32_t rng = seed ? seed : 99;
    for (int p = 0; p < FS_NUM_PADS; p++) {
        for (int s = 0; s < FS_NUM_STEPS; s++) {
            int prob = kStyles[style][p][s];
            uint8_t v = 0;
            if (prob >= 90) {
                if (fs_randf(&rng) * 100.0f < prob) v = (uint8_t)(100 + fs_rand(&rng) % 28);
            } else if (prob > 0) {
                float pr = prob * (0.4f + 1.2f * density);
                if (fs_randf(&rng) * 100.0f < pr) v = (uint8_t)(55 + fs_rand(&rng) % 40);
            }
            // hats: accent on the beat
            if (v && (p == FS_ROLE_HAT_CLOSED) && (s % 4 != 0)) v = (uint8_t)(v * 0.7f);
            steps[p][s] = v > 127 ? 127 : v;
        }
    }
    // open hat chokes closed hat on the same step
    for (int s = 0; s < FS_NUM_STEPS; s++) if (steps[FS_ROLE_HAT_OPEN][s]) steps[FS_ROLE_HAT_CLOSED][s] = 0;
}
