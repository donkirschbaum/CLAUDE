// Anu - plugin wrapper (DPF) around the Anushri engine.
// GPLv3. Based on the Mutable Instruments Anushri firmware by Emilie Gillet.

#include "DistrhoPlugin.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "engine.h"
#include "params.h"

START_NAMESPACE_DISTRHO

using namespace anu;

enum StateIndex {
  kStateSequence,
  kStatePatterns,
  kStateMidiLearn,
  kStateCommand,
  kStateCount
};

class AnuPlugin : public Plugin, public ParamListener {
 public:
  AnuPlugin() : Plugin(P_COUNT, 0, kStateCount) {
    engine_.Init(getSampleRate(), this);
  }

 protected:
  const char* getLabel() const override { return "Anu"; }
  const char* getDescription() const override {
    return "Monosynth + generative drum machine. Emulation of the Mutable Instruments "
           "Anushri, built on its open-source firmware.";
  }
  const char* getMaker() const override { return "Anu Project"; }
  const char* getHomePage() const override { return "https://github.com/pichenettes/anushri"; }
  const char* getLicense() const override { return "GPL-3.0-or-later"; }
  uint32_t getVersion() const override { return d_version(1, 0, 0); }

  void initAudioPort(bool input, uint32_t index, AudioPort& port) override {
    port.groupId = kPortGroupStereo;
    Plugin::initAudioPort(input, index, port);
  }

  void initParameter(uint32_t index, Parameter& parameter) override {
    const ParamInfo& p = kParams[index];
    parameter.name = p.name;
    parameter.shortName = p.label;
    parameter.symbol = p.symbol;
    parameter.ranges.min = p.min;
    parameter.ranges.max = p.max;
    parameter.ranges.def = p.def;
    switch (p.kind) {
      case KIND_KNOB:
      case KIND_BIPOLAR:
        parameter.hints = kParameterIsAutomatable;
        break;
      case KIND_INT:
        parameter.hints = kParameterIsAutomatable | kParameterIsInteger;
        break;
      case KIND_ENUM:
      case KIND_BOOL: {
        parameter.hints = kParameterIsAutomatable | kParameterIsInteger;
        if (p.kind == KIND_BOOL) parameter.hints |= kParameterIsBoolean;
        const int count = static_cast<int>(p.max - p.min) + 1;
        ParameterEnumerationValue* values = new ParameterEnumerationValue[count];
        for (int i = 0; i < count; ++i) {
          values[i].value = p.min + i;
          values[i].label = p.enums[i];
        }
        parameter.enumValues.count = static_cast<uint8_t>(count);
        parameter.enumValues.restrictedMode = true;
        parameter.enumValues.values = values;
        break;
      }
      case KIND_OUTPUT:
        parameter.hints = kParameterIsOutput | kParameterIsInteger;
        break;
    }
    if (index == P_TEMPO) {
      parameter.description = "39 = follow host tempo and transport (EXT)";
    }
  }

  void initState(uint32_t index, State& state) override {
    switch (index) {
      case kStateSequence:
        state.key = "sequence";
        state.label = "Recorded sequence";
        state.hints = kStateIsHostWritable;
        state.defaultValue = "";
        break;
      case kStatePatterns:
        state.key = "patterns";
        state.label = "Drum override patterns";
        state.hints = kStateIsHostWritable;
        state.defaultValue = "0000,0000,0000";
        break;
      case kStateMidiLearn:
        state.key = "midilearn";
        state.label = "MIDI learn assignments";
        state.hints = kStateIsHostWritable;
        state.defaultValue = "";
        break;
      case kStateCommand:
        state.key = "cmd";
        state.label = "UI command";
        state.hints = kStateIsOnlyForDSP;
        state.defaultValue = "";
        break;
    }
  }

  String getState(const char* key) const override {
    AnuEngine& e = const_cast<AnuEngine&>(engine_);
    if (std::strcmp(key, "sequence") == 0) return String(e.SerializeSequence().c_str());
    if (std::strcmp(key, "patterns") == 0) return String(e.SerializePatterns().c_str());
    if (std::strcmp(key, "midilearn") == 0) return String(e.SerializeLearn().c_str());
    return String();
  }

  void setState(const char* key, const char* value) override {
    if (std::strcmp(key, "sequence") == 0) {
      if (value[0]) engine_.DeserializeSequence(value);
    } else if (std::strcmp(key, "patterns") == 0) {
      engine_.DeserializePatterns(value);
    } else if (std::strcmp(key, "midilearn") == 0) {
      engine_.DeserializeLearn(value);
    } else if (std::strcmp(key, "cmd") == 0) {
      int a = 0, b = 0;
      char name[32] = { 0 };
      if (std::sscanf(value, "%31s %d %d", name, &a, &b) >= 1) {
        if (!std::strcmp(name, "rest")) engine_.QueueCommand(AnuEngine::CMD_INSERT_REST);
        else if (!std::strcmp(name, "tie")) engine_.QueueCommand(AnuEngine::CMD_INSERT_TIE);
        else if (!std::strcmp(name, "toggle")) engine_.QueueCommand(AnuEngine::CMD_TOGGLE_STEP, a, b);
        else if (!std::strcmp(name, "clear")) engine_.QueueCommand(AnuEngine::CMD_CLEAR_PATTERN, a);
        else if (!std::strcmp(name, "learn")) engine_.QueueCommand(AnuEngine::CMD_MIDI_LEARN, a);
        else if (!std::strcmp(name, "forget")) engine_.QueueCommand(AnuEngine::CMD_MIDI_FORGET, a);
        else if (!std::strcmp(name, "panic")) engine_.QueueCommand(AnuEngine::CMD_PANIC);
      }
    }
  }

  float getParameterValue(uint32_t index) const override { return engine_.GetParameter(index); }
  void setParameterValue(uint32_t index, float value) override { engine_.SetParameter(index, value); }

  void sampleRateChanged(double newSampleRate) override { engine_.SetSampleRate(newSampleRate); }

  void OnParameterChangedByEngine(uint32_t index, float value) override {
    requestParameterValueChange(index, value);
  }

  void run(const float**, float** outputs, uint32_t frames, const MidiEvent* midiEvents,
           uint32_t midiEventCount) override {
    const TimePosition& tp = getTimePosition();
    bool valid = tp.bbt.valid;
    double ppq = 0.0, bpm = 120.0;
    if (valid) {
      const double beat_type = tp.bbt.beatType > 0.0f ? tp.bbt.beatType : 4.0;
      const double beats = (tp.bbt.bar - 1) * static_cast<double>(tp.bbt.beatsPerBar) +
                           (tp.bbt.beat - 1) +
                           (tp.bbt.ticksPerBeat > 0.0 ? tp.bbt.tick / tp.bbt.ticksPerBeat : 0.0);
      ppq = beats * 4.0 / beat_type;
      bpm = tp.bbt.beatsPerMinute;
    }
    engine_.BeginBlock(frames, valid, tp.playing, ppq, bpm);

    float* out_l = outputs[0];
    float* out_r = outputs[1];
    uint32_t pos = 0;
    for (uint32_t i = 0; i < midiEventCount; ++i) {
      const MidiEvent& ev = midiEvents[i];
      uint32_t frame = ev.frame < frames ? ev.frame : frames;
      if (frame > pos) {
        engine_.Render(out_l, out_r, pos, frame);
        pos = frame;
      }
      const uint8_t* data = ev.size > MidiEvent::kDataSize ? ev.dataExt : ev.data;
      engine_.HandleMidi(data, ev.size, frame < frames ? frame : (frames ? frames - 1 : 0));
    }
    if (pos < frames) engine_.Render(out_l, out_r, pos, frames);
    engine_.EndBlock();

    for (int i = 0; i < engine_.num_out_events(); ++i) {
      const OutMidi& o = engine_.out_event(i);
      MidiEvent me;
      me.frame = frames ? (o.frame < frames ? o.frame : frames - 1) : 0;
      me.size = o.size;
      me.data[0] = o.data[0];
      me.data[1] = o.data[1];
      me.data[2] = o.data[2];
      me.data[3] = 0;
      me.dataExt = nullptr;
      if (!writeMidiEvent(me)) break;
    }
  }

 private:
  AnuEngine engine_;

  DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnuPlugin)
};

Plugin* createPlugin() { return new AnuPlugin(); }

END_NAMESPACE_DISTRHO
