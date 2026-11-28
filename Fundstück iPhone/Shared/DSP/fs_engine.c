// Real-time playback engine. The render path never allocates, never locks a mutex.
#include "fs_internal.h"
#include <stdatomic.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define FS_MAX_VOICES 32
#define FS_UI_RING 64
#define FS_MAX_EVENTS 256

typedef struct { uint32_t offset; int32_t pad; uint8_t velocity; } fs_event;

typedef struct {
    int      active;
    int      pad;
    double   pos, rate;
    uint32_t end, fadeStart;
    float    gl, gr;
    float    rel, relStep;   // choke release
    uint32_t age;
} fs_voice;

struct fs_engine {
    double sampleRate;
    fs_kit* kit;                     // written by main thread under kitMutex + renderLock
    pthread_mutex_t kitMutex;        // serializes non-RT kit access
    atomic_flag renderLock;          // render try-locks; main spins briefly when swapping

    _Atomic float   params[FS_NUM_PARAMS];
    _Atomic uint8_t steps[FS_NUM_PADS][FS_NUM_STEPS];
    _Atomic int32_t currentStep;
    _Atomic uint32_t triggerCount[FS_NUM_PADS];

    fs_event uiRing[FS_UI_RING];
    _Atomic uint32_t uiHead, uiTail;

    fs_event rt[FS_MAX_EVENTS];
    int rtCount;

    fs_voice voices[FS_MAX_VOICES];
    uint32_t ageCounter;

    double internalBeat;
    int    wasRunning;
    int64_t lastStepK;
};

static const int32_t kPadNotes[FS_NUM_PADS] = { 36, 38, 39, 42, 46, 45, 48, 37 };

int32_t fs_engine_note_for_pad(int32_t pad) { return (pad >= 0 && pad < FS_NUM_PADS) ? kPadNotes[pad] : -1; }

static int pad_for_note(int note) {
    switch (note) {
        case 36: case 35: return 0;
        case 38: case 40: return 1;
        case 39: return 2;
        case 42: case 44: return 3;
        case 46: case 49: return 4;
        case 45: case 41: case 43: return 5;
        case 48: case 47: case 50: return 6;
        case 37: case 51: case 56: return 7;
        default: break;
    }
    // C2…G2 chromatic as an alternative layout (60–67 = C3 octave also works)
    if (note >= 60 && note < 60 + FS_NUM_PADS) return note - 60;
    return -1;
}

fs_engine* fs_engine_create(double sr) {
    fs_engine* e = (fs_engine*)calloc(1, sizeof(fs_engine));
    if (!e) return NULL;
    e->sampleRate = sr > 1000 ? sr : 44100;
    pthread_mutex_init(&e->kitMutex, NULL);
    atomic_flag_clear(&e->renderLock);
    float p[FS_NUM_PARAMS]; fs_default_params(p);
    for (int i = 0; i < FS_NUM_PARAMS; i++) atomic_store(&e->params[i], p[i]);
    atomic_store(&e->currentStep, -1);
    e->lastStepK = INT64_MIN;
    e->kit = fs_kit_create(e->sampleRate);
    return e;
}

void fs_engine_destroy(fs_engine* e) {
    if (!e) return;
    fs_kit_destroy(e->kit);
    pthread_mutex_destroy(&e->kitMutex);
    free(e);
}

void fs_engine_set_sample_rate(fs_engine* e, double sr) { if (e && sr > 1000) e->sampleRate = sr; }

static void lock_render(fs_engine* e) {
    while (atomic_flag_test_and_set_explicit(&e->renderLock, memory_order_acquire)) sched_yield();
}
static void unlock_render(fs_engine* e) { atomic_flag_clear_explicit(&e->renderLock, memory_order_release); }

void fs_engine_load_kit(fs_engine* e, fs_kit* kit, int32_t keepState) {
    if (!e || !kit) return;
    pthread_mutex_lock(&e->kitMutex);
    lock_render(e);
    fs_kit* old = e->kit;
    e->kit = kit;
    for (int i = 0; i < FS_MAX_VOICES; i++) e->voices[i].active = 0;
    if (!keepState) {
        for (int i = 0; i < FS_NUM_PARAMS; i++) {
            if (i == FS_PARAM_PLAY || i == FS_PARAM_FOLLOW_HOST) continue; // transport state stays
            atomic_store_explicit(&e->params[i], kit->params[i], memory_order_relaxed);
        }
        for (int p = 0; p < FS_NUM_PADS; p++)
            for (int s = 0; s < FS_NUM_STEPS; s++)
                atomic_store_explicit(&e->steps[p][s], kit->steps[p][s], memory_order_relaxed);
    }
    unlock_render(e);
    pthread_mutex_unlock(&e->kitMutex);
    fs_kit_destroy(old);
}

fs_kit* fs_engine_snapshot(fs_engine* e) {
    if (!e) return NULL;
    pthread_mutex_lock(&e->kitMutex);
    fs_kit* k = fs_kit_copy(e->kit);
    pthread_mutex_unlock(&e->kitMutex);
    if (!k) return NULL;
    for (int i = 0; i < FS_NUM_PARAMS; i++) k->params[i] = atomic_load_explicit(&e->params[i], memory_order_relaxed);
    for (int p = 0; p < FS_NUM_PADS; p++)
        for (int s = 0; s < FS_NUM_STEPS; s++) k->steps[p][s] = atomic_load_explicit(&e->steps[p][s], memory_order_relaxed);
    return k;
}

void fs_engine_set_param(fs_engine* e, int32_t id, float v) {
    if (e && id >= 0 && id < FS_NUM_PARAMS) atomic_store_explicit(&e->params[id], fs_clamp_param(id, v), memory_order_relaxed);
}
float fs_engine_get_param(const fs_engine* e, int32_t id) {
    if (!e || id < 0 || id >= FS_NUM_PARAMS) return 0;
    return atomic_load_explicit((_Atomic float*)&e->params[id], memory_order_relaxed);
}
void fs_engine_set_step(fs_engine* e, int32_t pad, int32_t step, uint8_t v) {
    if (e && pad >= 0 && pad < FS_NUM_PADS && step >= 0 && step < FS_NUM_STEPS)
        atomic_store_explicit(&e->steps[pad][step], v > 127 ? 127 : v, memory_order_relaxed);
}
uint8_t fs_engine_get_step(const fs_engine* e, int32_t pad, int32_t step) {
    if (!e || pad < 0 || pad >= FS_NUM_PADS || step < 0 || step >= FS_NUM_STEPS) return 0;
    return atomic_load_explicit((_Atomic uint8_t*)&e->steps[pad][step], memory_order_relaxed);
}
void fs_engine_clear_pattern(fs_engine* e) {
    for (int p = 0; p < FS_NUM_PADS; p++) for (int s = 0; s < FS_NUM_STEPS; s++) fs_engine_set_step(e, p, s, 0);
}
void fs_engine_generate_pattern(fs_engine* e, int32_t style, uint32_t seed, float density) {
    if (!e) return;
    uint8_t st[FS_NUM_PADS][FS_NUM_STEPS];
    fs_generate_pattern(st, style, seed, density);
    for (int p = 0; p < FS_NUM_PADS; p++) for (int s = 0; s < FS_NUM_STEPS; s++) fs_engine_set_step(e, p, s, st[p][s]);
    fs_engine_set_param(e, FS_PARAM_TEMPO, fs_style_tempo(style));
}

void fs_engine_trigger(fs_engine* e, int32_t pad, uint8_t vel) {
    if (!e || pad < 0 || pad >= FS_NUM_PADS) return;
    uint32_t head = atomic_load_explicit(&e->uiHead, memory_order_relaxed);
    uint32_t tail = atomic_load_explicit(&e->uiTail, memory_order_acquire);
    if (head - tail >= FS_UI_RING) return; // full
    e->uiRing[head % FS_UI_RING] = (fs_event){ 0, pad, vel };
    atomic_store_explicit(&e->uiHead, head + 1, memory_order_release);
}

void fs_engine_queue_midi(fs_engine* e, uint8_t status, uint8_t d1, uint8_t d2, uint32_t offset) {
    if (!e) return;
    if ((status & 0xF0) != 0x90 || d2 == 0) return; // one-shots: only note-on matters
    int pad = pad_for_note(d1 & 0x7F);
    if (pad < 0 || e->rtCount >= FS_MAX_EVENTS / 2) return;
    e->rt[e->rtCount++] = (fs_event){ offset, pad, (uint8_t)(d2 & 0x7F) };
}

static void note_on(fs_engine* e, const fs_kit* kit, int pad, uint8_t vel) {
    const fs_pad* p = &kit->pads[pad];
    atomic_fetch_add_explicit(&e->triggerCount[pad], 1, memory_order_relaxed);
    if (p->length < 4 || !p->data) return;
    // choke group
    if (p->choke) {
        for (int i = 0; i < FS_MAX_VOICES; i++) {
            fs_voice* v = &e->voices[i];
            if (v->active && v->pad != pad && kit->pads[v->pad].choke == p->choke && v->relStep == 0) {
                v->relStep = 1.0f / (float)(0.012 * e->sampleRate);
            }
        }
    }
    fs_voice* v = NULL;
    for (int i = 0; i < FS_MAX_VOICES; i++) if (!e->voices[i].active) { v = &e->voices[i]; break; }
    if (!v) { // steal oldest
        uint32_t oldest = UINT32_MAX;
        for (int i = 0; i < FS_MAX_VOICES; i++) if (e->voices[i].age < oldest) { oldest = e->voices[i].age; v = &e->voices[i]; }
    }
    int base = FS_PARAM_PAD_BASE + pad * 4;
    float gain = atomic_load_explicit(&e->params[base + FS_PAD_GAIN], memory_order_relaxed);
    float tune = atomic_load_explicit(&e->params[base + FS_PAD_TUNE], memory_order_relaxed);
    float decay = atomic_load_explicit(&e->params[base + FS_PAD_DECAY], memory_order_relaxed);
    float pan = atomic_load_explicit(&e->params[base + FS_PAD_PAN], memory_order_relaxed);
    float amp = gain * ((float)vel / 127.0f);
    float a = (pan + 1.0f) * 0.25f * 3.14159265f;
    v->gl = amp * cosf(a) * 1.41421356f;
    v->gr = amp * sinf(a) * 1.41421356f;
    v->pad = pad;
    v->pos = 0;
    v->rate = (kit->sampleRate / e->sampleRate) * pow(2.0, tune / 12.0);
    uint32_t end = (uint32_t)(p->length * decay);
    if (end < 64) end = 64;
    if (end > p->length - 2) end = p->length - 2;
    v->end = end;
    uint32_t fade = decay < 0.999f ? (uint32_t)(end * 0.4f) : (end / 8 < 256 ? end / 8 : 256);
    v->fadeStart = end - fade;
    v->rel = 1.0f; v->relStep = 0;
    v->age = ++e->ageCounter;
    v->active = 1;
}

static void render_voices(fs_engine* e, const fs_kit* kit, float* L, float* R, uint32_t from, uint32_t to) {
    for (int vi = 0; vi < FS_MAX_VOICES; vi++) {
        fs_voice* v = &e->voices[vi];
        if (!v->active) continue;
        const float* d = kit->pads[v->pad].data;
        uint32_t fadeLen = v->end - v->fadeStart; if (fadeLen == 0) fadeLen = 1;
        for (uint32_t i = from; i < to; i++) {
            uint32_t ip = (uint32_t)v->pos;
            if (ip >= v->end) { v->active = 0; break; }
            float fr = (float)(v->pos - ip);
            float s = d[ip] + (d[ip + 1] - d[ip]) * fr;
            if (ip > v->fadeStart) { float r = 1.0f - (float)(ip - v->fadeStart) / fadeLen; s *= r * r; }
            if (v->relStep > 0) { v->rel -= v->relStep; if (v->rel <= 0) { v->active = 0; break; } s *= v->rel; }
            L[i] += s * v->gl;
            R[i] += s * v->gr;
            v->pos += v->rate;
        }
    }
}

static void sort_events(fs_event* ev, int n) {
    for (int i = 1; i < n; i++) {
        fs_event t = ev[i]; int j = i - 1;
        while (j >= 0 && ev[j].offset > t.offset) { ev[j + 1] = ev[j]; j--; }
        ev[j + 1] = t;
    }
}

void fs_engine_render(fs_engine* e, float* L, float* R, uint32_t frames, const fs_transport* t) {
    if (!e || !L || !R) return;
    memset(L, 0, sizeof(float) * frames);
    memset(R, 0, sizeof(float) * frames);
    if (atomic_flag_test_and_set_explicit(&e->renderLock, memory_order_acquire)) { e->rtCount = 0; return; }
    const fs_kit* kit = e->kit;
    double sr = e->sampleRate;

    fs_event ev[FS_MAX_EVENTS];
    int nev = 0;
    for (int i = 0; i < e->rtCount && nev < FS_MAX_EVENTS; i++) {
        ev[nev] = e->rt[i];
        if (ev[nev].offset >= frames) ev[nev].offset = frames ? frames - 1 : 0;
        nev++;
    }
    e->rtCount = 0;
    uint32_t tail = atomic_load_explicit(&e->uiTail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&e->uiHead, memory_order_acquire);
    while (tail != head && nev < FS_MAX_EVENTS) { ev[nev++] = e->uiRing[tail % FS_UI_RING]; tail++; }
    atomic_store_explicit(&e->uiTail, head, memory_order_release);

    // ---- sequencer
    int play = atomic_load_explicit(&e->params[FS_PARAM_PLAY], memory_order_relaxed) > 0.5f;
    int follow = atomic_load_explicit(&e->params[FS_PARAM_FOLLOW_HOST], memory_order_relaxed) > 0.5f;
    int useHost = follow && t && t->hostValid;
    int running = play && (useHost ? t->hostPlaying : 1);
    double tempo = useHost && t->tempo > 1 ? t->tempo : atomic_load_explicit(&e->params[FS_PARAM_TEMPO], memory_order_relaxed);
    double spb = sr * 60.0 / tempo;
    if (running) {
        if (!e->wasRunning) { e->internalBeat = 0; e->lastStepK = INT64_MIN; }
        double b0 = useHost ? t->beatPosition : e->internalBeat;
        double b1 = b0 + frames / spb;
        float swing = atomic_load_explicit(&e->params[FS_PARAM_SWING], memory_order_relaxed);
        int64_t kStart = (int64_t)floor(b0 * 4.0) - 1, kEnd = (int64_t)ceil(b1 * 4.0);
        for (int64_t k = kStart; k <= kEnd; k++) {
            double pos = k * 0.25 + ((k & 1) ? swing * 0.25 : 0.0);
            if (pos < b0 || pos >= b1 || k == e->lastStepK) continue;
            e->lastStepK = k;
            uint32_t off = (uint32_t)((pos - b0) * spb);
            if (off >= frames) off = frames - 1;
            int step = (int)(((k % FS_NUM_STEPS) + FS_NUM_STEPS) % FS_NUM_STEPS);
            for (int p = 0; p < FS_NUM_PADS && nev < FS_MAX_EVENTS; p++) {
                uint8_t vel = atomic_load_explicit(&e->steps[p][step], memory_order_relaxed);
                if (vel) ev[nev++] = (fs_event){ off, p, vel };
            }
        }
        int cur = (int)((((int64_t)floor(b0 * 4.0)) % FS_NUM_STEPS + FS_NUM_STEPS) % FS_NUM_STEPS);
        atomic_store_explicit(&e->currentStep, cur, memory_order_relaxed);
        if (!useHost) e->internalBeat = b1;
    } else {
        atomic_store_explicit(&e->currentStep, -1, memory_order_relaxed);
    }
    e->wasRunning = running;

    // ---- voices, sample accurate
    sort_events(ev, nev);
    uint32_t pos = 0;
    for (int i = 0; i < nev; i++) {
        uint32_t off = ev[i].offset < frames ? ev[i].offset : frames;
        if (off > pos) { render_voices(e, kit, L, R, pos, off); pos = off; }
        note_on(e, kit, ev[i].pad, ev[i].velocity);
    }
    if (pos < frames) render_voices(e, kit, L, R, pos, frames);

    // ---- master: drive + level
    float drive = atomic_load_explicit(&e->params[FS_PARAM_DRIVE], memory_order_relaxed);
    float master = atomic_load_explicit(&e->params[FS_PARAM_MASTER], memory_order_relaxed);
    if (drive > 0.001f) {
        float k = 1.0f + drive * 5.0f, norm = master / tanhf(k);
        for (uint32_t i = 0; i < frames; i++) { L[i] = tanhf(L[i] * k) * norm; R[i] = tanhf(R[i] * k) * norm; }
    } else {
        for (uint32_t i = 0; i < frames; i++) {
            float l = L[i] * master, r = R[i] * master;
            L[i] = l > 1.5f ? 1.5f : (l < -1.5f ? -1.5f : l);
            R[i] = r > 1.5f ? 1.5f : (r < -1.5f ? -1.5f : r);
        }
    }
    unlock_render(e);
}

int32_t fs_engine_current_step(const fs_engine* e) { return e ? atomic_load_explicit((_Atomic int32_t*)&e->currentStep, memory_order_relaxed) : -1; }
uint32_t fs_engine_trigger_count(const fs_engine* e, int32_t pad) {
    if (!e || pad < 0 || pad >= FS_NUM_PADS) return 0;
    return atomic_load_explicit((_Atomic uint32_t*)&e->triggerCount[pad], memory_order_relaxed);
}

void fs_engine_pad_name(fs_engine* e, int32_t pad, char* out, int32_t cap) {
    if (!out || cap <= 0) return;
    out[0] = 0;
    if (!e || pad < 0 || pad >= FS_NUM_PADS) return;
    pthread_mutex_lock(&e->kitMutex);
    snprintf(out, (size_t)cap, "%s", e->kit->pads[pad].name);
    pthread_mutex_unlock(&e->kitMutex);
}

void fs_engine_pad_waveform(fs_engine* e, int32_t pad, float* out, int32_t bins) {
    if (!e || !out || bins <= 0) return;
    memset(out, 0, sizeof(float) * bins);
    if (pad < 0 || pad >= FS_NUM_PADS) return;
    pthread_mutex_lock(&e->kitMutex);
    const fs_pad* p = &e->kit->pads[pad];
    if (p->data && p->length) {
        for (int b = 0; b < bins; b++) {
            uint32_t s = (uint32_t)((uint64_t)p->length * b / bins), en = (uint32_t)((uint64_t)p->length * (b + 1) / bins);
            float m = 0; for (uint32_t i = s; i < en; i++) { float a = fabsf(p->data[i]); if (a > m) m = a; }
            out[b] = m;
        }
    }
    pthread_mutex_unlock(&e->kitMutex);
}

float fs_engine_pad_seconds(fs_engine* e, int32_t pad) {
    if (!e || pad < 0 || pad >= FS_NUM_PADS) return 0;
    pthread_mutex_lock(&e->kitMutex);
    float s = (float)(e->kit->pads[pad].length / e->kit->sampleRate);
    pthread_mutex_unlock(&e->kitMutex);
    return s;
}
