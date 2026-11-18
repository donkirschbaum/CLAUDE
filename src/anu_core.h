// Digital core of Anushri: port of the original AVR firmware
// (envelopes, LFOs, voice modulation matrix, arpeggiator, sequencer,
// generative drum machine, drum synthesizer, swing clock).
//
// Original code (c) 2012 Emilie Gillet (Mutable Instruments), GPLv3.
// Port to a plugin context: one instance per plugin, no globals, no AVR I/O.
#pragma once

#include <cstdint>
#include <cstring>

#include "avr_ops.h"
#include "resources.h"

namespace anu {

// Rates of the original hardware (20 MHz ATmega644p).
static constexpr double kAudioRate = 20000000.0 / 510.0;          // 39215.686 Hz
static constexpr int kControlDivider = 16;                          // 2450.98 Hz
static constexpr double kControlRate = kAudioRate / kControlDivider;
static constexpr int kDrumBlockSize = 32;                           // 1225.49 Hz

// ---------------------------------------------------------------------------
// Envelope (anu/envelope.h)
enum EnvelopeSegment {
  ENV_SEGMENT_ATTACK = 0,
  ENV_SEGMENT_DECAY = 1,
  ENV_SEGMENT_SUSTAIN = 2,
  ENV_SEGMENT_RELEASE = 3,
  ENV_SEGMENT_DEAD = 4,
  ENV_NUM_SEGMENTS,
};

class Envelope {
 public:
  void Init() {
    std::memset(this, 0, sizeof(*this));
    target_[ENV_SEGMENT_ATTACK] = 65535;
    target_[ENV_SEGMENT_RELEASE] = 0;
    target_[ENV_SEGMENT_DEAD] = 0;
    increment_[ENV_SEGMENT_SUSTAIN] = 0;
    increment_[ENV_SEGMENT_DEAD] = 0;
    segment_ = ENV_SEGMENT_DEAD;
  }
  EnvelopeSegment segment() const { return static_cast<EnvelopeSegment>(segment_); }
  uint8_t gate() const { return segment_ < ENV_SEGMENT_RELEASE ? 255 : 0; }
  void Update(uint8_t a, uint8_t d, uint8_t s, uint8_t r) {
    increment_[ENV_SEGMENT_ATTACK] = lut_res_env_increments[a];
    increment_[ENV_SEGMENT_DECAY] = lut_res_env_increments[d];
    increment_[ENV_SEGMENT_RELEASE] = lut_res_env_increments[r];
    target_[ENV_SEGMENT_SUSTAIN] = target_[ENV_SEGMENT_DECAY] = static_cast<uint16_t>(s << 8);
  }
  void Trigger(EnvelopeSegment segment) {
    if (segment == ENV_SEGMENT_DEAD || segment == ENV_SEGMENT_ATTACK) {
      value_ = 0;
    }
    a_ = value_;
    b_ = target_[segment];
    segment_ = segment;
    phase_ = 0;
  }
  uint16_t Render() {
    uint32_t increment = increment_[segment_];
    phase_ += increment;
    if (phase_ < increment) {
      value_ = Mix(a_, b_, static_cast<uint16_t>(65535));
      Trigger(static_cast<EnvelopeSegment>(segment_ + 1));
    }
    if (increment_[segment_]) {
      value_ = Mix(a_, b_, InterpolateIncreasing(lut_res_env_expo, static_cast<uint16_t>(phase_ >> 16)));
    }
    return value_;
  }
  uint16_t value() const { return value_; }

 private:
  uint32_t increment_[ENV_NUM_SEGMENTS];
  uint16_t target_[ENV_NUM_SEGMENTS];
  uint8_t segment_;
  uint16_t a_, b_, value_;
  uint32_t phase_;
};

// ---------------------------------------------------------------------------
// LFO (anu/lfo.h, anu/lfo.cc)
enum LfoShape {
  LFO_SHAPE_TRIANGLE,
  LFO_SHAPE_SQUARE,
  LFO_SHAPE_RAMP_UP,
  LFO_SHAPE_RAMP_DOWN,
  LFO_SHAPE_S_H,
  LFO_SHAPE_BERNOUILLI,
  LFO_SHAPE_LINES,
  LFO_SHAPE_NOISE,
  LFO_SHAPE_LAST
};

class Lfo {
 public:
  void Init(Random* rng) {
    std::memset(this, 0, sizeof(*this));
    rng_ = rng;
  }
  void set_target_phase(uint16_t target_phase) {
    uint16_t phi = static_cast<uint16_t>(phase_ >> 16);
    int16_t d_error = static_cast<int16_t>((65536 / 24) - static_cast<uint16_t>(phi - previous_phase_));
    int16_t p_error = static_cast<int16_t>(target_phase - phi);
    int32_t error = 0;
    error += d_error;
    error += p_error;
    error <<= 8;
    int64_t inc = static_cast<int64_t>(phase_increment_) + error;
    if (inc < 109523) inc = 109523;
    else if (inc > 7009509) inc = 7009509;
    phase_increment_ = static_cast<uint32_t>(inc);
    previous_phase_ = phi;
  }
  void set_phase_increment(uint32_t phase_increment) { phase_increment_ = phase_increment; }
  void set_shape(uint8_t shape) { shape_ = shape; }
  uint16_t Render() {
    uint16_t value = 0;
    switch (shape_) {
      case LFO_SHAPE_TRIANGLE:
        value = (phase_ & 0x80000000) ? static_cast<uint16_t>(phase_ >> 15)
                                      : static_cast<uint16_t>(~(phase_ >> 15));
        break;
      case LFO_SHAPE_SQUARE:
        value = (phase_ & 0x80000000) ? 0xffff : 0;
        break;
      case LFO_SHAPE_RAMP_UP:
        value = static_cast<uint16_t>(phase_ >> 16);
        break;
      case LFO_SHAPE_RAMP_DOWN:
        value = static_cast<uint16_t>(~(phase_ >> 16));
        break;
      case LFO_SHAPE_S_H:
        if (looped_) value_ = rng_->GetWord();
        value = value_;
        break;
      case LFO_SHAPE_BERNOUILLI:
        if (looped_) value_ = (rng_->GetWord() & 1) ? 65535 : 0;
        value = value_;
        break;
      case LFO_SHAPE_LINES:
        if (looped_) {
          value_ = next_value_;
          next_value_ = rng_->GetWord();
        }
        value = Mix(value_, next_value_, static_cast<uint16_t>(phase_ >> 16));
        break;
      case LFO_SHAPE_NOISE:
      default:
        value = rng_->GetWord();
        break;
    }
    phase_ += phase_increment_;
    looped_ = phase_ < phase_increment_;
    return value;
  }

 private:
  uint32_t phase_increment_;
  uint32_t phase_;
  uint16_t previous_phase_;
  uint16_t value_;
  uint16_t next_value_;
  uint8_t shape_;
  bool looped_;
  Random* rng_;
};

// ---------------------------------------------------------------------------
// Note stack (anu/note_stack.h)
static const uint8_t kFreeSlot = 0xff;
struct NoteEntry {
  uint8_t note;
  uint8_t velocity;
  uint8_t next_ptr;
};

template <uint8_t capacity>
class NoteStack {
 public:
  void Init() { Clear(); }
  void NoteOn(uint8_t note, uint8_t velocity) {
    NoteOff(note);
    if (size_ == capacity) {
      uint8_t least_recent_note = 0;
      for (uint8_t i = 1; i <= capacity; ++i) {
        if (pool_[i].next_ptr == 0) least_recent_note = pool_[i].note;
      }
      NoteOff(least_recent_note);
    }
    uint8_t free_slot = 0;
    for (uint8_t i = 1; i <= capacity; ++i) {
      if (pool_[i].note == kFreeSlot) {
        free_slot = i;
        break;
      }
    }
    if (!free_slot) return;
    pool_[free_slot].next_ptr = root_ptr_;
    pool_[free_slot].note = note;
    pool_[free_slot].velocity = velocity;
    root_ptr_ = free_slot;
    for (uint8_t i = 0; i < size_; ++i) {
      if (pool_[sorted_ptr_[i]].note > note) {
        for (uint8_t j = size_; j > i; --j) sorted_ptr_[j] = sorted_ptr_[j - 1];
        sorted_ptr_[i] = free_slot;
        free_slot = 0;
        break;
      }
    }
    if (free_slot) sorted_ptr_[size_] = free_slot;
    ++size_;
  }
  void NoteOff(uint8_t note) {
    uint8_t current = root_ptr_;
    uint8_t previous = 0;
    while (current) {
      if (pool_[current].note == note) break;
      previous = current;
      current = pool_[current].next_ptr;
    }
    if (current) {
      if (previous) pool_[previous].next_ptr = pool_[current].next_ptr;
      else root_ptr_ = pool_[current].next_ptr;
      for (uint8_t i = 0; i < size_; ++i) {
        if (sorted_ptr_[i] == current) {
          for (uint8_t j = i; j + 1 < size_; ++j) sorted_ptr_[j] = sorted_ptr_[j + 1];
          break;
        }
      }
      pool_[current].next_ptr = 0;
      pool_[current].note = kFreeSlot;
      pool_[current].velocity = 0;
      --size_;
    }
  }
  void Clear() {
    size_ = 0;
    std::memset(pool_, 0, sizeof(pool_));
    std::memset(sorted_ptr_, 0, sizeof(sorted_ptr_));
    root_ptr_ = 0;
    for (uint8_t i = 0; i <= capacity; ++i) pool_[i].note = kFreeSlot;
  }
  uint8_t size() const { return size_; }
  const NoteEntry& most_recent_note() const { return pool_[root_ptr_]; }
  const NoteEntry& sorted_note(uint8_t index) const { return pool_[sorted_ptr_[index]]; }

 private:
  uint8_t size_;
  NoteEntry pool_[capacity + 1];
  uint8_t root_ptr_;
  uint8_t sorted_ptr_[capacity + 1];
};

// ---------------------------------------------------------------------------
// Swing clock (anu/clock.h, anu/clock.cc). Ticks at kAudioRate.
class Clock {
 public:
  static const uint8_t kNumStepsInGroovePattern = 16;
  static const uint8_t kNumTicksPerStep = 6;
  void Reset() {
    clock_counter_ = 0;
    tick_count_ = 0;
    step_count_ = 0;
    tick_duration_ = tick_duration_table_[0];
    num_clock_events_ = 0;
  }
  void Tick() {
    ++clock_counter_;
    if (clock_counter_ >= tick_duration_) {
      ++num_clock_events_;
      clock_counter_ = 0;
    }
  }
  uint8_t CountEvents() {
    uint8_t count = 0;
    while (num_clock_events_) {
      ++tick_count_;
      if (tick_count_ == kNumTicksPerStep) {
        tick_count_ = 0;
        ++step_count_;
        if (step_count_ == kNumStepsInGroovePattern) step_count_ = 0;
        tick_duration_ = tick_duration_table_[step_count_];
      }
      ++prescaler_counter_;
      if (prescaler_counter_ >= prescaler_) {
        ++count;
        prescaler_counter_ = 0;
      }
      --num_clock_events_;
    }
    return count;
  }
  void Update(uint8_t bpm, uint8_t groove_template, uint8_t groove_amount, uint8_t prescaler) {
    if (bpm < 20) bpm = 20;
    const int32_t kTempoFactor = 392156L;
    int32_t rounding = 2 * static_cast<int32_t>(bpm);
    int32_t denominator = 4 * static_cast<int32_t>(bpm);
    int16_t base_tick_duration = static_cast<int16_t>((kTempoFactor + rounding) / denominator);
    const int16_t* groove = groove_template == 0 ? lut_res_groove_swing : lut_res_groove_shuffle;
    for (uint8_t i = 0; i < kNumStepsInGroovePattern; ++i) {
      int32_t swing_direction = groove[i];
      swing_direction *= base_tick_duration;
      swing_direction *= groove_amount;
      int16_t swing = static_cast<int16_t>(swing_direction >> 16);
      tick_duration_table_[i] = static_cast<uint16_t>(base_tick_duration + swing);
    }
    tick_duration_ = tick_duration_table_[0];
    prescaler_ = prescaler;
  }

 private:
  uint16_t clock_counter_ = 0;
  uint16_t tick_duration_table_[kNumStepsInGroovePattern] = {};
  uint16_t tick_duration_ = 1;
  uint8_t tick_count_ = 0;
  uint8_t step_count_ = 0;
  uint8_t prescaler_ = 1;
  uint8_t prescaler_counter_ = 0;
  uint8_t num_clock_events_ = 0;
};

// ---------------------------------------------------------------------------
// Drum synthesizer (anu/drum_synth.h, anu/drum_synth.cc)
static const uint8_t kNumDrumInstruments = 3;

struct DrumPatch {
  uint8_t pitch;
  uint8_t pitch_decay;
  uint8_t pitch_mod;
  uint8_t amp_decay;
  uint8_t crunchiness;
  uint8_t level;
};

struct DrumState {
  uint16_t phase;
  uint16_t phase_increment;
  uint16_t pitch_env_phase;
  uint16_t pitch_env_increment;
  uint16_t amp_env_phase;
  uint16_t amp_env_increment;
  uint8_t amp_level;
  uint8_t amp_level_noise;
  uint8_t level;
};

class DrumSynth {
 public:
  void Init(Random* rng);
  void Trigger(uint8_t instrument, uint8_t level);
  void SetParameterCc(uint8_t cc, uint8_t value);
  void MorphPatch(uint8_t instrument, uint8_t value);
  void SetBalance(uint8_t value);
  void SetBandwidth(uint8_t bandwidth);
  // Renders one sample at kAudioRate. Returns the unsigned 8-bit PWM value.
  uint8_t RenderSample();
  bool playing() const { return playing_; }
  // Amplitude envelopes (for UI LEDs).
  uint8_t amp_level(int i) const { return state_[i].amp_level; }

 private:
  void UpdateModulations();
  DrumPatch patch_[kNumDrumInstruments];
  DrumState state_[kNumDrumInstruments];
  uint8_t sample_ = 128;
  uint8_t sample_counter_ = 0;
  uint8_t sample_rate_ = 0;
  uint8_t block_counter_ = 0;
  uint8_t noise_ = 0;
  bool playing_ = false;
  Random* rng_ = nullptr;
};

// ---------------------------------------------------------------------------
// Voice (anu/voice.h, anu/voice.cc) - computes the control voltages sent to
// the analog section.
enum PatchParameter {
  PRM_PATCH_VCO_DCO_RANGE,
  PRM_PATCH_VCO_DCO_FINE,
  PRM_PATCH_VCO_DETUNE,
  PRM_PATCH_VCO_FINE,
  PRM_PATCH_VCO_ENV_AMOUNT,
  PRM_PATCH_VCO_LFO_AMOUNT,
  PRM_PATCH_PW_ENV_AMOUNT,
  PRM_PATCH_PW_LFO_AMOUNT,
  PRM_PATCH_CUTOFF_BIAS,
  PRM_PATCH_CUTOFF_TRACKING,
  PRM_PATCH_CUTOFF_ENV_AMOUNT,
  PRM_PATCH_CUTOFF_LFO_AMOUNT,
  PRM_PATCH_ENV_ATTACK,
  PRM_PATCH_ENV_DECAY,
  PRM_PATCH_ENV_SUSTAIN,
  PRM_PATCH_ENV_RELEASE,
  PRM_PATCH_ENV_VCA_MORPH,
  PRM_PATCH_ENV_LEGATO_MODE,
  PRM_PATCH_ENV_PADDING_1,
  PRM_PATCH_ENV_PADDING_2,
  PRM_PATCH_LFO_SHAPE,
  PRM_PATCH_LFO_RATE,
  PRM_PATCH_VIBRATO_RATE,
  PRM_PATCH_VIBRATO_DESTINATION,
  PRM_PATCH_KBD_GLIDE,
  PRM_PATCH_KBD_VCF_VELOCITY_AMOUNT,
  PRM_PATCH_KBD_VCA_VELOCITY_AMOUNT,
  PRM_PATCH_PADDING,
  PRM_PATCH_LAST
};

struct Patch {
  int8_t vco_dco_range;
  int8_t vco_dco_fine;
  int8_t vco_detune;
  int8_t vco_fine;
  uint8_t vco_env_amount;
  uint8_t vco_lfo_amount;
  uint8_t pw_env_amount;
  uint8_t pw_lfo_amount;
  uint8_t cutoff_bias;
  uint8_t cutoff_tracking;
  uint8_t cutoff_env_amount;
  uint8_t cutoff_lfo_amount;
  uint8_t env_attack;
  uint8_t env_decay;
  uint8_t env_sustain;
  uint8_t env_release;
  uint8_t env_vca_morph;
  uint8_t env_legato_mode;
  uint8_t env_padding[2];
  uint8_t lfo_shape;
  uint8_t lfo_rate;
  uint8_t vibrato_rate;
  uint8_t vibrato_destination;
  uint8_t kbd_glide;
  uint8_t kbd_velocity_vcf_amount;
  uint8_t kbd_velocity_vca_amount;
  uint8_t padding;
};
static_assert(sizeof(Patch) == PRM_PATCH_LAST, "patch layout");

// Control voltages, expressed in musical units instead of DAC codes.
struct ControlVoltages {
  float vco_note;   // MIDI note number (fractional) of the VCO
  float dco_note;   // MIDI note number of the DCO
  float pw;         // 0..1 pulse width modulation CV
  float vcf_offset; // semitones added to the cutoff knob
  float vca;        // 0..1 VCA level
  bool dco_muted;
};

class Voice {
 public:
  void Init(Random* rng);
  void Refresh();  // one control-rate step
  void NoteOn(uint8_t note, uint8_t velocity, uint8_t slide, uint8_t accent, bool legato);
  void NoteOff(uint8_t note);
  void ControlChange(uint8_t controller, uint8_t value);
  void PitchBend(uint16_t pitch_bend) { mod_pitch_bend_ = static_cast<int16_t>(pitch_bend); }
  void Aftertouch(uint8_t velocity) { mod_aftertouch_ = velocity; }
  void AllSoundOff();
  void ResetAllControllers();
  void GateOn();
  void GateOff();
  void SetValue(uint8_t offset, uint8_t value);
  uint8_t GetValue(uint8_t offset) const {
    return reinterpret_cast<const uint8_t*>(&patch_)[offset];
  }
  const ControlVoltages& cv() const { return cv_; }
  bool gate() const { return vca_envelope_.gate(); }
  uint8_t lfo() const { return lfo_8_bits_; }
  bool at_rest() const { return vca_envelope_.segment() == ENV_SEGMENT_DEAD; }
  void set_note(uint8_t note) { pitch_ = pitch_target_ = static_cast<int16_t>(U8U8Mul(note, 128)); }
  void set_lfo_pll_target_phase(uint8_t step) {
    if (patch_.lfo_rate < 2) lfo_.set_target_phase(static_cast<uint16_t>(step * 2730));
  }
  void set_volume(uint8_t v) { volume_ = v; }
  Patch* mutable_patch() { return &patch_; }
  void Touch() { UpdateEnvelopeParameters(); }

 private:
  void UpdateEnvelopeParameters();
  Patch patch_;
  Lfo lfo_;
  Lfo vibrato_lfo_;
  Envelope vcf_envelope_;
  Envelope mod_envelope_;
  Envelope vca_envelope_;
  uint8_t lfo_8_bits_;
  int16_t dco_pitch_;
  uint16_t pitch_counter_;
  uint16_t pitch_increment_;
  int16_t pitch_source_;
  int16_t pitch_target_;
  int16_t pitch_;
  int16_t mod_pitch_bend_;
  uint8_t mod_wheel_;
  uint8_t mod_wheel_2_;
  uint8_t mod_aftertouch_;
  uint8_t mod_velocity_;
  uint8_t mod_accent_;
  uint8_t volume_;
  ControlVoltages cv_;
};

// ---------------------------------------------------------------------------
// Sequencer settings / sequence (anu/voice_controller.h)
static const uint8_t kNumDrumParts = 3;

enum ArpeggiatorDirection {
  ARPEGGIO_DIRECTION_UP = 0,
  ARPEGGIO_DIRECTION_DOWN,
  ARPEGGIO_DIRECTION_UP_DOWN,
  ARPEGGIO_DIRECTION_RANDOM,
};

enum SequencerSettingsParameter {
  PRM_SEQ_ARP_MODE,
  PRM_SEQ_ARP_PATTERN,
  PRM_SEQ_TEMPO,
  PRM_SEQ_SWING,
  PRM_SEQ_DRUMS_X,
  PRM_SEQ_DRUMS_Y,
  PRM_SEQ_DRUMS_BD_DENSITY,
  PRM_SEQ_DRUMS_SD_DENSITY,
  PRM_SEQ_DRUMS_HH_DENSITY,
  PRM_SEQ_DRUMS_BD_TONE,
  PRM_SEQ_DRUMS_SD_TONE,
  PRM_SEQ_DRUMS_HH_TONE,
  PRM_SEQ_DRUMS_BALANCE,
  PRM_SEQ_DRUMS_BANDWIDTH,
  PRM_SEQ_DRUMS_OVERRIDE,
  PRM_SEQ_ARP_ACIDITY,
  PRM_SEQ_LAST
};

struct SequencerSettings {
  uint8_t arp_mode;
  uint8_t arp_pattern;
  uint8_t tempo;
  uint8_t swing;
  uint8_t drums_x;
  uint8_t drums_y;
  uint8_t drums_density[kNumDrumParts];
  uint8_t drums_tone[kNumDrumParts];
  uint8_t drums_balance;
  uint8_t drums_bandwidth;
  uint8_t drums_override;
  uint8_t acidity;
  uint16_t drums_pattern[kNumDrumParts];

  uint8_t arp_range() const { return static_cast<uint8_t>(((arp_mode - 1) & 0x01) + 1); }
  uint8_t arp_direction() const { return static_cast<uint8_t>((arp_mode - 1) >> 1); }
  bool has_drums() const {
    return (drums_density[0] > 1) || (drums_density[1] > 1) || (drums_density[2] > 1) ||
           drums_override;
  }
};

struct Sequence {
  uint8_t num_notes;
  uint8_t notes[128];
  uint8_t accents[16];
  uint8_t slides[16];
};

// Receives the MIDI messages generated by the instrument (MIDI out).
class MidiOutListener {
 public:
  virtual ~MidiOutListener() {}
  virtual void OnInternalNoteOn(uint8_t note, uint8_t velocity) = 0;
  virtual void OnInternalNoteOff(uint8_t note) = 0;
  virtual void OnDrumNote(uint8_t instrument, uint8_t velocity) = 0;
};

class VoiceController {
 public:
  void Init(Random* rng, MidiOutListener* out);
  void NoteOn(uint8_t note, uint8_t velocity);
  void NoteOff(uint8_t note);
  // Handles CC semantics of the voice / sustain. Parameter CCs are handled
  // by the plugin's parameter layer.
  void ControlChange(uint8_t controller, uint8_t value);
  void PitchBend(uint16_t pitch_bend);
  void Aftertouch(uint8_t velocity) { voice_.Aftertouch(velocity); }
  void AllSoundOff();
  void ResetAllControllers() { voice_.ResetAllControllers(); }
  void AllNotesOff();
  void Clock();

  void Start() {
    StartClock();
    StartArpeggiator();
    StartSequencer();
    StartDrumMachine();
  }
  void Stop() {
    StopSequencer();
    StopClock();
  }
  void InsertRest() {
    if (!sequencer_recording_) return;
    sequence_.notes[sequence_.num_notes++] = 0xff;
    if (sequence_.num_notes == 128) StopRecording();
  }
  void InsertTie() {
    if (!sequencer_recording_) return;
    sequence_.notes[sequence_.num_notes++] = 0xfe;
    if (sequence_.num_notes == 128) StopRecording();
  }
  void HoldNotes() {
    if (ignore_note_off_messages_) {
      ReleaseAllHeldNotes();
    } else if (pressed_keys_.size()) {
      ignore_note_off_messages_ = true;
    }
  }
  // Latching hold (like the HOLD button, but also latches keys pressed later).
  void SetHold(bool on) {
    if (on) ignore_note_off_messages_ = true;
    else if (ignore_note_off_messages_) ReleaseAllHeldNotes();
  }
  void StartRecording();
  void StopRecording();
  void RemoteControlDrumSequencer(uint8_t note);
  void ReleaseAllHeldNotes();

  // Internal clock: called once per kAudioRate sample.
  void TickInternalClock() { clock_.Tick(); }
  uint8_t CountClockEvents() { return clock_.CountEvents(); }
  void ResetInternalClock() { clock_.Reset(); }
  // Retunes the internal clock (used to free-run at the host tempo).
  void UpdateClockTempo(uint8_t bpm) { clock_.Update(bpm, 1, seq_settings_.swing >> 1, 1); }

  // Host sync helpers: aligns the counters to an absolute 24 ppqn position.
  void AlignToPulse(int64_t pulse);

  void SetValue(uint8_t offset, uint8_t value);
  uint8_t GetValue(uint8_t offset) const {
    return reinterpret_cast<const uint8_t*>(&seq_settings_)[offset];
  }

  Voice* mutable_voice() { return &voice_; }
  const Voice& voice() const { return voice_; }
  DrumSynth* mutable_drum_synth() { return &drum_synth_; }
  const DrumSynth& drum_synth() const { return drum_synth_; }
  Sequence* mutable_sequence() { return &sequence_; }
  const Sequence& sequence() const { return sequence_; }
  SequencerSettings* mutable_sequencer_settings() { return &seq_settings_; }
  const SequencerSettings& sequencer_settings() const { return seq_settings_; }

  bool sequencer_running() const { return sequencer_running_; }
  bool sequencer_recording() const { return sequencer_recording_; }
  bool hold_state() const { return ignore_note_off_messages_; }
  bool clock_running() const { return clock_running_; }
  uint8_t sequencer_step() const { return sequencer_note_; }
  uint8_t drum_step() const { return drum_sequencer_step_; }
  uint8_t clock_pulse() const { return clock_pulse_; }
  void ClearClockPulse() { if (clock_pulse_) --clock_pulse_; }
  uint8_t lfo_sync_counter() const { return lfo_sync_counter_; }
  bool internal_clock() const { return seq_settings_.tempo >= 40; }
  bool has_drums() const { return sequencer_running_ && seq_settings_.has_drums(); }
  bool has_arpeggiator() const { return seq_settings_.arp_mode && pressed_keys_.size(); }
  uint8_t remote_instrument() const { return drum_remote_control_current_instrument_; }

  void TouchClock();
  void RefreshDrumSynthSettings();
  void RefreshDrumSynthMixing();

 private:
  void ResetArpeggiatorPattern();
  void StartClock();
  void StopClock();
  void StartArpeggiator();
  void ClockArpeggiator();
  void StopArpeggiator();
  void StepArpeggiator();
  void StartSequencer();
  void ClockSequencer();
  void StopSequencer();
  void StartDrumMachine() { drum_sequencer_step_ = 0; }
  void ClockDrumMachine();
  uint8_t ReadDrumMap(uint8_t step, uint8_t instrument, uint8_t x, uint8_t y);

  Random* rng_ = nullptr;
  MidiOutListener* out_ = nullptr;
  ::anu::Clock clock_;
  SequencerSettings seq_settings_;
  Sequence sequence_;
  Voice voice_;
  DrumSynth drum_synth_;

  bool ignore_note_off_messages_;
  uint8_t clock_pulse_;
  uint8_t clock_counter_;
  uint8_t lfo_sync_counter_;
  NoteStack<16> pressed_keys_;
  bool sequencer_running_;
  bool sequencer_recording_;
  bool clock_running_;
  uint8_t sequencer_note_;
  int8_t sequencer_transposition_;
  uint8_t sequencer_velocity_;
  uint8_t previous_generated_note_;
  uint16_t arp_pattern_mask_;
  uint8_t arp_pattern_step_;
  int8_t arp_direction_;
  int8_t arp_step_;
  int8_t arp_octave_;
  uint8_t drum_sequencer_step_;
  uint8_t drum_sequencer_perturbation_[kNumDrumParts];
  uint8_t drum_remote_control_current_instrument_;
};

}  // namespace anu
