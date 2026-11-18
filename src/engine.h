// AnuEngine: glues the ported firmware core, the analog model and MIDI I/O.
#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include "analog.h"
#include "anu_core.h"
#include "params.h"

namespace anu {

struct OutMidi {
  uint32_t frame;
  uint8_t size;
  uint8_t data[3];
};

class ParamListener {
 public:
  virtual ~ParamListener() {}
  // Called from the audio thread when MIDI (CC, learn, remote programming...)
  // changed the value of a parameter.
  virtual void OnParameterChangedByEngine(uint32_t index, float value) = 0;
};

class AnuEngine : public MidiOutListener {
 public:
  static const int kMaxOutEvents = 1024;

  void Init(double sample_rate, ParamListener* listener);
  void SetSampleRate(double sample_rate);

  void SetParameter(uint32_t index, float value);
  float GetParameter(uint32_t index) const { return params_[index]; }

  // Called at the start of a block with host transport information.
  void BeginBlock(uint32_t frames, bool host_valid, bool host_playing, double host_ppq,
                  double host_bpm);
  // MIDI input at the current position inside the block.
  void HandleMidi(const uint8_t* data, uint32_t size, uint32_t frame);
  // Renders frames [from, to) of the current block.
  void Render(float* out_l, float* out_r, uint32_t from, uint32_t to);
  void EndBlock();

  // UI commands (thread-safe, queued).
  void QueueCommand(int command, int a = 0, int b = 0);
  enum Command {
    CMD_INSERT_REST = 1,
    CMD_INSERT_TIE,
    CMD_TOGGLE_STEP,      // a = instrument, b = step
    CMD_CLEAR_PATTERN,    // a = instrument
    CMD_MIDI_LEARN,       // a = parameter (-1 cancels)
    CMD_MIDI_FORGET,      // a = parameter
    CMD_PANIC,
  };

  // Generated MIDI output for the current block.
  int num_out_events() const { return num_out_events_; }
  const OutMidi& out_event(int i) const { return out_events_[i]; }

  // State serialisation (call from non-RT thread; they take a spinlock).
  std::string SerializeSequence();
  void DeserializeSequence(const std::string& s);
  std::string SerializePatterns();
  void DeserializePatterns(const std::string& s);
  std::string SerializeLearn();
  void DeserializeLearn(const std::string& s);
  int learning_param() const { return learn_target_.load(); }

 private:
  // MidiOutListener
  void OnInternalNoteOn(uint8_t note, uint8_t velocity) override;
  void OnInternalNoteOff(uint8_t note) override;
  void OnDrumNote(uint8_t instrument, uint8_t velocity) override;

  void ApplyParameter(uint32_t index);
  void ApplyAllParameters();
  void HandleControlChange(uint8_t cc, uint8_t value);
  bool SetParamFromCc(uint32_t index, uint8_t value8, int side);
  void ChangeParamFromEngine(uint32_t index, float value);
  void EmitMidi(uint8_t status, uint8_t d1, uint8_t d2);
  void EmitMidi2(uint8_t status, uint8_t d1);
  void Tick();
  void ControlStep();
  void ProcessCommands();
  void UpdateOutputs();
  void Lock() { while (lock_.test_and_set(std::memory_order_acquire)) {} }
  bool TryLock() { return !lock_.test_and_set(std::memory_order_acquire); }
  void Unlock() { lock_.clear(std::memory_order_release); }

  Random rng_;
  VoiceController vc_;
  AnalogVoice analog_;
  ParamListener* listener_ = nullptr;
  float params_[P_COUNT];
  double sample_rate_ = 48000.0;
  double tick_increment_ = 1.0;
  double tick_phase_ = 0.0;
  int control_counter_ = 0;
  uint8_t drum_prev_ = 128, drum_cur_ = 128;
  float drum_lp_ = 0.0f, drum_lp_coef_ = 0.5f;
  float drum_dc_x_ = 0.0f, drum_dc_y_ = 0.0f, drum_dc_coef_ = 0.999f;
  float drum_level_smooth_ = 0.8f;

  // Host sync.
  bool host_mode_ = false;
  bool host_playing_ = false;
  double host_pulse_ = 0.0;       // position in 24 ppqn pulses at current frame
  double host_pulse_inc_ = 0.0;   // pulses per frame
  int64_t last_pulse_ = -1;

  // MIDI out.
  OutMidi out_events_[kMaxOutEvents];
  int num_out_events_ = 0;
  uint32_t current_frame_ = 0;
  int32_t drum_note_off_countdown_[3] = { -1, -1, -1 };
  uint8_t drum_note_sent_[3] = { 0, 0, 0 };
  uint8_t drum_channel_sent_[3] = { 9, 9, 9 };
  uint8_t seq_channel_sent_ = 0;
  bool ui_run_ = false, ui_record_ = false, ui_hold_ = false;

  // MIDI learn: cc -> parameter (+1, 0 = none).
  uint16_t learn_map_[128];
  std::atomic<int> learn_target_{ -1 };

  // Command queue (UI -> DSP), single producer protected by the lock.
  struct Cmd { int command, a, b; };
  static const int kCmdQueueSize = 256;
  Cmd cmd_queue_[kCmdQueueSize];
  std::atomic<int> cmd_write_{ 0 };
  std::atomic<int> cmd_read_{ 0 };
  std::atomic_flag cmd_lock_ = ATOMIC_FLAG_INIT;

  // Pending state loads (applied by the audio thread).
  std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
  bool pending_sequence_ = false;
  Sequence pending_sequence_data_;
  bool pending_patterns_ = false;
  uint16_t pending_patterns_data_[3];
  // Snapshot for saving.
  Sequence snapshot_sequence_;
  uint16_t snapshot_patterns_[3];
  uint16_t snapshot_learn_[128];

  uint32_t led_state_ = 0;
  uint16_t led_clock_hold_ = 0;
};

}  // namespace anu
