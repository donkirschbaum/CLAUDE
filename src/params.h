// Parameter definitions shared by the DSP and the UI.
#pragma once

#include <cstdint>

namespace anu {

enum ParamKind : uint8_t {
  KIND_KNOB,     // 0..1
  KIND_BIPOLAR,  // -1..1 (crossfade knobs with centre detent)
  KIND_INT,      // integer range
  KIND_ENUM,     // integer with labels
  KIND_BOOL,
  KIND_OUTPUT    // DSP -> UI
};

enum ParamId : uint32_t {
  // --- analog section (upper panel)
  P_FM,
  P_PW,
  P_SYNC,
  P_MIX,
  P_SUB_LEVEL,
  P_SUB_SOURCE,
  P_CUTOFF,
  P_RESONANCE,
  P_VCF_MODE,
  P_FUZZ,
  P_VOLUME,
  P_DRUM_LEVEL,
  // --- hardwired MCU pots
  P_VCO_MOD,
  P_PW_MOD,
  P_VCF_ENV,
  P_VCF_LFO,
  // --- page 1: synth (ADSR / LFO)
  P_GLIDE,
  P_VCO_RANGE,
  P_VCO_DETUNE,
  P_LFO_SHAPE,
  P_LFO_RATE,
  P_ATTACK,
  P_DECAY,
  P_SUSTAIN,
  P_RELEASE,
  P_VCA_ENV,
  // --- page 2: clock / keyboard
  P_TEMPO,
  P_SWING,
  P_ARP_MODE,
  P_ARP_PATTERN,
  P_ACIDITY,
  P_DCO_RANGE,
  P_DCO_FINE,
  P_VIBRATO_RATE,
  P_VIBRATO_DEST,
  P_VELOCITY_MOD,
  // --- page 3: drums
  P_DRUMS_X,
  P_BD_DENSITY,
  P_SD_DENSITY,
  P_HH_DENSITY,
  P_DRUMS_BALANCE,
  P_DRUMS_Y,
  P_BD_TONE,
  P_SD_TONE,
  P_HH_TONE,
  P_DRUMS_BANDWIDTH,
  // --- CC-only patch parameters of the original
  P_CUTOFF_BIAS,
  P_CUTOFF_TRACKING,
  P_LEGATO,
  // --- transport / sequencer
  P_RUN,
  P_RECORD,
  P_HOLD,
  P_DRUM_OVERRIDE,
  // --- MIDI settings
  P_MIDI_IN_CHANNEL,
  P_MIDI_OUT_CHANNEL,
  P_MIDI_OUT_SEQ,
  P_MIDI_OUT_DRUMS,
  P_DRUM_OUT_CHANNEL,
  P_BD_NOTE,
  P_SD_NOTE,
  P_HH_NOTE,
  P_DRUM_GATE_MS,
  P_MIDI_THRU,
  // --- outputs (DSP -> UI)
  P_OUT_DRUM_STEP,
  P_OUT_LEDS,
  P_OUT_PATTERN_BD,
  P_OUT_PATTERN_SD,
  P_OUT_PATTERN_HH,
  P_OUT_SEQ_LENGTH,
  P_COUNT
};

struct ParamInfo {
  const char* symbol;
  const char* name;
  const char* label;  // short label for the panel
  ParamKind kind;
  float min, max, def;
  int16_t cc;    // original / assigned MIDI CC (-1 = none)
  int16_t cc2;   // second CC (crossfade: LFO side)
  const char* const* enums;
};

static const char* const kSubSourceNames[] = { "DCO", "-1 Oct", "-2 Oct" };
static const char* const kVcfModeNames[] = { "LP", "BP", "HP" };
static const char* const kLfoShapeNames[] = {
  "Triangle", "Square", "Ramp Up", "Ramp Down", "S&H", "Bernoulli", "Smooth Rnd", "Noise"
};
static const char* const kArpModeNames[] = {
  "Off", "Up 1", "Up 2", "Down 1", "Down 2", "Up/Dn 1", "Up/Dn 2", "Random 1", "Random 2"
};
static const char* const kArpPatternNames[] = {
  "Pattern 1", "Pattern 2", "Pattern 3", "Pattern 4", "Pattern 5", "Pattern 6"
};
static const char* const kOnOff[] = { "Off", "On" };

static const ParamInfo kParams[P_COUNT] = {
  { "fm",            "FM Amount",          "FM",        KIND_KNOB,    0, 1, 0.0f,    85, -1, nullptr },
  { "pw",            "Pulse Width",        "PW",        KIND_KNOB,    0, 1, 0.5f,    86, -1, nullptr },
  { "sync",          "Sync",               "SYNC",      KIND_BOOL,    0, 1, 0,       90, -1, kOnOff },
  { "mix",           "Saw/Square Mix",     "MIX",       KIND_KNOB,    0, 1, 0.5f,    87, -1, nullptr },
  { "sub_level",     "Sub Level",          "SUB",       KIND_KNOB,    0, 1, 0.0f,    88, -1, nullptr },
  { "sub_source",    "Sub Source",         "SUB SRC",   KIND_ENUM,    0, 2, 1,       89, -1, kSubSourceNames },
  { "cutoff",        "Cutoff",             "CUTOFF",    KIND_KNOB,    0, 1, 0.6f,    80, -1, nullptr },
  { "resonance",     "Resonance",          "RES",       KIND_KNOB,    0, 1, 0.2f,    71, -1, nullptr },
  { "vcf_mode",      "VCF Mode",           "MODE",      KIND_ENUM,    0, 2, 0,       82, -1, kVcfModeNames },
  { "fuzz",          "Fuzz",               "FUZZ",      KIND_BOOL,    0, 1, 0,       83, -1, kOnOff },
  { "volume",        "Volume",             "VOLUME",    KIND_KNOB,    0, 1, 0.945f,   7, -1, nullptr },
  { "drum_level",    "Drum Level",         "DRUMS",     KIND_KNOB,    0, 1, 0.8f,    84, -1, nullptr },

  { "vco_mod",       "VCO Mod (Env<>LFO)", "VCO MOD",   KIND_BIPOLAR, -1, 1, 0.0f,   16, 17, nullptr },
  { "pw_mod",        "PW Mod (Env<>LFO)",  "PW MOD",    KIND_BIPOLAR, -1, 1, 0.5f,   18, 19, nullptr },
  { "vcf_env",       "VCF Env Mod",        "VCF ENV",   KIND_KNOB,    0, 1, 0.0f,    20, -1, nullptr },
  { "vcf_lfo",       "VCF LFO Mod",        "VCF LFO",   KIND_KNOB,    0, 1, 0.0f,    21, -1, nullptr },

  { "glide",         "Glide",              "GLIDE",     KIND_KNOB,    0, 1, 0.0f,     5, -1, nullptr },
  { "vco_range",     "VCO Range",          "RANGE",     KIND_INT,   -12, 36, 0,      50, -1, nullptr },
  { "vco_detune",    "VCO Detune",         "DETUNE",    KIND_INT,   -32, 32, 0,      51, -1, nullptr },
  { "lfo_shape",     "LFO Shape",          "LFO SHAPE", KIND_ENUM,    0, 7, 0,       54, -1, kLfoShapeNames },
  { "lfo_rate",      "LFO Rate",           "LFO RATE",  KIND_KNOB,    0, 1, 96.f/255, 55, 76, nullptr },
  { "attack",        "Attack",             "ATTACK",    KIND_KNOB,    0, 1, 0.0f,    24, 73, nullptr },
  { "decay",         "Decay",              "DECAY",     KIND_KNOB,    0, 1, 64.f/255, 25, 75, nullptr },
  { "sustain",       "Sustain",            "SUSTAIN",   KIND_KNOB,    0, 1, 128.f/255, 26, -1, nullptr },
  { "release",       "Release",            "RELEASE",   KIND_KNOB,    0, 1, 128.f/255, 27, 72, nullptr },
  { "vca_env",       "VCA Env Shape",      "VCA ENV",   KIND_KNOB,    0, 1, 128.f/255, 28, -1, nullptr },

  { "tempo",         "Tempo (39 = Host)",  "TEMPO",     KIND_INT,    39, 240, 120,   -1, -1, nullptr },
  { "swing",         "Swing",              "SWING",     KIND_KNOB,    0, 1, 0.0f,   112, -1, nullptr },
  { "arp_mode",      "Arp Mode",           "ARP",       KIND_ENUM,    0, 8, 0,      113, -1, kArpModeNames },
  { "arp_pattern",   "Arp Pattern",        "PATTERN",   KIND_ENUM,    0, 5, 0,      114, -1, kArpPatternNames },
  { "acidity",       "Acidity",            "ACIDITY",   KIND_INT,     0, 15, 0,     115, -1, nullptr },
  { "dco_range",     "DCO Range",          "DCO OCT",   KIND_INT,    -2, 2, 0,       48, -1, nullptr },
  { "dco_fine",      "DCO Fine",           "DCO FINE",  KIND_INT,  -127, 127, 0,     49, -1, nullptr },
  { "vibrato_rate",  "Vibrato Rate",       "VIB RATE",  KIND_KNOB,    0, 1, 160.f/255, 52, -1, nullptr },
  { "vibrato_dest",  "Mod Wheel Dest",     "WHEEL",     KIND_KNOB,    0, 1, 0.0f,    53, -1, nullptr },
  { "velocity_mod",  "Velocity (VCF<>VCA)", "VELO",     KIND_BIPOLAR, -1, 1, 0.0f,   29, 30, nullptr },

  { "drums_x",       "Drums Map X",        "X",         KIND_KNOB,    0, 1, 0.0f,   102, -1, nullptr },
  { "bd_density",    "BD Density",         "BD",        KIND_KNOB,    0, 1, 0.0f,   104, -1, nullptr },
  { "sd_density",    "SD Density",         "SD",        KIND_KNOB,    0, 1, 0.0f,   105, -1, nullptr },
  { "hh_density",    "HH Density",         "HH",        KIND_KNOB,    0, 1, 0.0f,   106, -1, nullptr },
  { "drums_balance", "Drums Balance",      "BALANCE",   KIND_KNOB,    0, 1, 128.f/255, 110, -1, nullptr },
  { "drums_y",       "Drums Map Y",        "Y",         KIND_KNOB,    0, 1, 0.0f,   103, -1, nullptr },
  { "bd_tone",       "BD Tone",            "BD TONE",   KIND_KNOB,    0, 1, 0.0f,   107, -1, nullptr },
  { "sd_tone",       "SD Tone",            "SD TONE",   KIND_KNOB,    0, 1, 0.0f,   108, -1, nullptr },
  { "hh_tone",       "HH Tone",            "HH TONE",   KIND_KNOB,    0, 1, 0.0f,   109, -1, nullptr },
  { "drums_bw",      "Drums Bandwidth",    "BANDW",     KIND_KNOB,    0, 1, 1.0f,   111, -1, nullptr },

  { "cutoff_bias",   "Cutoff Bias",        "BIAS",      KIND_BIPOLAR, -1, 1, 0.0f,   22, 74, nullptr },
  { "cutoff_track",  "Cutoff Kbd Tracking","TRACK",     KIND_KNOB,    0, 1, 128.f/255, 23, -1, nullptr },
  { "legato",        "Legato Mode",        "LEGATO",    KIND_BOOL,    0, 1, 0,       31, -1, kOnOff },

  { "run",           "Run / Stop",         "RUN",       KIND_BOOL,    0, 1, 0,       -1, -1, kOnOff },
  { "record",        "Record",             "REC",       KIND_BOOL,    0, 1, 0,       -1, -1, kOnOff },
  { "hold",          "Hold",               "HOLD",      KIND_BOOL,    0, 1, 0,       -1, -1, kOnOff },
  { "drum_override", "Drum Pattern Override", "OVERRIDE", KIND_BOOL,  0, 1, 0,       -1, -1, kOnOff },

  { "midi_in_ch",    "MIDI In Channel (0 = Omni)", "IN CH", KIND_INT, 0, 16, 0,      -1, -1, nullptr },
  { "midi_out_ch",   "MIDI Out Channel (Arp/Seq)", "OUT CH", KIND_INT, 1, 16, 1,     -1, -1, nullptr },
  { "midi_out_seq",  "MIDI Out Arp/Seq",   "SEQ OUT",   KIND_BOOL,    0, 1, 1,       -1, -1, kOnOff },
  { "midi_out_drums","MIDI Out Drums",     "DRUM OUT",  KIND_BOOL,    0, 1, 1,       -1, -1, kOnOff },
  { "drum_out_ch",   "Drum MIDI Channel",  "DRUM CH",   KIND_INT,     1, 16, 10,     -1, -1, nullptr },
  { "bd_note",       "BD MIDI Note",       "BD NOTE",   KIND_INT,     0, 127, 36,    -1, -1, nullptr },
  { "sd_note",       "SD MIDI Note",       "SD NOTE",   KIND_INT,     0, 127, 38,    -1, -1, nullptr },
  { "hh_note",       "HH MIDI Note",       "HH NOTE",   KIND_INT,     0, 127, 42,    -1, -1, nullptr },
  { "drum_gate_ms",  "Drum Note Length (ms)", "GATE",   KIND_INT,     1, 500, 30,    -1, -1, nullptr },
  { "midi_thru",     "MIDI Thru",          "THRU",      KIND_BOOL,    0, 1, 0,       -1, -1, kOnOff },

  { "out_drum_step", "Drum Step",          "",          KIND_OUTPUT,  -1, 15, -1,    -1, -1, nullptr },
  { "out_leds",      "LED State",          "",          KIND_OUTPUT,  0, 65535, 0,   -1, -1, nullptr },
  { "out_pat_bd",    "BD Pattern",         "",          KIND_OUTPUT,  0, 65535, 0,   -1, -1, nullptr },
  { "out_pat_sd",    "SD Pattern",         "",          KIND_OUTPUT,  0, 65535, 0,   -1, -1, nullptr },
  { "out_pat_hh",    "HH Pattern",         "",          KIND_OUTPUT,  0, 65535, 0,   -1, -1, nullptr },
  { "out_seq_len",   "Sequence Length",    "",          KIND_OUTPUT,  0, 128, 4,     -1, -1, nullptr },
};

// LED bits in P_OUT_LEDS
enum LedBits {
  LED_GATE = 1 << 0,
  LED_CLOCK = 1 << 1,
  LED_LFO = 1 << 2,
  LED_RUNNING = 1 << 3,
  LED_RECORDING = 1 << 4,
  LED_HOLD = 1 << 5,
  LED_BD = 1 << 6,
  LED_SD = 1 << 7,
  LED_HH = 1 << 8,
  LED_OVERRIDE = 1 << 9,
};

}  // namespace anu
