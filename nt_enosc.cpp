
#include "enosc_plugin_stubs.h"

#include <atomic>
#include <cstring> // For std::memset
#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include <math.h>
#include <new>

#include "./enosc/lib/easiglib/bitfield.hh"
#include "./enosc/lib/easiglib/buffer.hh"
#include "./enosc/lib/easiglib/dsp.hh"
#include "./enosc/lib/easiglib/filter.hh"
#include "./enosc/lib/easiglib/math.hh"
#include "./enosc/lib/easiglib/numtypes.hh"
#include "./enosc/lib/easiglib/signal.hh"
#include "./enosc/lib/easiglib/util.hh"
#include "./enosc/src/distortion.hh"
#include "./enosc/src/oscillator.hh"
#include "./enosc/src/parameters.hh"
#include "./enosc/src/polyptic_oscillator.hh"
#include "./enosc/src/quantizer.hh"
#include <algorithm>

// A simple class for parameter smoothing.
class Smoother {
public:
  Smoother() : current_(0.0f), target_(0.0f), alpha_(0.0005f) {}

  void set_target(float target) { target_ = target; }

  // Set target without interpolation
  void set_hard(float value) {
    target_ = value;
    current_ = value;
  }

  float next() {
    current_ += alpha_ * (target_ - current_);
    return current_;
  }

  float current() const { return current_; }

private:
  float current_;
  float target_;
  float alpha_;
};

// Plugin parameters mapping to EnOSC engine Parameters
enum {
  kParamPitchCV,
  kParamRootCV,

  kParamOutputA,
  kParamOutputAMode,
  kParamOutputB,
  kParamOutputBMode,

  kParamBalance,
  kParamRoot,
  kParamPitch,
  kParamSpread,
  kParamDetune,

  kParamModMode,
  kParamModValue,

  kParamScaleMode,
  kParamScaleValue,

  kParamTwistMode,
  kParamTwistValue,

  kParamWarpMode,
  kParamWarpValue,

  kParamNumOsc,
  kParamStereoMode,
  kParamFreezeMode,
  kParamFreeze,
#ifdef LEARN_ENABLED
  kParamLearn,
  kParamCrossfade,

  kParamNewNote,
  kParamFineTune,
  kParamAddNote,
  kParamRemoveLastNote,
  kParamResetScale,
  kParamManualLearn,
#endif
  kNumParams
};

// Enumeration strings for mode parameters
static const char *const enumMod[] = {"One", "Two", "Three"};
static const char *const enumScale[] = {"12-TET", "Octave", "Free"};
static const char *const enumTwist[] = {"Feedback", "Pulsar", "Crush"};
static const char *const enumWarp[] = {"Fold", "Cheby", "Segment"};
static const char *const enumSplit[] = {"Alternate", "Lo/Hi", "Lowest/Rest"};
static const char *const enumFreeze[] = {"Off", "On"};
static const char *const enumLearn[] = {"Off", "On"};
static const char *const enumAction[] = {"Off", "On"};

const int kNumBusses = 28;

// Parameter definitions
// clang-format off
static const _NT_parameter parameters[] = {
    {.name = "Pitch CV Input", .min = 0, .max = 28, .def = 1, .unit = (uint8_t)kNT_unitCvInput, .scaling = 0, .enumStrings = NULL},
    {.name = "Root CV Input", .min = 0, .max = 28, .def = 2, .unit = (uint8_t)kNT_unitCvInput, .scaling = 0, .enumStrings = NULL},
    {.name = "Output A", .min = 1, .max = 28, .def = 13, .unit = (uint8_t)kNT_unitAudioOutput, .scaling = 0, .enumStrings = NULL},
    {.name = "Output B", .min = 1, .max = 28, .def = 14, .unit = (uint8_t)kNT_unitAudioOutput, .scaling = 0, .enumStrings = NULL},
    {.name = "Balance", .min = -100, .max = 100, .def = 100, .unit = (uint8_t)kNT_unitPercent, .scaling = 0, .enumStrings = NULL},
    {.name = "Root Note", .min = 0, .max = 21, .def = 12, .unit = (uint8_t)kNT_unitMIDINote, .scaling = 0, .enumStrings = NULL},
    {.name = "Pitch", .min = 0, .max = 127, .def = 0, .unit = (uint8_t)kNT_unitMIDINote, .scaling = 0, .enumStrings = NULL},
    {.name = "Spread", .min = 0,  .max = 12, .def = 4, .unit = (uint8_t)kNT_unitSemitones, .scaling = 0, .enumStrings = NULL},
    
    //detune should always be positive
    {.name = "Detune", .min = 0,  .max = 100, .def = 0, .unit = (uint8_t)kNT_unitPercent, .scaling = 0, .enumStrings = NULL},

    {.name = "Mod mode", .min = 0, .max = 2, .def = 0, .unit = (uint8_t)kNT_unitEnum, .scaling = 0, .enumStrings = enumMod},
    {.name = "Cross FM", .min = 0, .max = 100, .def = 0, .unit = (uint8_t)kNT_unitPercent, .scaling = 0, .enumStrings = NULL},

    {.name = "Scale Mode", .min = 0, .max = 2, .def = 0, .unit = (uint8_t)kNT_unitEnum, .scaling = 0, .enumStrings = enumScale},
    {.name = "Scale Preset", .min = 0, .max = 9, .def = 0, .unit = (uint8_t)kNT_unitNone, .scaling = 0, .enumStrings = NULL},

    {.name = "Twist mode", .min = 0, .max = 2, .def = 0, .unit = (uint8_t)kNT_unitEnum, .scaling = 0, .enumStrings = enumTwist},
    {.name = "Twist", .min = 0, .max = 100, .def = 0, .unit = (uint8_t)kNT_unitPercent, .scaling = 0, .enumStrings = NULL},

    {.name = "Warp mode", .min = 0, .max = 2, .def = 0, .unit = (uint8_t)kNT_unitEnum, .scaling = 0, .enumStrings = enumWarp},
    {.name = "Warp", .min = 0, .max = 100, .def = 0, .unit = (uint8_t)kNT_unitPercent, .scaling = 0, .enumStrings = NULL},

    {.name = "Num Osc", .min = 1, .max = 16, .def = 16, .unit = (uint8_t)kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Stereo mode", .min = 0, .max = 2, .def = 0, .unit = (uint8_t)kNT_unitEnum, .scaling = 0, .enumStrings = enumSplit},
    {.name = "Freeze mode", .min = 0, .max = 2, .def = 0, .unit = (uint8_t)kNT_unitEnum, .scaling = 0, .enumStrings = enumSplit},
    {.name = "Freeze", .min = 0, .max = 1, .def = 0, .unit = (uint8_t)kNT_unitEnum, .scaling = 0, .enumStrings = enumFreeze},
#ifdef LEARN_ENABLED
    {.name = "Learn", .min = 0, .max = 1, .def = 0, .unit = (uint8_t)kNT_unitEnum, .scaling = 0, .enumStrings = enumLearn},
    {.name = "Crossfade", .min = 0, .max = 100, .def = 12, .unit = (uint8_t)kNT_unitPercent, .scaling = 0, .enumStrings = NULL}, // 12% approx 0.125 internal
    {.name = "New Note MIDI", .min = 0, .max = 127, .def = 60, .unit = (uint8_t)kNT_unitMIDINote, .scaling = 0, .enumStrings = NULL},
    {.name = "Fine Tune", .min = -100, .max = 100, .def = 0, .unit = (uint8_t)kNT_unitSemitones, .scaling = (uint8_t)kNT_scaling100, .enumStrings = NULL},
    {.name = "Add Note", .min = 0, .max = 1, .def = 0, .unit = (uint8_t)kNT_unitEnum, .scaling = 0, .enumStrings = enumAction},
    {.name = "Remove Last Note", .min = 0, .max = 1, .def = 0, .unit = (uint8_t)kNT_unitEnum, .scaling = 0, .enumStrings = enumAction},
    {.name = "Reset Scale", .min = 0, .max = 1, .def = 0, .unit = (uint8_t)kNT_unitEnum, .scaling = 0, .enumStrings = enumAction},
    {.name = "Manual Learn", .min = 0, .max = 1, .def = 0, .unit = (uint8_t)kNT_unitEnum, .scaling = 0, .enumStrings = enumLearn},
#endif
};
// clang-format on

// Forward declaration for parameterChanged
void parameterChanged(_NT_algorithm *self, int p);

// DTC struct holds algorithm state and oscillator instance
struct _ntEnosc_DTC {
  Parameters params;
  PolypticOscillator<kBlockSize> osc;
  Scale *current_scale;
  std::atomic<int> next_num_osc;
  Buffer<Frame, kBlockSize> blk;

  // Smoothers
  Smoother s_balance;
  Smoother s_root;
  Smoother s_pitch;
  Smoother s_spread;
  Smoother s_detune;
  Smoother s_mod_value;
  Smoother s_twist_value;
  Smoother s_warp_value;
#ifdef LEARN_ENABLED
  Smoother s_crossfade;
  Smoother s_fine_tune;
  Smoother s_new_note;
#endif

  int16_t prev_kParamLearn_val = 0;
  float dtc_manual_learn_offset = 0;

  _ntEnosc_DTC(_NT_algorithm *self) : osc(params) {}
};

struct _ntEnosc_Alg : public _NT_algorithm {
  _ntEnosc_Alg(_ntEnosc_DTC *d) : dtc(d) {}
  _ntEnosc_DTC *dtc;
};



void calculateStaticRequirements(_NT_staticRequirements &req) {
  req.dram = 0;
}

void initialise(_NT_staticMemoryPtrs &ptrs, const _NT_staticRequirements &req) {}

void calculateRequirements(_NT_algorithmRequirements &req, const int32_t *) {
  req.numParameters = kNumParams;
  req.sram = sizeof(_ntEnosc_Alg);
  req.dtc = sizeof(_ntEnosc_DTC);
  req.itc = 0;
}

_NT_algorithm *construct(const _NT_algorithmMemoryPtrs &ptrs,
                         const _NT_algorithmRequirements &req,
                         const int32_t *) {
  auto *alg = new (ptrs.sram) _ntEnosc_Alg(nullptr);
  alg->parameters = parameters;
  alg->parameterPages = nullptr;
  auto *d = new (ptrs.dtc) _ntEnosc_DTC(alg);
  alg->dtc = d;

  // Initialize smoothers with default values from parameters array
  d->s_balance.set_hard(parameters[kParamBalance].def);
  d->s_root.set_hard(parameters[kParamRoot].def);
  d->s_pitch.set_hard(parameters[kParamPitch].def);
  d->s_spread.set_hard(parameters[kParamSpread].def);
  d->s_detune.set_hard(parameters[kParamDetune].def);
  d->s_mod_value.set_hard(parameters[kParamModValue].def);
  d->s_twist_value.set_hard(parameters[kParamTwistValue].def);
  d->s_warp_value.set_hard(parameters[kParamWarpValue].def);
#ifdef LEARN_ENABLED
  d->s_crossfade.set_hard(parameters[kParamCrossfade].def);
  d->s_fine_tune.set_hard(parameters[kParamFineTune].def);
  d->s_new_note.set_hard(parameters[kParamNewNote].def);
#endif

  return alg;
}

void parameterChanged(_NT_algorithm *self, int p) {
  _ntEnosc_Alg *a = (_ntEnosc_Alg *)self;
  auto &params = a->dtc->params;
  int16_t val = self->v[p];

  switch (p) {
#ifdef LEARN_ENABLED
  case kParamLearn: {
    int16_t prev_learn_val = a->dtc->prev_kParamLearn_val;
    if (val == 1 && prev_learn_val == 0) {
      a->dtc->osc.enable_learn();
      a->dtc->osc.enable_pre_listen();
      a->dtc->dtc_manual_learn_offset =
          a->dtc->s_pitch.current() - a->dtc->s_root.current();
    } else if (val == 0 && prev_learn_val == 1) {
      a->dtc->osc.disable_learn();
    }
    a->dtc->prev_kParamLearn_val = val;
    break;
  }
  case kParamManualLearn: {
    if (bool(val)) {
      a->dtc->osc.enable_pre_listen();
      a->dtc->osc.enable_follow_new_note();
      params.new_note = f(a->dtc->s_pitch.current());
    } else {
      a->dtc->osc.disable_follow_new_note();
    }
    break;
  }
#endif
  case kParamFreeze: {
    a->dtc->osc.set_freeze(bool(val));
    break;
  }
  case kParamBalance: a->dtc->s_balance.set_target(val); break;
  case kParamRoot: a->dtc->s_root.set_target(val); break;
  case kParamPitch: a->dtc->s_pitch.set_target(val); break;
  case kParamSpread: a->dtc->s_spread.set_target(val); break;
  case kParamDetune: a->dtc->s_detune.set_target(val); break;
  case kParamModValue: a->dtc->s_mod_value.set_target(val); break;
  case kParamTwistValue: a->dtc->s_twist_value.set_target(val); break;
  case kParamWarpValue: a->dtc->s_warp_value.set_target(val); break;
#ifdef LEARN_ENABLED
  case kParamCrossfade: a->dtc->s_crossfade.set_target(val); break;
  case kParamFineTune: a->dtc->s_fine_tune.set_target(val); break;
  case kParamNewNote: a->dtc->s_new_note.set_target(val); break;
#endif
  case kParamModMode: params.modulation.mode = ModulationMode(val); break;
  case kParamScaleMode: params.scale.mode = ScaleMode(val); break;
  case kParamScaleValue: params.scale.value = val; break;
  case kParamTwistMode: params.twist.mode = TwistMode(val); break;
  case kParamWarpMode: params.warp.mode = WarpMode(val); break;
  case kParamNumOsc: params.alt.numOsc = val; break;
  case kParamStereoMode: params.alt.stereo_mode = static_cast<SplitMode>(val); break;
  case kParamFreezeMode: params.alt.freeze_mode = static_cast<SplitMode>(val); break;
#ifdef LEARN_ENABLED
  case kParamAddNote:
    if (bool(val)) {
      float note_to_add_float = params.new_note.repr() +
                                a->dtc->dtc_manual_learn_offset +
                                params.fine_tune.repr();
      float clamped_note_float =
          std::clamp(note_to_add_float, 0.0f, 127.0f);
      a->dtc->osc.new_note(f(clamped_note_float));
    }
    break;
  case kParamRemoveLastNote:
    if (bool(val)) { a->dtc->osc.remove_last_note(); }
    break;
  case kParamResetScale:
    if (bool(val)) { a->dtc->osc.reset_current_scale(); }
    break;
#endif
  default:
    break;
  }
}

void step(_NT_algorithm *self, float *busFrames, int numFramesBy4) {
  auto *alg = (_ntEnosc_Alg *)self;
  auto *dtc = alg->dtc;

  const int numFrames = numFramesBy4 * 4;

  int chanA = int(self->v[kParamOutputA]) - 1;
  int chanB = int(self->v[kParamOutputB]) - 1;
  constexpr int MAX_CHAN = 27;
  chanA = std::clamp(chanA, 0, MAX_CHAN);
  chanB = std::clamp(chanB, 0, MAX_CHAN);

  int pitch_cv_bus_idx = int(self->v[kParamPitchCV]) - 1;
  int root_cv_bus_idx = int(self->v[kParamRootCV]) - 1;
  pitch_cv_bus_idx = std::clamp(pitch_cv_bus_idx, 0, MAX_CHAN);
  root_cv_bus_idx = std::clamp(root_cv_bus_idx, 0, MAX_CHAN);

  float *outA = busFrames + chanA * numFrames;
  float *outB = busFrames + chanB * numFrames;

  bool replaceA = (self->v[kParamOutputAMode] != 0);
  bool replaceB = (self->v[kParamOutputBMode] != 0);

  constexpr int BS = kBlockSize;

  for (int frame = 0; frame < numFrames; frame += BS) {
    float smoothed_balance = dtc->s_balance.next();
    f balance = f(smoothed_balance / 100.f);
    balance *= balance * balance;
    balance *= 4.0_f;
    dtc->params.balance = Math::fast_exp2(balance);

    float smoothed_spread = dtc->s_spread.next();
    f spread_val = f(smoothed_spread);
    spread_val *= f(10.0f / 16.0f);
    dtc->params.spread = spread_val;

    float smoothed_detune = dtc->s_detune.next();
    f detune_val = f(smoothed_detune / 100.f);
    detune_val = (detune_val * detune_val) * (detune_val * detune_val);
    detune_val *= f(10.0f / 16.0f);
    dtc->params.detune = detune_val;

    dtc->params.modulation.value = f(dtc->s_mod_value.next() / 100.f);
    dtc->params.twist.value = f(dtc->s_twist_value.next() / 100.f);
    dtc->params.warp.value = f(dtc->s_warp_value.next() / 100.f);

#ifdef LEARN_ENABLED
    dtc->params.alt.crossfade_factor = f(dtc->s_crossfade.next() / 100.f);
    dtc->params.fine_tune = f(dtc->s_fine_tune.next() / 100.f);
    dtc->params.new_note = f(dtc->s_new_note.next());
#endif

    f current_pitch_cv_value =
        f(busFrames[pitch_cv_bus_idx * numFrames + frame] * 12.0f);
    f current_root_cv_value =
        f(busFrames[root_cv_bus_idx * numFrames + frame] * 12.0f);

    float pitch_pot_base = dtc->s_pitch.next();
    float root_panel_midi_note = dtc->s_root.next();

    const float pitch_range = 72.0f;
    float pitch_offset = (pitch_pot_base / 127.f) * pitch_range;
    pitch_offset -= pitch_range / 2.0f;

    dtc->params.pitch =
        f(60.0f + pitch_offset) + current_pitch_cv_value + dtc->params.fine_tune;

    const float root_range = 21.0f;
    float root_offset = (root_panel_midi_note / 21.f) * root_range;

    dtc->params.root = f(root_offset) + current_root_cv_value;

    dtc->osc.Process(dtc->blk);

    int valid = std::min(BS, numFrames - frame);
    for (int i = 0; i < valid; ++i) {
      float sampleL = Float(dtc->blk[i].l).repr() * 5.0f;
      float sampleR = Float(dtc->blk[i].r).repr() * 5.0f;

      if (replaceA) {
        outA[frame + i] = sampleL;
      } else {
        outA[frame + i] += sampleL;
      }
      if (replaceB) {
        outB[frame + i] = sampleR;
      } else {
        outB[frame + i] += sampleR;
      }
    }
  }
}

static const _NT_factory factory = {
    .guid = NT_MULTICHAR('T', 'h', 'E', 'O'),
    .name = "EnsembleOsc",
    .description = "4ms Ensemble Oscillator port",
    .calculateStaticRequirements = calculateStaticRequirements,
    .initialise = initialise,
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .parameterChanged = parameterChanged,
    .step = step,
    .draw = nullptr,
    .midiRealtime = nullptr,
    .midiMessage = nullptr,
    .tags = 0,
    .hasCustomUi = nullptr,
    .customUi = nullptr,
    .setupUi = nullptr,
    .serialise = nullptr,
    .deserialise = nullptr,
};

extern "C" uintptr_t pluginEntry(_NT_selector selector, uint32_t index) {
  switch (selector) {
  case kNT_selector_version:
    return kNT_apiVersionCurrent;
  case kNT_selector_numFactories:
    return 1;
  case kNT_selector_factoryInfo:
    return (index == 0) ? (uintptr_t)&factory : 0;
  }
  return 0;
}