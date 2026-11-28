// Fundstück DSP core – plain C so it runs in the app, in the AUv3 render thread,
// and can be unit-tested on any machine.
#ifndef FS_DSP_H
#define FS_DSP_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FS_NUM_PADS   8
#define FS_NUM_STEPS  16
#define FS_NAME_LEN   32
#define FS_NUM_PARAMS 48
#define FS_NUM_STYLES 4

// Roles == pad index in an auto-built kit
#define FS_ROLE_KICK       0
#define FS_ROLE_SNARE      1
#define FS_ROLE_CLAP       2
#define FS_ROLE_HAT_CLOSED 3
#define FS_ROLE_HAT_OPEN   4
#define FS_ROLE_TOM_LOW    5
#define FS_ROLE_TOM_HIGH   6
#define FS_ROLE_PERC       7

// Global parameters
#define FS_PARAM_MASTER      0   // 0 … 1.5
#define FS_PARAM_DRIVE       1   // 0 … 1
#define FS_PARAM_SWING       2   // 0 … 0.5
#define FS_PARAM_TEMPO       3   // 60 … 200 BPM (internal clock)
#define FS_PARAM_PLAY        4   // 0/1  pattern on
#define FS_PARAM_FOLLOW_HOST 5   // 0/1  use host tempo/transport
#define FS_PARAM_PAD_BASE    16  // + pad*4 + {0 gain, 1 tune, 2 decay, 3 pan}
#define FS_PAD_GAIN  0           // 0 … 1.5
#define FS_PAD_TUNE  1           // -24 … +24 semitones
#define FS_PAD_DECAY 2           // 0.05 … 1 (fraction of sample length)
#define FS_PAD_PAN   3           // -1 … 1

// Styles for the groove generator
#define FS_STYLE_BOOMBAP 0
#define FS_STYLE_HOUSE   1
#define FS_STYLE_TRAP    2
#define FS_STYLE_BROKEN  3

typedef struct fs_kit fs_kit;
typedef struct fs_engine fs_engine;

typedef struct {
    int32_t hostValid;     // musical context available
    int32_t hostPlaying;   // host transport moving
    double  tempo;         // host BPM
    double  beatPosition;  // host beat at first sample of buffer
} fs_transport;

typedef struct {
    uint32_t start, end;   // sample frames
    float peak, rms;
    float centroid;        // Hz
    float lowRatio;        // energy < 180 Hz
    float highRatio;       // energy > 6 kHz
    float flatness;        // 0 tonal … 1 noisy
    float zcr;             // zero crossings per sample
    float decayMs;         // peak → -20 dB
} fs_slice;

// ---------- Kit ----------
fs_kit*      fs_kit_create(double sampleRate);
void         fs_kit_destroy(fs_kit* kit);
fs_kit*      fs_kit_copy(const fs_kit* kit);
void         fs_kit_set_pad(fs_kit* kit, int32_t pad, const float* data, uint32_t length,
                            const char* name, int32_t role);
uint32_t     fs_kit_pad_length(const fs_kit* kit, int32_t pad);
const float* fs_kit_pad_data(const fs_kit* kit, int32_t pad);
const char*  fs_kit_pad_name(const fs_kit* kit, int32_t pad);
double       fs_kit_sample_rate(const fs_kit* kit);
void         fs_kit_set_param(fs_kit* kit, int32_t id, float value);
float        fs_kit_get_param(const fs_kit* kit, int32_t id);
void         fs_kit_set_step(fs_kit* kit, int32_t pad, int32_t step, uint8_t velocity);
uint8_t      fs_kit_get_step(const fs_kit* kit, int32_t pad, int32_t step);

size_t       fs_kit_serialized_size(const fs_kit* kit);
size_t       fs_kit_serialize(const fs_kit* kit, uint8_t* out, size_t capacity);
fs_kit*      fs_kit_deserialize(const uint8_t* in, size_t length);

/// Synthesized fallback kit so the plugin is never silent.
fs_kit*      fs_kit_create_demo(double sampleRate, uint32_t seed);

/// Write one pad as 24-bit mono WAV. Returns 1 on success.
int32_t      fs_kit_write_pad_wav(const fs_kit* kit, int32_t pad, const char* path);
int32_t      fs_write_wav(const char* path, const float* data, uint32_t length, double sampleRate);

// ---------- Analysis ----------
/// Find percussive hits. sensitivity 0 (only loud hits) … 1 (everything).
int32_t fs_detect_slices(const float* x, uint32_t n, double sampleRate, float sensitivity,
                         fs_slice* out, int32_t maxOut);

/// Recording → 8-pad drum kit + groove.
/// character 0 (raw/natural) … 1 (heavily layered & punchy). seed varies assignment.
fs_kit* fs_build_kit(const float* x, uint32_t n, double sampleRate, float sensitivity,
                     float character, int32_t style, uint32_t seed);

void    fs_generate_pattern(uint8_t steps[FS_NUM_PADS][FS_NUM_STEPS], int32_t style,
                            uint32_t seed, float density);
float   fs_style_tempo(int32_t style);
const char* fs_role_name(int32_t role);

// ---------- Engine (real-time) ----------
fs_engine* fs_engine_create(double sampleRate);
void       fs_engine_destroy(fs_engine* e);
void       fs_engine_set_sample_rate(fs_engine* e, double sampleRate); // not while rendering

/// Takes ownership of kit. keepState=1 keeps current params/pattern.
void       fs_engine_load_kit(fs_engine* e, fs_kit* kit, int32_t keepState);
/// New kit with current samples + live params/pattern (caller destroys).
fs_kit*    fs_engine_snapshot(fs_engine* e);

void    fs_engine_set_param(fs_engine* e, int32_t id, float value);
float   fs_engine_get_param(const fs_engine* e, int32_t id);
void    fs_engine_set_step(fs_engine* e, int32_t pad, int32_t step, uint8_t velocity);
uint8_t fs_engine_get_step(const fs_engine* e, int32_t pad, int32_t step);
void    fs_engine_clear_pattern(fs_engine* e);
void    fs_engine_generate_pattern(fs_engine* e, int32_t style, uint32_t seed, float density);

/// UI thread: trigger pad (lock-free queue).
void    fs_engine_trigger(fs_engine* e, int32_t pad, uint8_t velocity);
/// Render thread, before fs_engine_render: queue a MIDI 1.0 message at frame offset.
void    fs_engine_queue_midi(fs_engine* e, uint8_t status, uint8_t data1, uint8_t data2,
                             uint32_t offset);
int32_t fs_engine_note_for_pad(int32_t pad);

void    fs_engine_render(fs_engine* e, float* left, float* right, uint32_t frames,
                         const fs_transport* transport);

// UI helpers
int32_t  fs_engine_current_step(const fs_engine* e);           // -1 when stopped
uint32_t fs_engine_trigger_count(const fs_engine* e, int32_t pad);
void     fs_engine_pad_name(fs_engine* e, int32_t pad, char* out, int32_t capacity);
void     fs_engine_pad_waveform(fs_engine* e, int32_t pad, float* out, int32_t bins);
float    fs_engine_pad_seconds(fs_engine* e, int32_t pad);

#ifdef __cplusplus
}
#endif
#endif
