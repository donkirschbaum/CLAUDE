// Port of the Anushri firmware (c) 2012 Emilie Gillet, GPLv3.
#include "anu_core.h"

namespace anu {

// ===========================================================================
// DrumSynth
// ===========================================================================

static const uint8_t drum_presets[15][5] = {
  // BD
  { 60, 18, 104, 120, 0 },
  { 56, 60, 120, 150, 0 },
  { 60, 42, 130, 180, 14 },
  { 72, 20, 66, 224, 0 },
  { 42, 52, 106, 160, 60 },
  // SD
  { 108, 18, 16, 72, 64 },
  { 108, 36, 32, 96, 140 },
  { 108, 36, 50, 90, 180 },
  { 116, 36, 32, 80, 150 },
  { 124, 40, 190, 90, 40 },
  // HH
  { 124, 0, 0, 80, 0 },
  { 136, 0, 0, 80, 0 },
  { 136, 0, 0, 110, 0 },
  { 148, 0, 0, 90, 0 },
  { 154, 0, 0, 45, 0 },
};

void DrumSynth::Init(Random* rng) {
  std::memset(state_, 0, sizeof(state_));
  std::memset(patch_, 0, sizeof(patch_));
  rng_ = rng;
  sample_ = 128;
  sample_counter_ = 0;
  sample_rate_ = 0;
  block_counter_ = 0;
  playing_ = false;
  for (uint8_t i = 0; i < kNumDrumInstruments; ++i) {
    state_[i].amp_env_phase = 0xffff;
    state_[i].pitch_env_phase = 0xffff;
  }
}

void DrumSynth::Trigger(uint8_t instrument, uint8_t level) {
  if (instrument >= kNumDrumInstruments) return;
  state_[instrument].phase = 0;
  state_[instrument].pitch_env_phase = 0;
  state_[instrument].amp_env_phase = 0;
  state_[instrument].pitch_env_increment = lut_res_drm_env_increments[patch_[instrument].pitch_decay];
  state_[instrument].amp_env_increment = lut_res_drm_env_increments[patch_[instrument].amp_decay];
  state_[instrument].level = U8U8MulShift8(level, patch_[instrument].level);
  playing_ = true;
}

void DrumSynth::MorphPatch(uint8_t instrument, uint8_t value) {
  uint8_t offset = static_cast<uint8_t>(instrument * 5 + (value >> 6));
  uint8_t balance = static_cast<uint8_t>(value << 2);
  const uint8_t* a = drum_presets[offset];
  // The original reads one preset past the end of the instrument's bank
  // for the topmost knob position with a 0 balance - harmless, but we clamp.
  const uint8_t* b = drum_presets[offset + 1 < 15 ? offset + 1 : 14];
  uint8_t* address = &patch_[instrument].pitch;
  for (uint8_t i = 0; i < 5; ++i) {
    address[i] = U8Mix(a[i], b[i], balance);
  }
}

static const uint8_t drums_cc_map[] = {
  0, 1, 2, 3, 4, 5,
  6, 7, 8, 9, 10, 11,
  12, 15, 17
};

void DrumSynth::SetParameterCc(uint8_t cc, uint8_t value) {
  if (cc < 16 || cc > 30) return;
  uint8_t address = drums_cc_map[cc - 16];
  uint8_t* data = reinterpret_cast<uint8_t*>(patch_);
  data[address] = static_cast<uint8_t>(value << 1);
}

void DrumSynth::SetBandwidth(uint8_t bandwidth) {
  bandwidth = static_cast<uint8_t>(~bandwidth);
  sample_rate_ = bandwidth >> 3;
}

void DrumSynth::SetBalance(uint8_t mix) {
  if (mix < 128) {
    patch_[0].level = 255;
    patch_[1].level = static_cast<uint8_t>(mix << 1);
  } else {
    patch_[0].level = static_cast<uint8_t>(~((mix - 128) << 1));
    patch_[1].level = 255;
  }
  patch_[2].level = patch_[1].level >> 1;
}

uint8_t DrumSynth::RenderSample() {
  if (block_counter_ == 0) {
    UpdateModulations();
    noise_ = rng_->state_msb();
  }
  block_counter_ = (block_counter_ + 1) & (kDrumBlockSize - 1);

  ++sample_counter_;
  int16_t mix = 128;
  noise_ = static_cast<uint8_t>(noise_ * 73 + 1);

  state_[0].phase += state_[0].phase_increment;
  state_[1].phase += state_[1].phase_increment;
  state_[2].phase += state_[2].phase_increment;
  uint16_t phase_0 = state_[0].phase;
  uint16_t phase_1 = state_[1].phase;
  uint16_t phase_2 = state_[2].phase;

  // BD: interpolated sine.
  int8_t bd = static_cast<int8_t>(wav_res_sine[phase_0 >> 8]);
  int8_t bd_2 = static_cast<int8_t>(wav_res_sine[(phase_0 >> 8) + 1]);
  bd = static_cast<int8_t>(bd + S8U8MulShift8(static_cast<int8_t>(bd_2 - bd), phase_0 & 0xff));
  mix += S8U8MulShift8(bd, state_[0].amp_level);

  // SD: sine + noise.
  int8_t sd = static_cast<int8_t>(wav_res_sine[phase_1 >> 8]);
  mix += S8U8MulShift8(sd, state_[1].amp_level);
  mix += S8U8MulShift8(static_cast<int8_t>(noise_), state_[1].amp_level_noise);

  // HH: sample playback.
  int8_t hh = static_cast<int8_t>(wav_res_hh[phase_2 >> 4]);
  mix += S8U8MulShift8(hh, state_[2].amp_level);

  if (sample_counter_ > sample_rate_) {
    if (mix > 255) mix = 255;
    if (mix < 0) mix = 0;
    sample_ = static_cast<uint8_t>(mix);
    sample_counter_ = 0;
  }
  return sample_;
}

void DrumSynth::UpdateModulations() {
  playing_ = false;
  for (uint8_t i = 0; i < kNumDrumInstruments; ++i) {
    state_[i].amp_env_phase += state_[i].amp_env_increment;
    if (state_[i].amp_env_phase < state_[i].amp_env_increment) {
      state_[i].amp_env_phase = 0xffff;
      state_[i].amp_env_increment = 0;
    }
    state_[i].amp_level = U8U8MulShift8(
        state_[i].level, InterpolateSample(wav_res_drm_envelope, state_[i].amp_env_phase));

    state_[i].pitch_env_phase += state_[i].pitch_env_increment;
    if (state_[i].pitch_env_phase < state_[i].pitch_env_increment) {
      state_[i].pitch_env_phase = 0xffff;
      state_[i].pitch_env_increment = 0;
    }

    uint16_t pitch = static_cast<uint16_t>(patch_[i].pitch << 8);
    if (i == 0) {
      pitch = static_cast<uint16_t>(pitch + U8U8Mul(rng_->GetByte(), patch_[i].crunchiness));
    }
    pitch = static_cast<uint16_t>(pitch + U8U8Mul(
        patch_[i].pitch_mod, InterpolateSample(wav_res_drm_envelope, state_[i].pitch_env_phase)));
    state_[i].phase_increment = InterpolateIncreasing(lut_res_drm_phase_increments, pitch);
    if (state_[i].amp_env_increment) playing_ = true;
  }
  state_[1].amp_level_noise = U8U8MulShift8(state_[1].amp_level, patch_[1].crunchiness);
  state_[1].amp_level = U8U8MulShift8(state_[1].amp_level, static_cast<uint8_t>(~patch_[1].crunchiness));
  state_[2].phase_increment >>= 6;
}

// ===========================================================================
// Voice
// ===========================================================================

static const Patch init_patch = {
  0, 0, 0, 0, 0, 0,
  0, 128,
  128, 128, 0, 0,
  0, 64, 128, 128, 128, 0, { 0, 0 },
  0, 96, 160, 0,
  0, 0, 0, 0
};

void Voice::Init(Random* rng) {
  patch_ = init_patch;
  lfo_.Init(rng);
  vibrato_lfo_.Init(rng);
  vcf_envelope_.Init();
  vca_envelope_.Init();
  mod_envelope_.Init();
  lfo_8_bits_ = 0;
  dco_pitch_ = 60 * 128;
  pitch_counter_ = 0xffff;
  pitch_increment_ = 0;
  pitch_ = pitch_source_ = pitch_target_ = 60 * 128;
  mod_velocity_ = 100;
  mod_accent_ = 0;
  volume_ = 240;
  std::memset(&cv_, 0, sizeof(cv_));
  cv_.vco_note = cv_.dco_note = 60.0f;
  ResetAllControllers();
  UpdateEnvelopeParameters();
}

void Voice::ControlChange(uint8_t controller, uint8_t value) {
  switch (controller) {
    case 7:  // Volume
      volume_ = static_cast<uint8_t>(value << 1);
      break;
    case 1:  // Modulation wheel
      mod_wheel_ = value;
      break;
    case 2:  // Breath controller
      mod_wheel_2_ = value;
      break;
  }
}

void Voice::SetValue(uint8_t offset, uint8_t value) {
  if (offset >= PRM_PATCH_LAST) return;
  reinterpret_cast<uint8_t*>(&patch_)[offset] = value;
  if (offset >= PRM_PATCH_ENV_ATTACK && offset < PRM_PATCH_ENV_PADDING_1) {
    UpdateEnvelopeParameters();
  }
}

void Voice::UpdateEnvelopeParameters() {
  vcf_envelope_.Update(patch_.env_attack, patch_.env_decay, patch_.env_sustain, patch_.env_release);
  mod_envelope_.Update(patch_.env_attack, patch_.env_decay, 0, patch_.env_decay);
  uint8_t morph = patch_.env_vca_morph;
  uint8_t vca_a, vca_d, vca_s, vca_r;
  if (morph < 128) {
    morph = static_cast<uint8_t>(morph << 1);
    vca_a = U8Mix(patch_.env_attack, 0, morph);
    vca_d = U8Mix(patch_.env_decay, patch_.env_decay >> 1, morph);
    vca_s = U8Mix(patch_.env_sustain, 192, morph);
    vca_r = U8Mix(patch_.env_release, patch_.env_release >> 1, morph);
  } else {
    morph = static_cast<uint8_t>((morph - 128) << 1);
    vca_a = 0;
    vca_d = U8Mix(patch_.env_decay >> 1, 0, morph);
    vca_s = U8Mix(192, 255, morph);
    vca_r = U8Mix(patch_.env_release >> 1, 8, morph);
  }
  vca_envelope_.Update(vca_a, vca_d, vca_s, vca_r);
}

void Voice::AllSoundOff() {
  vca_envelope_.Trigger(ENV_SEGMENT_DEAD);
  vcf_envelope_.Trigger(ENV_SEGMENT_DEAD);
  mod_envelope_.Trigger(ENV_SEGMENT_DEAD);
}

void Voice::ResetAllControllers() {
  mod_pitch_bend_ = 8192;
  mod_wheel_ = 0;
  mod_wheel_2_ = 0;
  mod_aftertouch_ = 0;
}

void Voice::Refresh() {
  lfo_.set_shape(patch_.lfo_shape);
  if (patch_.lfo_rate >= 2) {
    lfo_.set_phase_increment(lut_res_lfo_increments[patch_.lfo_rate]);
  }
  vibrato_lfo_.set_phase_increment(lut_res_lfo_increments[96 + (patch_.vibrato_rate >> 1)]);

  uint8_t mod_wheel_pitch = 0;
  uint8_t mod_wheel_growl = 0;
  uint8_t vibrato_destination = patch_.vibrato_destination;
  if (vibrato_destination < 128) {
    mod_wheel_pitch = mod_wheel_;
    mod_wheel_growl = U8U8MulShift8(static_cast<uint8_t>(vibrato_destination << 1), mod_wheel_);
  } else {
    vibrato_destination = static_cast<uint8_t>(~vibrato_destination);
    mod_wheel_growl = mod_wheel_;
    mod_wheel_pitch = U8U8MulShift8(static_cast<uint8_t>(vibrato_destination << 1), mod_wheel_);
  }

  uint16_t lfo_unsigned = lfo_.Render();
  int16_t lfo = static_cast<int16_t>(lfo_unsigned - 32768);
  int16_t vibrato_lfo = static_cast<int16_t>(vibrato_lfo_.Render() - 32768);
  lfo_8_bits_ = static_cast<uint8_t>(lfo_unsigned >> 8);
  uint16_t mod_envelope = mod_envelope_.Render();

  // Glide.
  pitch_counter_ = static_cast<uint16_t>(pitch_counter_ + pitch_increment_);
  if (pitch_counter_ < pitch_increment_) {
    pitch_counter_ = 0xffff;
    pitch_increment_ = 0;
  }
  int32_t pitch = Mix(pitch_source_, pitch_target_, pitch_counter_);
  pitch_ = static_cast<int16_t>(pitch);
  pitch += (mod_pitch_bend_ - 8192) >> 5;
  pitch += S8U8Mul(patch_.vco_dco_range, 6) << 8;
  pitch += patch_.vco_dco_fine;
  pitch += S8U8MulShift8(static_cast<int8_t>(vibrato_lfo >> 8), mod_wheel_pitch);
  dco_pitch_ = static_cast<int16_t>(pitch);

  pitch += S8U8Mul(patch_.vco_detune, 128);
  pitch += patch_.vco_fine;
  pitch += U16U8MulShift8(mod_envelope, patch_.vco_env_amount) >> 4;
  pitch += S16U8MulShift8(lfo, patch_.vco_lfo_amount) >> 4;

  // The DAC + VCO expo converter span roughly +/- 4 octaves around C4.
  const int32_t kVcoMin = 60 * 128 - 6291;
  const int32_t kVcoMax = 60 * 128 + 6291;
  if (pitch < kVcoMin) pitch = kVcoMin;
  if (pitch > kVcoMax) pitch = kVcoMax;
  cv_.vco_note = static_cast<float>(pitch) / 128.0f;
  cv_.dco_note = static_cast<float>(dco_pitch_) / 128.0f;

  // PW CV.
  int32_t pw = 0;
  pw += U16U8MulShift8(mod_envelope, patch_.pw_env_amount) >> 2;
  pw += U16U8MulShift8(static_cast<uint16_t>(lfo + 32768), patch_.pw_lfo_amount) >> 2;
  pw >>= 2;
  if (pw < 0) pw = 0;
  if (pw > 4095) pw = 4095;
  cv_.pw = static_cast<float>(pw) / 4095.0f;

  // VCF CV.
  uint16_t vcf_envelope = vcf_envelope_.Render();
  int32_t cutoff = 60 * 128;
  cutoff += S16U8MulShift8(static_cast<int16_t>(dco_pitch_ - 60 * 128), patch_.cutoff_tracking) << 1;
  cutoff += S8U8Mul(static_cast<int8_t>(patch_.cutoff_bias + 128), 64);
  uint16_t growl_amount = mod_wheel_growl;
  growl_amount += mod_wheel_2_;
  if (growl_amount >= 255) growl_amount = 255;
  cutoff += S16U8MulShift8(vibrato_lfo, static_cast<uint8_t>(growl_amount)) >> 3;
  uint16_t env_amount = static_cast<uint16_t>(patch_.cutoff_env_amount + (mod_accent_ >> 1));
  env_amount += U8U8MulShift8(mod_velocity_, patch_.kbd_velocity_vcf_amount);
  if (env_amount > 255) env_amount = 255;
  cutoff += U16U8MulShift8(vcf_envelope, static_cast<uint8_t>(env_amount)) >> 2;
  cutoff += S16U8MulShift8(lfo, patch_.cutoff_lfo_amount) >> 3;
  const int32_t kVcfRange = 12583;  // DAC span in 1/128th of semitones.
  int32_t offset = cutoff - 60 * 128;
  if (offset < -kVcfRange) offset = -kVcfRange;
  if (offset > kVcfRange) offset = kVcfRange;
  cv_.vcf_offset = static_cast<float>(offset) / 128.0f;

  // VCA CV.
  uint16_t vca_envelope = U16U8MulShift8(
      vca_envelope_.Render(),
      U8Mix(255, static_cast<uint8_t>(mod_velocity_ << 1), patch_.kbd_velocity_vca_amount));
  vca_envelope = U16U8MulShift8(vca_envelope, volume_);
  uint16_t vca_cv = U16ShiftRight4(vca_envelope);
  cv_.vca = static_cast<float>(vca_cv) / 4095.0f;
  cv_.dco_muted = vca_cv == 0;
}

void Voice::GateOn() {
  vca_envelope_.Trigger(ENV_SEGMENT_ATTACK);
  vcf_envelope_.Trigger(ENV_SEGMENT_ATTACK);
  mod_envelope_.Trigger(ENV_SEGMENT_ATTACK);
}

void Voice::GateOff() {
  vca_envelope_.Trigger(ENV_SEGMENT_RELEASE);
  vcf_envelope_.Trigger(ENV_SEGMENT_RELEASE);
  mod_envelope_.Trigger(ENV_SEGMENT_RELEASE);
}

void Voice::NoteOn(uint8_t note, uint8_t velocity, uint8_t slide, uint8_t accent, bool legato) {
  if (!legato || !patch_.env_legato_mode) GateOn();
  pitch_source_ = pitch_;
  pitch_target_ = static_cast<int16_t>(U8U8Mul(note, 128));
  uint16_t v = static_cast<uint16_t>(velocity + accent);
  if (v > 127) v = 127;
  mod_velocity_ = static_cast<uint8_t>(v);
  mod_accent_ = accent;
  if (!pitch_source_ || (!legato && patch_.env_legato_mode)) {
    pitch_source_ = pitch_target_;
  }
  uint16_t slide_time = static_cast<uint16_t>(patch_.kbd_glide + slide);
  if (slide_time > 255) slide_time = 255;
  pitch_increment_ = lut_res_glide_increments[slide_time];
  pitch_counter_ = 0;
}

void Voice::NoteOff(uint8_t note) {
  if (note != 0xff) GateOff();
}

// ===========================================================================
// VoiceController
// ===========================================================================

static const uint8_t* const drum_map[3][3] = {
  { wav_res_drum_map_node_8, wav_res_drum_map_node_3, wav_res_drum_map_node_6 },
  { wav_res_drum_map_node_2, wav_res_drum_map_node_4, wav_res_drum_map_node_0 },
  { wav_res_drum_map_node_7, wav_res_drum_map_node_1, wav_res_drum_map_node_5 },
};

// Clock PPQN setting 2 (factory default): 24 ppqn, sequencer steps on 16ths.
static const uint8_t kClockDivision = 6;
static const uint8_t kClockRateCompensation = 1;

void VoiceController::Init(Random* rng, MidiOutListener* out) {
  rng_ = rng;
  out_ = out;
  std::memset(&seq_settings_, 0, sizeof(seq_settings_));
  seq_settings_.tempo = 120;
  seq_settings_.drums_balance = 128;
  seq_settings_.drums_bandwidth = 255;
  std::memset(&sequence_, 0, sizeof(sequence_));
  sequence_.num_notes = 4;
  sequence_.notes[0] = 60;
  sequence_.notes[1] = 60;
  sequence_.notes[2] = 48;
  sequence_.notes[3] = 48;

  pressed_keys_.Init();
  voice_.Init(rng);
  drum_synth_.Init(rng);
  ignore_note_off_messages_ = false;
  sequencer_running_ = false;
  sequencer_recording_ = false;
  clock_running_ = false;
  clock_pulse_ = 0;
  clock_counter_ = 0;
  lfo_sync_counter_ = 0;
  sequencer_note_ = 0;
  sequencer_transposition_ = 0;
  sequencer_velocity_ = 64;
  previous_generated_note_ = 0xff;
  arp_pattern_mask_ = 1;
  arp_pattern_step_ = 0;
  arp_direction_ = 1;
  arp_step_ = 0;
  arp_octave_ = 0;
  drum_sequencer_step_ = 0;
  std::memset(drum_sequencer_perturbation_, 0, sizeof(drum_sequencer_perturbation_));
  drum_remote_control_current_instrument_ = 0;

  voice_.set_note(60);
  TouchClock();
  clock_.Reset();
  RefreshDrumSynthSettings();
  RefreshDrumSynthMixing();
}

void VoiceController::RefreshDrumSynthSettings() {
  for (uint8_t i = 0; i < kNumDrumParts; ++i) {
    drum_synth_.MorphPatch(i, seq_settings_.drums_tone[i]);
  }
}

void VoiceController::RefreshDrumSynthMixing() {
  drum_synth_.SetBalance(seq_settings_.drums_balance);
  drum_synth_.SetBandwidth(seq_settings_.drums_bandwidth);
}

void VoiceController::TouchClock() {
  clock_.Update(seq_settings_.tempo < 40 ? 120 : seq_settings_.tempo, 1,
                seq_settings_.swing >> 1, kClockRateCompensation);
}

void VoiceController::SetValue(uint8_t offset, uint8_t value) {
  if (offset >= PRM_SEQ_LAST) return;
  uint8_t* bytes = reinterpret_cast<uint8_t*>(&seq_settings_);
  uint8_t previous_value = bytes[offset];
  bytes[offset] = value;
  if (value != previous_value) {
    if (offset == PRM_SEQ_ARP_MODE) {
      arp_direction_ = seq_settings_.arp_direction() == ARPEGGIO_DIRECTION_DOWN ? -1 : 1;
    } else if (offset >= PRM_SEQ_TEMPO && offset <= PRM_SEQ_SWING) {
      TouchClock();
    } else if (offset >= PRM_SEQ_DRUMS_BD_TONE && offset <= PRM_SEQ_DRUMS_HH_TONE) {
      RefreshDrumSynthSettings();
    } else if (offset >= PRM_SEQ_DRUMS_BALANCE && offset <= PRM_SEQ_DRUMS_BANDWIDTH) {
      RefreshDrumSynthMixing();
    }
  }
}

void VoiceController::NoteOn(uint8_t note, uint8_t velocity) {
  if (velocity == 0) {
    NoteOff(note);
    return;
  }
  if (sequencer_running_ && sequence_.num_notes) {
    sequencer_transposition_ = static_cast<int8_t>(note - 60);
    sequencer_velocity_ = velocity;
  } else {
    pressed_keys_.NoteOn(note, velocity);
    if (seq_settings_.arp_mode == 0) {
      voice_.NoteOn(note, velocity, 0, 0, pressed_keys_.size() != 1);
    } else if (pressed_keys_.size() == 1 && !clock_running_) {
      StartClock();
      StartArpeggiator();
    }
  }
  if (sequencer_recording_) {
    sequence_.notes[sequence_.num_notes++] = note;
    if (sequence_.num_notes == 128) sequencer_recording_ = false;
  }
}

void VoiceController::NoteOff(uint8_t note) {
  if (!(sequencer_running_ && sequence_.num_notes) && !ignore_note_off_messages_) {
    if (seq_settings_.arp_mode == 0) {
      uint8_t top_note = pressed_keys_.most_recent_note().note;
      pressed_keys_.NoteOff(note);
      if (pressed_keys_.size() == 0) {
        voice_.NoteOff(note);
      } else if (top_note == note) {
        voice_.NoteOn(pressed_keys_.most_recent_note().note,
                      pressed_keys_.most_recent_note().velocity, 0, 0, true);
      }
    } else {
      pressed_keys_.NoteOff(note);
      if (pressed_keys_.size() == 0) {
        StopArpeggiator();
        if (!sequencer_running_) StopClock();
      }
    }
  }
}

void VoiceController::ReleaseAllHeldNotes() {
  ignore_note_off_messages_ = false;
  while (pressed_keys_.size()) {
    NoteOff(pressed_keys_.most_recent_note().note);
  }
}

void VoiceController::ControlChange(uint8_t controller, uint8_t value) {
  if (controller == 1 && sequencer_recording_ && value > 0x40) {
    uint8_t idx = sequence_.num_notes >> 3;
    uint8_t mask = static_cast<uint8_t>(1 << (sequence_.num_notes & 0x7));
    if (idx < 16) sequence_.accents[idx] |= mask;
  }
  voice_.ControlChange(controller, value);
  if (controller == 64) {
    if (value >= 64) ignore_note_off_messages_ = true;
    else ReleaseAllHeldNotes();
  }
}

void VoiceController::PitchBend(uint16_t pitch_bend) {
  voice_.PitchBend(pitch_bend);
  if (sequencer_recording_ && (pitch_bend > 8192 + 2048 || pitch_bend < 8192 - 2048)) {
    uint8_t idx = sequence_.num_notes >> 3;
    uint8_t mask = static_cast<uint8_t>(1 << (sequence_.num_notes & 0x7));
    if (idx < 16) sequence_.slides[idx] |= mask;
  }
}

void VoiceController::AllSoundOff() {
  pressed_keys_.Clear();
  voice_.AllSoundOff();
}

void VoiceController::AllNotesOff() {
  pressed_keys_.Clear();
  voice_.GateOff();
}

void VoiceController::Clock() {
  clock_pulse_ = 8;
  voice_.set_lfo_pll_target_phase(lfo_sync_counter_);
  if (!clock_counter_) {
    ClockArpeggiator();
    ClockSequencer();
    ClockDrumMachine();
  }
  ++clock_counter_;
  if (clock_counter_ >= kClockDivision) clock_counter_ = 0;
  ++lfo_sync_counter_;
  if (lfo_sync_counter_ == 24) lfo_sync_counter_ = 0;
}

void VoiceController::AlignToPulse(int64_t pulse) {
  if (pulse < 0) pulse = 0;
  clock_counter_ = static_cast<uint8_t>(pulse % kClockDivision);
  lfo_sync_counter_ = static_cast<uint8_t>(pulse % 24);
  int64_t step = pulse / kClockDivision;
  if (clock_counter_ != 0) ++step;  // next step boundary
  drum_sequencer_step_ = static_cast<uint8_t>(step % 16);
  if (sequence_.num_notes) {
    sequencer_note_ = static_cast<uint8_t>(step % sequence_.num_notes);
  }
}

void VoiceController::StartClock() {
  clock_.Reset();
  clock_counter_ = 0;
  lfo_sync_counter_ = 0;
  clock_running_ = true;
}

void VoiceController::StartSequencer() {
  if (!(pressed_keys_.size() && sequence_.num_notes == 0)) AllSoundOff();
  if (sequencer_recording_) StopRecording();
  sequencer_note_ = 0;
  sequencer_running_ = true;
}

void VoiceController::StartArpeggiator() {
  voice_.AllSoundOff();
  previous_generated_note_ = 0xff;
  arp_pattern_mask_ = 0x1;
  arp_pattern_step_ = 0;
  arp_direction_ = seq_settings_.arp_direction() == ARPEGGIO_DIRECTION_DOWN ? -1 : 1;
  ResetArpeggiatorPattern();
}

void VoiceController::StopClock() {
  clock_running_ = false;
}

void VoiceController::StopSequencer() {
  AllSoundOff();
  out_->OnInternalNoteOff(previous_generated_note_);
  previous_generated_note_ = 0xff;
  sequencer_running_ = false;
}

void VoiceController::StopArpeggiator() {
  AllNotesOff();
  out_->OnInternalNoteOff(previous_generated_note_);
  previous_generated_note_ = 0xff;
}

void VoiceController::ClockSequencer() {
  if (!sequence_.num_notes || !sequencer_running_) return;
  if (sequencer_note_ >= sequence_.num_notes) sequencer_note_ = 0;
  int16_t note = sequence_.notes[sequencer_note_];
  if (note == 0xff) {
    voice_.NoteOff(previous_generated_note_);
    out_->OnInternalNoteOff(previous_generated_note_);
    previous_generated_note_ = 0xff;
  } else if (note == 0xfe) {
    // Tie.
  } else {
    uint8_t idx = sequencer_note_ >> 3;
    uint8_t mask = static_cast<uint8_t>(1 << (sequencer_note_ & 0x7));
    note += sequencer_transposition_;
    if (note < 0) note = 0;
    if (note > 127) note = 127;
    bool slid = sequence_.slides[idx] & mask;
    bool accented = sequence_.accents[idx] & mask;
    if (!slid) out_->OnInternalNoteOff(previous_generated_note_);
    voice_.NoteOn(static_cast<uint8_t>(note), sequencer_velocity_, slid ? 0x60 : 0,
                  accented ? 0x70 : 0, slid);
    out_->OnInternalNoteOn(static_cast<uint8_t>(note), accented ? 0x7f : sequencer_velocity_);
    if (previous_generated_note_ != 0xff && slid) {
      out_->OnInternalNoteOff(previous_generated_note_);
    }
    previous_generated_note_ = static_cast<uint8_t>(note);
  }
  ++sequencer_note_;
  if (sequencer_note_ >= sequence_.num_notes) sequencer_note_ = 0;
}

void VoiceController::ClockDrumMachine() {
  uint16_t step_mask = static_cast<uint16_t>(1 << drum_sequencer_step_);
  uint8_t override_mask = 1;
  if (has_drums()) {
    uint8_t x = seq_settings_.drums_x;
    uint8_t y = seq_settings_.drums_y;
    for (uint8_t i = 0; i < kNumDrumParts; ++i) {
      uint8_t level = ReadDrumMap(drum_sequencer_step_, i, x, y);
      if (level < 255 - drum_sequencer_perturbation_[i]) {
        level = static_cast<uint8_t>(level + drum_sequencer_perturbation_[i]);
      }
      uint8_t threshold = static_cast<uint8_t>(~seq_settings_.drums_density[i]);
      if (seq_settings_.drums_override & override_mask) {
        level = (seq_settings_.drums_pattern[i] & step_mask) ? 255 : 0;
        threshold = 0;
      }
      if (level > threshold) {
        uint8_t trig_level = static_cast<uint8_t>(128 + (level >> 1));
        drum_synth_.Trigger(i, trig_level);
        out_->OnDrumNote(i, trig_level >> 1);
      }
      override_mask = static_cast<uint8_t>(override_mask << 1);
    }
  }
  ++drum_sequencer_step_;
  if (drum_sequencer_step_ >= 16) {
    drum_sequencer_step_ = 0;
    for (uint8_t i = 0; i < kNumDrumParts; ++i) {
      drum_sequencer_perturbation_[i] = rng_->GetByte() >> 3;
    }
  }
}

static const uint8_t slide_probability[] = {
  1, 0, 4, 4, 2, 0, 8, 6, 1, 1, 0, 0, 0, 0, 15, 15,
};
static const uint8_t accent_level[] = {
  16, 0, 2, 0, 1, 0, 0, 0, 8, 0, 1, 0, 4, 0, 2, 2,
};

void VoiceController::ClockArpeggiator() {
  if (has_arpeggiator()) {
    uint8_t pattern_index = seq_settings_.arp_pattern < 6 ? seq_settings_.arp_pattern : 5;
    uint16_t pattern = lut_res_arpeggiator_patterns[pattern_index];
    bool has_arpeggiator_note = arp_pattern_mask_ & pattern;
    if (has_arpeggiator_note) {
      StepArpeggiator();
      const NoteEntry& arpeggio_note = pressed_keys_.sorted_note(static_cast<uint8_t>(arp_step_));
      int16_t note = arpeggio_note.note;
      uint8_t velocity = arpeggio_note.velocity;
      note += 12 * arp_octave_;
      while (note > 127) note -= 12;
      uint8_t random = rng_->GetByte();
      uint16_t slide_threshold = U8U8Mul(slide_probability[arp_pattern_step_], seq_settings_.acidity);
      if (arp_step_ == 0) slide_threshold >>= 1;
      uint8_t accent = static_cast<uint8_t>(U8U8Mul(accent_level[arp_pattern_step_], seq_settings_.acidity) >> 1);
      accent = static_cast<uint8_t>((accent + U8U8MulShift8(accent, random)) >> 1);
      bool slid = random < static_cast<uint8_t>(slide_threshold);
      if (!slid) out_->OnInternalNoteOff(previous_generated_note_);
      voice_.NoteOn(static_cast<uint8_t>(note), velocity, slid ? 0x60 : 0, accent, slid);
      if (slid) out_->OnInternalNoteOff(previous_generated_note_);
      out_->OnInternalNoteOn(static_cast<uint8_t>(note), velocity);
      previous_generated_note_ = static_cast<uint8_t>(note);
    } else {
      voice_.NoteOff(previous_generated_note_);
      out_->OnInternalNoteOff(previous_generated_note_);
      previous_generated_note_ = 0xff;
    }
  }
  ++arp_pattern_step_;
  arp_pattern_mask_ = static_cast<uint16_t>(arp_pattern_mask_ << 1);
  if (!arp_pattern_mask_) {
    arp_pattern_mask_ = 1;
    arp_pattern_step_ = 0;
  }
}

void VoiceController::ResetArpeggiatorPattern() {
  if (arp_direction_ == 1) {
    arp_octave_ = 0;
    arp_step_ = 0;
  } else {
    arp_step_ = static_cast<int8_t>(pressed_keys_.size() - 1);
    arp_octave_ = static_cast<int8_t>(seq_settings_.arp_range() - 1);
  }
}

void VoiceController::StepArpeggiator() {
  uint8_t num_notes = pressed_keys_.size();
  if (!num_notes) return;
  if (seq_settings_.arp_direction() == ARPEGGIO_DIRECTION_RANDOM) {
    uint8_t random_byte = rng_->GetByte();
    arp_octave_ = static_cast<int8_t>(random_byte & 0xf);
    arp_step_ = static_cast<int8_t>((random_byte & 0xf0) >> 4);
    while (arp_octave_ >= seq_settings_.arp_range()) arp_octave_ = static_cast<int8_t>(arp_octave_ - seq_settings_.arp_range());
    while (arp_step_ >= num_notes) arp_step_ = static_cast<int8_t>(arp_step_ - num_notes);
  } else {
    arp_step_ = static_cast<int8_t>(arp_step_ + arp_direction_);
    bool change_octave = false;
    if (arp_step_ >= num_notes) {
      arp_step_ = 0;
      change_octave = true;
    } else if (arp_step_ < 0) {
      arp_step_ = static_cast<int8_t>(num_notes - 1);
      change_octave = true;
    }
    if (change_octave) {
      arp_octave_ = static_cast<int8_t>(arp_octave_ + arp_direction_);
      if (arp_octave_ >= seq_settings_.arp_range() || arp_octave_ < 0) {
        if (seq_settings_.arp_direction() == ARPEGGIO_DIRECTION_UP_DOWN) {
          arp_direction_ = static_cast<int8_t>(-arp_direction_);
          ResetArpeggiatorPattern();
          if (num_notes > 1 || seq_settings_.arp_range() > 1) StepArpeggiator();
        } else {
          ResetArpeggiatorPattern();
        }
      }
    }
  }
  if (arp_step_ < 0 || arp_step_ >= num_notes) arp_step_ = 0;
}

void VoiceController::StartRecording() {
  Stop();
  StopArpeggiator();
  seq_settings_.arp_mode = 0;
  sequencer_recording_ = true;
  std::memset(&sequence_, 0, sizeof(Sequence));
}

void VoiceController::StopRecording() {
  Stop();
  sequencer_recording_ = false;
  sequencer_transposition_ = 0;
  sequencer_velocity_ = 64;
}

uint8_t VoiceController::ReadDrumMap(uint8_t step, uint8_t instrument, uint8_t x, uint8_t y) {
  uint8_t i = x >> 7;
  uint8_t j = y >> 7;
  const uint8_t* a_map = drum_map[i][j];
  const uint8_t* b_map = drum_map[i + 1][j];
  const uint8_t* c_map = drum_map[i][j + 1];
  const uint8_t* d_map = drum_map[i + 1][j + 1];
  uint8_t offset = static_cast<uint8_t>(U8ShiftLeft4(instrument) + step);
  uint8_t a = a_map[offset];
  uint8_t b = b_map[offset];
  uint8_t c = c_map[offset];
  uint8_t d = d_map[offset];
  uint8_t xb = static_cast<uint8_t>(x << 1);
  uint8_t yb = static_cast<uint8_t>(y << 1);
  return U8Mix(U8Mix(a, b, xb), U8Mix(c, d, xb), yb);
}

static const uint8_t white_keys[] = {
  0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23, 24, 26
};

void VoiceController::RemoteControlDrumSequencer(uint8_t note) {
  if (note == 37) {
    seq_settings_.drums_override ^= 0xff;
  } else if (note == 39) {
    seq_settings_.drums_pattern[drum_remote_control_current_instrument_] = 0;
  } else {
    uint16_t step_mask = 1;
    for (uint8_t i = 0; i < kNumDrumParts; ++i) {
      if (note == 42 + (i << 1)) {
        drum_remote_control_current_instrument_ = i;
        return;
      }
    }
    for (uint8_t i = 0; i < 16; ++i) {
      if (note == 36 + white_keys[i]) {
        seq_settings_.drums_pattern[drum_remote_control_current_instrument_] ^= step_mask;
        return;
      }
      step_mask = static_cast<uint16_t>(step_mask << 1);
    }
  }
}

}  // namespace anu
