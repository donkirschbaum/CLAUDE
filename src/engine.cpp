#include "engine.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace anu {

static inline uint8_t ToU8(float v) {
  if (v <= 0.0f) return 0;
  if (v >= 1.0f) return 255;
  return static_cast<uint8_t>(std::lround(v * 255.0f));
}

static inline int RoundInt(float v) { return static_cast<int>(std::lround(v)); }

void AnuEngine::Init(double sample_rate, ParamListener* listener) {
  listener_ = listener;
  rng_.Seed(0x2161);
  vc_.Init(&rng_, this);
  for (uint32_t i = 0; i < P_COUNT; ++i) params_[i] = kParams[i].def;
  std::memset(learn_map_, 0, sizeof(learn_map_));
  std::memset(snapshot_learn_, 0, sizeof(snapshot_learn_));
  snapshot_sequence_ = vc_.sequence();
  std::memset(snapshot_patterns_, 0, sizeof(snapshot_patterns_));
  SetSampleRate(sample_rate);
  ApplyAllParameters();
}

void AnuEngine::SetSampleRate(double sample_rate) {
  sample_rate_ = sample_rate > 1000.0 ? sample_rate : 48000.0;
  tick_increment_ = kAudioRate / sample_rate_;
  analog_.Init(sample_rate_);
  float fc = 14000.0f;
  if (fc > 0.45f * static_cast<float>(sample_rate_)) fc = 0.45f * static_cast<float>(sample_rate_);
  drum_lp_coef_ = 1.0f - std::exp(-2.0f * static_cast<float>(M_PI) * fc / static_cast<float>(sample_rate_));
  drum_dc_coef_ = 1.0f - static_cast<float>(2.0 * M_PI * 10.0 / sample_rate_);
  ApplyAllParameters();
}

// ---------------------------------------------------------------------------
// Parameters

void AnuEngine::SetParameter(uint32_t index, float value) {
  if (index >= P_COUNT) return;
  const ParamInfo& p = kParams[index];
  if (p.kind == KIND_OUTPUT) return;
  if (value < p.min) value = p.min;
  if (value > p.max) value = p.max;
  float previous = params_[index];
  params_[index] = value;
  switch (index) {
    // Transport "buttons": act on changes only.
    case P_RUN: {
      bool on = value > 0.5f;
      if ((previous > 0.5f) != on) {
        if (on && !vc_.sequencer_running()) vc_.Start();
        else if (!on && vc_.sequencer_running()) vc_.Stop();
      }
      return;
    }
    case P_RECORD: {
      bool on = value > 0.5f;
      if ((previous > 0.5f) != on) {
        if (on && !vc_.sequencer_recording()) vc_.StartRecording();
        else if (!on && vc_.sequencer_recording()) vc_.StopRecording();
      }
      return;
    }
    case P_HOLD: {
      if ((previous > 0.5f) != (value > 0.5f)) {
        if (vc_.sequencer_recording()) {
          if (value > 0.5f) vc_.InsertTie();
        } else {
          vc_.SetHold(value > 0.5f);
        }
      }
      return;
    }
    default:
      ApplyParameter(index);
  }
}

static void SetCrossfade(Voice* voice, uint8_t offset, float v) {
  uint8_t pot = ToU8((v + 1.0f) * 0.5f);
  uint8_t value = wav_res_deadband[pot];
  if (value >= 128) {
    voice->SetValue(offset, 0);
    voice->SetValue(offset + 1, static_cast<uint8_t>((value - 128) << 1));
  } else {
    voice->SetValue(offset, static_cast<uint8_t>(255 - (value << 1)));
    voice->SetValue(offset + 1, 0);
  }
}

void AnuEngine::ApplyParameter(uint32_t index) {
  const float v = params_[index];
  Voice* voice = vc_.mutable_voice();
  AnalogKnobs knobs;
  bool update_knobs = false;
  switch (index) {
    case P_FM: case P_PW: case P_SYNC: case P_MIX: case P_SUB_LEVEL: case P_SUB_SOURCE:
    case P_CUTOFF: case P_RESONANCE: case P_VCF_MODE: case P_FUZZ:
      update_knobs = true;
      break;
    case P_VOLUME: voice->set_volume(static_cast<uint8_t>(RoundInt(v * 254.0f))); break;
    case P_DRUM_LEVEL: break;
    case P_VCO_MOD: SetCrossfade(voice, PRM_PATCH_VCO_ENV_AMOUNT, v); break;
    case P_PW_MOD: SetCrossfade(voice, PRM_PATCH_PW_ENV_AMOUNT, v); break;
    case P_VCF_ENV: voice->SetValue(PRM_PATCH_CUTOFF_ENV_AMOUNT, ToU8(v)); break;
    case P_VCF_LFO: voice->SetValue(PRM_PATCH_CUTOFF_LFO_AMOUNT, ToU8(v)); break;
    case P_GLIDE: voice->SetValue(PRM_PATCH_KBD_GLIDE, ToU8(v)); break;
    case P_VCO_RANGE: voice->SetValue(PRM_PATCH_VCO_DETUNE, static_cast<uint8_t>(static_cast<int8_t>(RoundInt(v)))); break;
    case P_VCO_DETUNE: voice->SetValue(PRM_PATCH_VCO_FINE, static_cast<uint8_t>(static_cast<int8_t>(RoundInt(v)))); break;
    case P_LFO_SHAPE: voice->SetValue(PRM_PATCH_LFO_SHAPE, static_cast<uint8_t>(RoundInt(v))); break;
    case P_LFO_RATE: voice->SetValue(PRM_PATCH_LFO_RATE, ToU8(v)); break;
    case P_ATTACK: voice->SetValue(PRM_PATCH_ENV_ATTACK, ToU8(v)); break;
    case P_DECAY: voice->SetValue(PRM_PATCH_ENV_DECAY, ToU8(v)); break;
    case P_SUSTAIN: voice->SetValue(PRM_PATCH_ENV_SUSTAIN, ToU8(v)); break;
    case P_RELEASE: voice->SetValue(PRM_PATCH_ENV_RELEASE, ToU8(v)); break;
    case P_VCA_ENV: voice->SetValue(PRM_PATCH_ENV_VCA_MORPH, ToU8(v)); break;
    case P_TEMPO: {
      int t = RoundInt(v);
      vc_.SetValue(PRM_SEQ_TEMPO, static_cast<uint8_t>(t < 40 ? 0 : t));
      vc_.TouchClock();
      break;
    }
    case P_SWING: vc_.SetValue(PRM_SEQ_SWING, ToU8(v)); break;
    case P_ARP_MODE: vc_.SetValue(PRM_SEQ_ARP_MODE, static_cast<uint8_t>(RoundInt(v))); break;
    case P_ARP_PATTERN: vc_.SetValue(PRM_SEQ_ARP_PATTERN, static_cast<uint8_t>(RoundInt(v))); break;
    case P_ACIDITY: vc_.SetValue(PRM_SEQ_ARP_ACIDITY, static_cast<uint8_t>(RoundInt(v))); break;
    case P_DCO_RANGE: voice->SetValue(PRM_PATCH_VCO_DCO_RANGE, static_cast<uint8_t>(static_cast<int8_t>(RoundInt(v)))); break;
    case P_DCO_FINE: voice->SetValue(PRM_PATCH_VCO_DCO_FINE, static_cast<uint8_t>(static_cast<int8_t>(RoundInt(v)))); break;
    case P_VIBRATO_RATE: voice->SetValue(PRM_PATCH_VIBRATO_RATE, ToU8(v)); break;
    case P_VIBRATO_DEST: voice->SetValue(PRM_PATCH_VIBRATO_DESTINATION, ToU8(v)); break;
    case P_VELOCITY_MOD: SetCrossfade(voice, PRM_PATCH_KBD_VCF_VELOCITY_AMOUNT, v); break;
    case P_DRUMS_X: vc_.SetValue(PRM_SEQ_DRUMS_X, ToU8(v)); break;
    case P_DRUMS_Y: vc_.SetValue(PRM_SEQ_DRUMS_Y, ToU8(v)); break;
    case P_BD_DENSITY: vc_.SetValue(PRM_SEQ_DRUMS_BD_DENSITY, ToU8(v)); break;
    case P_SD_DENSITY: vc_.SetValue(PRM_SEQ_DRUMS_SD_DENSITY, ToU8(v)); break;
    case P_HH_DENSITY: vc_.SetValue(PRM_SEQ_DRUMS_HH_DENSITY, ToU8(v)); break;
    case P_BD_TONE: vc_.SetValue(PRM_SEQ_DRUMS_BD_TONE, ToU8(v)); break;
    case P_SD_TONE: vc_.SetValue(PRM_SEQ_DRUMS_SD_TONE, ToU8(v)); break;
    case P_HH_TONE: vc_.SetValue(PRM_SEQ_DRUMS_HH_TONE, ToU8(v)); break;
    case P_DRUMS_BALANCE: vc_.SetValue(PRM_SEQ_DRUMS_BALANCE, ToU8(v)); break;
    case P_DRUMS_BANDWIDTH: vc_.SetValue(PRM_SEQ_DRUMS_BANDWIDTH, ToU8(v)); break;
    case P_CUTOFF_BIAS: voice->SetValue(PRM_PATCH_CUTOFF_BIAS, ToU8((v + 1.0f) * 0.5f)); break;
    case P_CUTOFF_TRACKING: voice->SetValue(PRM_PATCH_CUTOFF_TRACKING, ToU8(v)); break;
    case P_LEGATO: voice->SetValue(PRM_PATCH_ENV_LEGATO_MODE, v > 0.5f ? 255 : 0); break;
    case P_DRUM_OVERRIDE:
      vc_.mutable_sequencer_settings()->drums_override = v > 0.5f ? 0xff : 0;
      break;
    default:
      break;
  }
  if (update_knobs) {
    knobs.fm = params_[P_FM];
    knobs.pw = params_[P_PW];
    knobs.sync = params_[P_SYNC] > 0.5f;
    knobs.mix = params_[P_MIX];
    knobs.sub_level = params_[P_SUB_LEVEL];
    knobs.sub_source = RoundInt(params_[P_SUB_SOURCE]);
    knobs.cutoff = params_[P_CUTOFF];
    knobs.resonance = params_[P_RESONANCE];
    knobs.vcf_mode = RoundInt(params_[P_VCF_MODE]);
    knobs.fuzz = params_[P_FUZZ] > 0.5f;
    analog_.SetKnobs(knobs);
  }
}

void AnuEngine::ApplyAllParameters() {
  for (uint32_t i = 0; i < P_COUNT; ++i) {
    if (i == P_RUN || i == P_RECORD || i == P_HOLD || kParams[i].kind == KIND_OUTPUT) continue;
    ApplyParameter(i);
  }
}

void AnuEngine::ChangeParamFromEngine(uint32_t index, float value) {
  if (params_[index] == value) return;
  params_[index] = value;
  if (index != P_RUN && index != P_RECORD && index != P_HOLD) ApplyParameter(index);
  if (listener_) listener_->OnParameterChangedByEngine(index, value);
}

// Converts an incoming CC value to a parameter value. `side` is 0 for the
// first CC of a parameter, 1 for the second one, -1 for a learned CC.
bool AnuEngine::SetParamFromCc(uint32_t index, uint8_t value, int side) {
  const ParamInfo& p = kParams[index];
  uint8_t v8 = static_cast<uint8_t>(value << 1);
  float v = 0.0f;
  switch (p.kind) {
    case KIND_KNOB:
      v = value / 127.0f;
      break;
    case KIND_BIPOLAR:
      if (side >= 0 && (index == P_VCO_MOD || index == P_PW_MOD || index == P_VELOCITY_MOD)) {
        // Original CCs address the env and LFO amounts individually.
        v = (side == 0 ? -1.0f : 1.0f) * (value / 127.0f);
      } else {
        v = value / 127.0f * 2.0f - 1.0f;
      }
      break;
    case KIND_INT:
    case KIND_ENUM:
      if (side < 0) {
        v = p.min + std::round(value / 127.0f * (p.max - p.min));
      } else if (index == P_VCO_RANGE) {
        v = wav_res_pitch_deadband[v8];
      } else {
        int range = static_cast<int>(p.max - p.min) + 1;
        v = static_cast<float>(((range * v8) >> 8) + static_cast<int>(p.min));
      }
      break;
    case KIND_BOOL:
      if (index == P_RUN || index == P_RECORD || index == P_HOLD) {
        if (value < 64) return false;  // button release
        v = params_[index] > 0.5f ? 0.0f : 1.0f;
      } else {
        v = value >= 64 ? 1.0f : 0.0f;
      }
      break;
    default:
      return false;
  }
  if (index == P_TEMPO && side < 0 && v < 40) v = 39;
  float previous = params_[index];
  SetParameter(index, v);
  if (listener_ && params_[index] != previous) {
    listener_->OnParameterChangedByEngine(index, params_[index]);
  }
  return true;
}

void AnuEngine::HandleControlChange(uint8_t cc, uint8_t value) {
  int target = learn_target_.load();
  if (target >= 0 && target < static_cast<int>(P_COUNT)) {
    for (int i = 0; i < 128; ++i) {
      if (learn_map_[i] == target + 1) learn_map_[i] = 0;
    }
    learn_map_[cc] = static_cast<uint16_t>(target + 1);
    learn_target_.store(-1);
  }
  if (learn_map_[cc]) {
    SetParamFromCc(learn_map_[cc] - 1, value, -1);
  } else {
    for (uint32_t i = 0; i < P_COUNT; ++i) {
      if (kParams[i].cc == cc) SetParamFromCc(i, value, 0);
      else if (kParams[i].cc2 == cc) SetParamFromCc(i, value, 1);
    }
  }
  switch (cc) {
    case 1: case 2: case 64:
      vc_.ControlChange(cc, value);
      break;
    case 120:
      vc_.AllSoundOff();
      break;
    case 121:
      vc_.ResetAllControllers();
      break;
    case 123:
      vc_.AllNotesOff();
      break;
  }
}

// ---------------------------------------------------------------------------
// MIDI

void AnuEngine::EmitMidi(uint8_t status, uint8_t d1, uint8_t d2) {
  if (num_out_events_ >= kMaxOutEvents) return;
  OutMidi& e = out_events_[num_out_events_++];
  e.frame = current_frame_;
  e.size = 3;
  e.data[0] = status;
  e.data[1] = d1 & 0x7f;
  e.data[2] = d2 & 0x7f;
}

void AnuEngine::EmitMidi2(uint8_t status, uint8_t d1) {
  if (num_out_events_ >= kMaxOutEvents) return;
  OutMidi& e = out_events_[num_out_events_++];
  e.frame = current_frame_;
  e.size = 2;
  e.data[0] = status;
  e.data[1] = d1 & 0x7f;
  e.data[2] = 0;
}

void AnuEngine::OnInternalNoteOn(uint8_t note, uint8_t velocity) {
  if (params_[P_MIDI_OUT_SEQ] < 0.5f) return;
  seq_channel_sent_ = static_cast<uint8_t>(RoundInt(params_[P_MIDI_OUT_CHANNEL]) - 1) & 0x0f;
  EmitMidi(0x90 | seq_channel_sent_, note, velocity ? velocity : 1);
}

void AnuEngine::OnInternalNoteOff(uint8_t note) {
  if (note == 0xff || params_[P_MIDI_OUT_SEQ] < 0.5f) return;
  EmitMidi(0x80 | seq_channel_sent_, note, 0);
}

void AnuEngine::OnDrumNote(uint8_t instrument, uint8_t velocity) {
  if (params_[P_MIDI_OUT_DRUMS] < 0.5f || instrument > 2) return;
  if (drum_note_off_countdown_[instrument] >= 0) {
    EmitMidi(0x80 | drum_channel_sent_[instrument], drum_note_sent_[instrument], 0);
    drum_note_off_countdown_[instrument] = -1;
  }
  uint8_t channel = static_cast<uint8_t>(RoundInt(params_[P_DRUM_OUT_CHANNEL]) - 1) & 0x0f;
  uint8_t note = static_cast<uint8_t>(RoundInt(params_[P_BD_NOTE + instrument])) & 0x7f;
  EmitMidi(0x90 | channel, note, velocity ? velocity : 1);
  drum_note_sent_[instrument] = note;
  drum_channel_sent_[instrument] = channel;
  drum_note_off_countdown_[instrument] =
      static_cast<int32_t>(params_[P_DRUM_GATE_MS] * 0.001 * sample_rate_);
}

void AnuEngine::HandleMidi(const uint8_t* data, uint32_t size, uint32_t frame) {
  if (size < 1) return;
  current_frame_ = frame;
  uint8_t status = data[0];
  if (status >= 0xf0) return;  // system messages are handled by the host
  uint8_t type = status & 0xf0;
  uint8_t channel = status & 0x0f;
  uint8_t d1 = size > 1 ? data[1] & 0x7f : 0;
  uint8_t d2 = size > 2 ? data[2] & 0x7f : 0;

  if (params_[P_MIDI_THRU] > 0.5f) {
    if (type == 0xc0 || type == 0xd0) EmitMidi2(status, d1);
    else EmitMidi(status, d1, d2);
  }

  uint8_t drum_channel = static_cast<uint8_t>(RoundInt(params_[P_DRUM_OUT_CHANNEL]) - 1) & 0x0f;
  // Drum triggering from MIDI (channel 10 by default).
  if (channel == drum_channel) {
    if (type == 0x90 && d2) {
      for (uint8_t i = 0; i < 3; ++i) {
        if (d1 == RoundInt(params_[P_BD_NOTE + i])) {
          vc_.mutable_drum_synth()->Trigger(i, static_cast<uint8_t>(d2 << 1));
        }
      }
    } else if (type == 0xb0) {
      vc_.mutable_drum_synth()->SetParameterCc(d1, d2);
    }
  }
  // Drum pattern programming from MIDI (channel 16).
  if (channel == 15 && type == 0x90 && d2) {
    vc_.RemoteControlDrumSequencer(d1);
  }

  int in_channel = RoundInt(params_[P_MIDI_IN_CHANNEL]);
  bool accepted = in_channel == 0 ? (channel != drum_channel && channel != 15)
                                  : channel == in_channel - 1;
  if (!accepted) return;

  switch (type) {
    case 0x90:
      if (d2) vc_.NoteOn(d1, d2);
      else vc_.NoteOff(d1);
      break;
    case 0x80:
      vc_.NoteOff(d1);
      break;
    case 0xb0:
      HandleControlChange(d1, d2);
      break;
    case 0xe0:
      vc_.PitchBend(static_cast<uint16_t>(d1 | (d2 << 7)));
      break;
    case 0xd0:
      vc_.Aftertouch(d1);
      break;
    case 0xa0:
      vc_.Aftertouch(d2);
      break;
  }
}

// ---------------------------------------------------------------------------
// Commands and state

void AnuEngine::QueueCommand(int command, int a, int b) {
  while (cmd_lock_.test_and_set(std::memory_order_acquire)) {}
  int w = cmd_write_.load(std::memory_order_relaxed);
  int next = (w + 1) % kCmdQueueSize;
  if (next != cmd_read_.load(std::memory_order_acquire)) {
    cmd_queue_[w] = Cmd{ command, a, b };
    cmd_write_.store(next, std::memory_order_release);
  }
  cmd_lock_.clear(std::memory_order_release);
  if (command == CMD_MIDI_LEARN) learn_target_.store(a);
}

void AnuEngine::ProcessCommands() {
  int r = cmd_read_.load(std::memory_order_relaxed);
  while (r != cmd_write_.load(std::memory_order_acquire)) {
    const Cmd& c = cmd_queue_[r];
    SequencerSettings* s = vc_.mutable_sequencer_settings();
    switch (c.command) {
      case CMD_INSERT_REST: vc_.InsertRest(); break;
      case CMD_INSERT_TIE: vc_.InsertTie(); break;
      case CMD_TOGGLE_STEP:
        if (c.a >= 0 && c.a < 3 && c.b >= 0 && c.b < 16) {
          s->drums_pattern[c.a] ^= static_cast<uint16_t>(1 << c.b);
        }
        break;
      case CMD_CLEAR_PATTERN:
        if (c.a >= 0 && c.a < 3) s->drums_pattern[c.a] = 0;
        break;
      case CMD_MIDI_LEARN: break;  // handled through learn_target_
      case CMD_MIDI_FORGET:
        for (int i = 0; i < 128; ++i) {
          if (learn_map_[i] == c.a + 1) learn_map_[i] = 0;
        }
        break;
      case CMD_PANIC:
        vc_.AllSoundOff();
        for (uint8_t ch = 0; ch < 16; ++ch) EmitMidi(0xb0 | ch, 123, 0);
        break;
    }
    r = (r + 1) % kCmdQueueSize;
  }
  cmd_read_.store(r, std::memory_order_release);
}

static void ToHex(std::string& out, const uint8_t* data, size_t n) {
  static const char* digits = "0123456789abcdef";
  for (size_t i = 0; i < n; ++i) {
    out += digits[data[i] >> 4];
    out += digits[data[i] & 15];
  }
}

static bool FromHex(const std::string& s, uint8_t* data, size_t n) {
  if (s.size() < n * 2) return false;
  auto nib = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  for (size_t i = 0; i < n; ++i) {
    int a = nib(s[2 * i]), b = nib(s[2 * i + 1]);
    if (a < 0 || b < 0) return false;
    data[i] = static_cast<uint8_t>((a << 4) | b);
  }
  return true;
}

std::string AnuEngine::SerializeSequence() {
  Sequence copy;
  Lock();
  copy = snapshot_sequence_;
  Unlock();
  std::string out;
  ToHex(out, reinterpret_cast<const uint8_t*>(&copy), sizeof(copy));
  return out;
}

void AnuEngine::DeserializeSequence(const std::string& s) {
  Sequence seq;
  if (!FromHex(s, reinterpret_cast<uint8_t*>(&seq), sizeof(seq))) return;
  if (seq.num_notes > 128) seq.num_notes = 128;
  Lock();
  pending_sequence_data_ = seq;
  pending_sequence_ = true;
  snapshot_sequence_ = seq;
  Unlock();
  params_[P_OUT_SEQ_LENGTH] = seq.num_notes;
}

std::string AnuEngine::SerializePatterns() {
  uint16_t p[3];
  Lock();
  std::memcpy(p, snapshot_patterns_, sizeof(p));
  Unlock();
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%04x,%04x,%04x", p[0], p[1], p[2]);
  return buf;
}

void AnuEngine::DeserializePatterns(const std::string& s) {
  unsigned a = 0, b = 0, c = 0;
  if (std::sscanf(s.c_str(), "%x,%x,%x", &a, &b, &c) != 3) return;
  Lock();
  pending_patterns_data_[0] = static_cast<uint16_t>(a);
  pending_patterns_data_[1] = static_cast<uint16_t>(b);
  pending_patterns_data_[2] = static_cast<uint16_t>(c);
  std::memcpy(snapshot_patterns_, pending_patterns_data_, sizeof(snapshot_patterns_));
  pending_patterns_ = true;
  Unlock();
}

std::string AnuEngine::SerializeLearn() {
  uint16_t map[128];
  Lock();
  std::memcpy(map, snapshot_learn_, sizeof(map));
  Unlock();
  std::string out;
  for (int i = 0; i < 128; ++i) {
    if (map[i] && map[i] <= P_COUNT) {
      if (!out.empty()) out += ";";
      out += std::to_string(i) + "=" + kParams[map[i] - 1].symbol;
    }
  }
  return out;
}

void AnuEngine::DeserializeLearn(const std::string& s) {
  uint16_t map[128];
  std::memset(map, 0, sizeof(map));
  size_t pos = 0;
  while (pos < s.size()) {
    size_t end = s.find(';', pos);
    if (end == std::string::npos) end = s.size();
    std::string item = s.substr(pos, end - pos);
    size_t eq = item.find('=');
    if (eq != std::string::npos) {
      int cc = std::atoi(item.substr(0, eq).c_str());
      std::string sym = item.substr(eq + 1);
      for (uint32_t p = 0; p < P_COUNT; ++p) {
        if (sym == kParams[p].symbol && cc >= 0 && cc < 128) map[cc] = static_cast<uint16_t>(p + 1);
      }
    }
    pos = end + 1;
  }
  Lock();
  std::memcpy(snapshot_learn_, map, sizeof(map));
  std::memcpy(learn_map_, map, sizeof(map));
  Unlock();
}

// ---------------------------------------------------------------------------
// Processing

void AnuEngine::BeginBlock(uint32_t frames, bool host_valid, bool host_playing, double host_ppq,
                           double host_bpm) {
  (void)frames;
  num_out_events_ = 0;
  current_frame_ = 0;

  if (TryLock()) {
    if (pending_sequence_) {
      if (vc_.sequencer_running()) vc_.Stop();
      *vc_.mutable_sequence() = pending_sequence_data_;
      pending_sequence_ = false;
    }
    if (pending_patterns_) {
      std::memcpy(vc_.mutable_sequencer_settings()->drums_pattern, pending_patterns_data_,
                  sizeof(pending_patterns_data_));
      pending_patterns_ = false;
    }
    snapshot_sequence_ = vc_.sequence();
    std::memcpy(snapshot_patterns_, vc_.sequencer_settings().drums_pattern, sizeof(snapshot_patterns_));
    std::memcpy(snapshot_learn_, learn_map_, sizeof(snapshot_learn_));
    Unlock();
  }
  ProcessCommands();

  bool host_mode = params_[P_TEMPO] < 39.5f;
  if (host_mode && host_valid && host_bpm > 1.0) {
    int bpm = static_cast<int>(std::lround(host_bpm));
    if (bpm < 20) bpm = 20;
    if (bpm > 255) bpm = 255;
    vc_.UpdateClockTempo(static_cast<uint8_t>(bpm));
    double pulse = host_ppq * 24.0;
    host_pulse_inc_ = host_bpm / 60.0 * 24.0 / sample_rate_;
    if (host_playing && !host_playing_) {
      vc_.Start();
      last_pulse_ = static_cast<int64_t>(std::ceil(pulse - 1e-6)) - 1;
      vc_.AlignToPulse(last_pulse_ + 1);
    } else if (host_playing) {
      if (std::fabs(pulse - host_pulse_) > 1.0) {
        // Loop or relocation.
        last_pulse_ = static_cast<int64_t>(std::ceil(pulse - 1e-6)) - 1;
        vc_.AlignToPulse(last_pulse_ + 1);
      }
    }
    if (!host_playing && host_playing_ && vc_.sequencer_running()) {
      vc_.Stop();
    }
    host_pulse_ = pulse;
    host_playing_ = host_playing;
  } else {
    if (host_mode_ && !host_mode) vc_.TouchClock();
    host_playing_ = false;
  }
  host_mode_ = host_mode;
}

void AnuEngine::Tick() {
  if (!(host_mode_ && host_playing_)) vc_.TickInternalClock();
  drum_prev_ = drum_cur_;
  drum_cur_ = vc_.mutable_drum_synth()->RenderSample();
  if (++control_counter_ >= kControlDivider) {
    control_counter_ = 0;
    ControlStep();
  }
}

void AnuEngine::ControlStep() {
  if (!(host_mode_ && host_playing_)) {
    uint8_t n = vc_.CountClockEvents();
    while (n--) vc_.Clock();
  }
  Voice* voice = vc_.mutable_voice();
  voice->Refresh();
  const ControlVoltages& cv = voice->cv();
  analog_.SetControlVoltages(cv.vco_note, cv.dco_note, cv.pw, cv.vcf_offset, cv.vca);
  vc_.ClearClockPulse();
}

void AnuEngine::Render(float* out_l, float* out_r, uint32_t from, uint32_t to) {
  for (uint32_t f = from; f < to; ++f) {
    current_frame_ = f;
    if (host_mode_ && host_playing_) {
      int64_t ip = static_cast<int64_t>(std::floor(host_pulse_ + 1e-9));
      int guard = 0;
      while (ip > last_pulse_ && guard++ < 4) {
        ++last_pulse_;
        vc_.Clock();
      }
      if (ip > last_pulse_) last_pulse_ = ip;
      host_pulse_ += host_pulse_inc_;
    }
    for (int i = 0; i < 3; ++i) {
      if (drum_note_off_countdown_[i] >= 0) {
        if (drum_note_off_countdown_[i]-- == 0) {
          EmitMidi(0x80 | drum_channel_sent_[i], drum_note_sent_[i], 0);
          drum_note_off_countdown_[i] = -1;
        }
      }
    }
    tick_phase_ += tick_increment_;
    while (tick_phase_ >= 1.0) {
      tick_phase_ -= 1.0;
      Tick();
    }
    float frac = static_cast<float>(tick_phase_ / (tick_increment_ < 1.0 ? 1.0 : tick_increment_));
    if (frac > 1.0f) frac = 1.0f;
    float d = static_cast<float>(drum_prev_) + (static_cast<float>(drum_cur_) - drum_prev_) * frac;
    d = (d - 128.0f) / 128.0f;
    drum_lp_ += drum_lp_coef_ * (d - drum_lp_);
    float dy = drum_lp_ - drum_dc_x_ + drum_dc_coef_ * drum_dc_y_;
    drum_dc_x_ = drum_lp_;
    drum_dc_y_ = dy;
    drum_level_smooth_ += 0.001f * (params_[P_DRUM_LEVEL] - drum_level_smooth_);
    float synth = analog_.Process();
    float out = synth + dy * drum_level_smooth_ * 0.45f;
    out_l[f] = out;
    out_r[f] = out;
  }
}

void AnuEngine::UpdateOutputs() {
  const SequencerSettings& s = vc_.sequencer_settings();
  bool running = vc_.sequencer_running();
  uint32_t leds = 0;
  if (vc_.voice().gate()) leds |= LED_GATE;
  if (vc_.voice().lfo() > 127) leds |= LED_LFO;
  uint8_t step = static_cast<uint8_t>((vc_.drum_step() + 15) & 15);
  if (running && (step & 3) == 0) leds |= LED_CLOCK;
  if (running) leds |= LED_RUNNING;
  if (vc_.sequencer_recording()) leds |= LED_RECORDING;
  if (vc_.hold_state()) leds |= LED_HOLD;
  if (vc_.drum_synth().amp_level(0) > 48) leds |= LED_BD;
  if (vc_.drum_synth().amp_level(1) > 48) leds |= LED_SD;
  if (vc_.drum_synth().amp_level(2) > 48) leds |= LED_HH;
  if (s.drums_override) leds |= LED_OVERRIDE;
  leds |= static_cast<uint32_t>(vc_.remote_instrument()) << 12;
  params_[P_OUT_LEDS] = static_cast<float>(leds);
  params_[P_OUT_DRUM_STEP] = running ? static_cast<float>(step) : -1.0f;
  params_[P_OUT_PATTERN_BD] = s.drums_pattern[0];
  params_[P_OUT_PATTERN_SD] = s.drums_pattern[1];
  params_[P_OUT_PATTERN_HH] = s.drums_pattern[2];
  params_[P_OUT_SEQ_LENGTH] = vc_.sequence().num_notes;

  // Keep the transport "buttons" in sync with the engine state.
  ChangeParamFromEngine(P_RUN, running ? 1.0f : 0.0f);
  ChangeParamFromEngine(P_RECORD, vc_.sequencer_recording() ? 1.0f : 0.0f);
  ChangeParamFromEngine(P_HOLD, vc_.hold_state() ? 1.0f : 0.0f);
  ChangeParamFromEngine(P_DRUM_OVERRIDE, s.drums_override ? 1.0f : 0.0f);
}

void AnuEngine::EndBlock() {
  UpdateOutputs();
}

}  // namespace anu
