// #define DYNAMIC_DATA_LOCAL_HH
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
  kParamLearn,
  kParamCrossfade,

  kParamNewNote,
  kParamFineTune,
  kParamAddNote,
  kParamRemoveLastNote,
  kParamResetScale,
  kParamManualLearn,

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
    NT_PARAMETER_CV_INPUT("Pitch CV Input", 0, 1)
    NT_PARAMETER_CV_INPUT("Root CV Input", 0, 2)
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Output A", 1, 13)
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Output B", 1, 14)
    {.name = "Balance", .min = -100, .max = 100, .def = 50, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL},
    {.name = "Root", .min = -24, .max = 24, .def = 0, .unit = kNT_unitSemitones, .scaling = 0, .enumStrings = NULL},
    {.name = "Pitch", .min = 0, .max = 127, .def = 69, .unit = kNT_unitMIDINote, .scaling = 0, .enumStrings = NULL},
    {.name = "Spread", .min = 0,  .max = 100, .def = 0, .unit = kNT_unitPercent,   .scaling = 0, .enumStrings = NULL},
    {.name = "Detune", .min = 0,  .max = 100, .def = 0, .unit = kNT_unitPercent,   .scaling = 0, .enumStrings = NULL},

    {.name = "Mod mode", .min = 0, .max = 2, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = enumMod},
    {.name = "Cross FM", .min = 0, .max = 100, .def = 0, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL},

    {.name = "Scale Mode", .min = 0, .max = 2, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = enumScale},
    {.name = "Scale Preset", .min = 0, .max = 9, .def = 0, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},

    {.name = "Twist mode", .min = 0, .max = 2, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = enumTwist},
    {.name = "Twist", .min = 0, .max = 100, .def = 0, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL},

    {.name = "Warp mode", .min = 0, .max = 2, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = enumWarp},
    {.name = "Warp", .min = 0, .max = 100, .def = 0, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL},

    {.name = "Num Osc", .min = 1, .max = 16, .def = 16, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Stereo mode", .min = 0, .max = 2, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = enumSplit},
    {.name = "Freeze mode", .min = 0, .max = 2, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = enumSplit},
    {.name = "Freeze", .min = 0, .max = 1, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = enumFreeze},
    {.name = "Learn", .min = 0, .max = 1, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = enumLearn},
    {.name = "Crossfade", .min = 0, .max = 100, .def = 12, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL},

    {.name = "New note", .min = 0, .max = 150, .def = 0, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
    {.name = "Fine Tune", .min = -100, .max = 100, .def = 0, .unit = kNT_unitCents, .scaling = 0, .enumStrings = NULL},
    {.name = "Add Note", .min = 0, .max = 1, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = enumAction},
    {.name = "Remove Last Note", .min = 0, .max = 1, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = enumAction},
    {.name = "Reset Scale", .min = 0, .max = 1, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = enumAction},
    {.name = "Manual Learn", .min = 0, .max = 1, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = enumLearn},
};
// clang-format off

// Forward declaration for parameterChanged
void parameterChanged(_NT_algorithm *self, int p);

// DTC struct holds algorithm state and oscillator instance
struct _ntEnosc_DTC
{
  Parameters params;
  PolypticOscillator<kBlockSize> osc;
  Scale *current_scale;
  std::atomic<int> next_num_osc;
  Buffer<Frame, kBlockSize> blk;
  float pitch_pot_base = 69.f; // Default MIDI note
  float root_pot_base = 0.f;   // Default semitone offset

  float prev_kParamLearn_val = 0.f;
  float dtc_manual_learn_offset = 0.f; // New member to store the offset

  _ntEnosc_DTC(_NT_algorithm *self) : osc(params) {
    // Initialize 'alt' parameters to their true defaults, as Parameters::clear() uses memset
    params.alt.numOsc = 16; // Corrected: Default from Parameters::Alt is 16
    params.alt.crossfade_factor = 0.5_f; // Default from Parameters::Alt
  }
};

struct _ntEnosc_Alg : public _NT_algorithm
{
  _ntEnosc_Alg(_ntEnosc_DTC *d) : dtc(d) {}
  _ntEnosc_DTC *dtc;
};

// Define the global pointer here now that it will point to DRAM
DynamicData *g_DynamicData;

void calculateStaticRequirements(_NT_staticRequirements &req)
{
  req.dram = 0;
}

void initialise(_NT_staticMemoryPtrs &ptrs, const _NT_staticRequirements &req)
{
}

void calculateRequirements(_NT_algorithmRequirements &req, const int32_t *)
{
  req.numParameters = kNumParams;
  req.sram = sizeof(_ntEnosc_Alg);
  req.dtc = sizeof(_ntEnosc_DTC);
  req.itc = 0;
}

_NT_algorithm *construct(const _NT_algorithmMemoryPtrs &ptrs,
                         const _NT_algorithmRequirements &req,
                         const int32_t *)
{
  auto *alg = new (ptrs.sram) _ntEnosc_Alg(nullptr);
  alg->parameters = parameters;
  alg->parameterPages = nullptr;
  auto *d = new (ptrs.dtc) _ntEnosc_DTC(alg);
  alg->dtc = d;

  return alg;
}

void parameterChanged(_NT_algorithm *self, int p)
{
  _ntEnosc_Alg *a = (_ntEnosc_Alg *)self;
  auto &params = a->dtc->params;
  float v = self->v[p];
  float prev_learn_val = a->dtc->prev_kParamLearn_val;

  // All parameters are directly handled, no UI modes needed
  switch (p) {
  case kParamLearn: {
    if (v == 1.f && prev_learn_val == 0.f) { // Learn parameter turned on
      a->dtc->osc.enable_learn();
      a->dtc->osc.enable_pre_listen();
      a->dtc->dtc_manual_learn_offset = a->dtc->osc.lowest_pitch().repr() - params.root.repr();
    } else if (v == 0.f && prev_learn_val == 1.f) { // Learn parameter turned off
      a->dtc->osc.disable_learn();
    }
    a->dtc->prev_kParamLearn_val = v;
    break;
  }
  case kParamManualLearn: {
    if (bool(v)) { // Manual Learn turned on
      a->dtc->osc.enable_pre_listen();
      a->dtc->osc.enable_follow_new_note();
    } else { // Manual Learn turned off
      a->dtc->osc.disable_follow_new_note();
    }
    break;
  }
  case kParamFreeze: {
    a->dtc->osc.set_freeze(bool(v));
    break;
  }
  case kParamBalance: {
    params.balance = f(v / 100.f) * 2_f - 1_f;
    break;
  }
  case kParamRoot:
    params.root = f(v);
    a->dtc->root_pot_base = v;
    break;
  case kParamPitch:
    params.pitch = f(v);
    a->dtc->pitch_pot_base = v;
    break;
  case kParamSpread: {
    f sp = f(v / 100.f);
    sp *= 10_f / f(16);
    params.spread = sp * 12_f;
    break;
  }
  case kParamDetune: {
    f dt = f(v / 100.f);
    dt = (dt * dt) * (dt * dt);
    dt *= 10_f / f(16);
    params.detune = dt;
    break;
  }
  case kParamModMode:
    params.modulation.mode = ModulationMode(int(v));
    break;
  case kParamModValue: {
    f m = f(v / 100.f); // 0 … 1 from the panel
    m *= m; // square curve
    m *= 4_f / f(params.alt.numOsc);   // Normalise for polyphony
    switch (params.modulation.mode) {
      case ONE:    m *= 2.5_f; break;
      case TWO:    m *= 1.0_f; break;
      case THREE:  m *= 1.8_f; break;
    }
    if (m > 4.999_f) m = 4.999_f; // Safety
    params.modulation.value = m;
    break;
  }
  case kParamScaleMode: {
    params.scale.mode = ScaleMode(int(v));
    break;
  }
  case kParamScaleValue: {
    params.scale.value = int(v + 0.5f);
    break;
  }
  case kParamTwistMode:
    params.twist.mode = TwistMode(int(v));
    break;
  case kParamTwistValue:
  {
    f tw = f(v / 100.f);
    if (params.twist.mode == FEEDBACK) {
      tw *= tw * 0.7_f;
    } else if (params.twist.mode == PULSAR) {
      tw = Math::fast_exp2(tw * 6_f);
      if (tw < 1_f) tw = 1_f;
    } else /* CRUSH */ {
      tw = tw * tw * 0.5_f;
    }
    params.twist.value = tw;
    break;
  }
  case kParamWarpMode:
    params.warp.mode = WarpMode(int(v));
    break;
  case kParamWarpValue:
  {
    f wr = f(v / 100.f);
    if (params.warp.mode == FOLD) {
      wr = wr * wr;
      wr = wr * 0.9_f + 0.004_f;
      if (wr > 0.999_f) wr = 0.999_f;
    } else /* CHEBY or SEGMENT */ {
      if (wr >= 0.999_f) wr = 0.999_f;
    }
    params.warp.value = wr;
    break;
  }
  case kParamNumOsc: {
    int numOsc = roundf((v / 100.f) * (16 - 1) + 1);
    params.alt.numOsc = std::clamp(numOsc, 1, 16);
    break;
  }
  case kParamStereoMode: {
    int stereo_mode = roundf((v / 100.f) * 2);
    params.alt.stereo_mode = static_cast<SplitMode>(std::clamp(stereo_mode, 0, 2));
    break;
  }
  case kParamFreezeMode: {
    int freeze_mode = roundf((v / 100.f) * 2);
    params.alt.freeze_mode = static_cast<SplitMode>(std::clamp(freeze_mode, 0, 2));
    break;
  }
  case kParamCrossfade: {
    f crossfade_factor = f(v / 100.f) * 0.5_f; // Maps 0-100% to 0-0.5
    params.alt.crossfade_factor = crossfade_factor;
    break;
  }
  case kParamNewNote:
    params.new_note = f(v);
    break;
  case kParamFineTune:
    params.fine_tune = f(v / 100.f);
    break;
  case kParamAddNote:
    if (bool(v)) {
      a->dtc->osc.new_note(params.new_note + f(a->dtc->dtc_manual_learn_offset) + params.fine_tune);
    }
    break;
  case kParamRemoveLastNote:
    if (bool(v)) {
      a->dtc->osc.remove_last_note();
    }
    break;
  case kParamResetScale:
    if (bool(v)) {
      a->dtc->osc.reset_current_scale();
    }
    break;
  default:
    break;
  }
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4)
{
    auto* alg = (_ntEnosc_Alg*)self;
    auto* dtc = alg->dtc;

    const int numFrames = numFramesBy4 * 4;

    // UI is 1-based, clamp to [0 .. MAX_CHAN]
    int chanA = int(self->v[kParamOutputA]) - 1;
    int chanB = int(self->v[kParamOutputB]) - 1;
    constexpr int MAX_CHAN = 27;  // for a 28-channel bus
    chanA = std::clamp(chanA, 0, MAX_CHAN);
    chanB = std::clamp(chanB, 0, MAX_CHAN);

    // Get bus indices for CV inputs
    int pitch_cv_bus_idx = int(self->v[kParamPitchCV]) - 1; // 1-based to 0-based
    int root_cv_bus_idx = int(self->v[kParamRootCV]) - 1;   // 1-based to 0-based

    // Clamp bus indices to valid range
    pitch_cv_bus_idx = std::clamp(pitch_cv_bus_idx, 0, MAX_CHAN);
    root_cv_bus_idx = std::clamp(root_cv_bus_idx, 0, MAX_CHAN);

    // pointers into each channel's block
    float* outA = busFrames + chanA * numFrames;
    float* outB = busFrames + chanB * numFrames;

    // 0 = mix, nonzero = replace
    bool replaceA = (self->v[kParamOutputAMode] != 0);
    bool replaceB = (self->v[kParamOutputBMode] != 0);

    constexpr int BS = kBlockSize;  // 8 samples

    for (int frame = 0; frame < numFrames; frame += BS)
    {
        // Read CV input values for the current block from the first sample of the block
        // Assuming CV inputs are 0.0 to 1.0, map to +/- 5V range (10 octaves = 120 semitones)
        // This is a common Eurorack CV scaling.
        f current_pitch_cv_value = f((busFrames[pitch_cv_bus_idx * numFrames + frame] - 0.5f) * 120.0f);
        f current_root_cv_value = f((busFrames[root_cv_bus_idx * numFrames + frame] - 0.5f) * 120.0f);

        // Apply CV to parameters, adding to the base pot values
        dtc->params.pitch = f(dtc->pitch_pot_base) + current_pitch_cv_value;
        dtc->params.root = f(dtc->root_pot_base) + current_root_cv_value;

        // produce BS stereo samples in dtc->blk
        dtc->osc.Process(dtc->blk);

        int valid = std::min(BS, numFrames - frame);
        for (int i = 0; i < valid; ++i)
        {
            // convert fixed-point to float
            float sampleL = Float(dtc->blk[i].l).repr();
            float sampleR = Float(dtc->blk[i].r).repr();

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
    .guid = NT_MULTICHAR('N', 'T', 'E', 'O'),
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

extern "C" uintptr_t pluginEntry(_NT_selector selector, uint32_t index)
{
  switch (selector)
  {
  case kNT_selector_version:
    return kNT_apiVersionCurrent;
  case kNT_selector_numFactories:
    return 1;
  case kNT_selector_factoryInfo:
    return (index == 0) ? (uintptr_t)&factory : 0;
  }
  return 0;
}
