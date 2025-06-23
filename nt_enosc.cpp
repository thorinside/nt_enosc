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
    NT_PARAMETER_CV_INPUT("Pitch CV Input", 0, 1)
    NT_PARAMETER_CV_INPUT("Root CV Input", 0, 2)
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Output A", 1, 13)
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Output B", 1, 14)
    {.name = "Balance", .min = -100, .max = 100, .def = 100, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL},
    {.name = "Root Note", .min = 0, .max = 21, .def = 12, .unit = kNT_unitMIDINote, .scaling = 0, .enumStrings = NULL},
    {.name = "Pitch", .min = 0, .max = 127, .def = 0, .unit = kNT_unitMIDINote, .scaling = 0, .enumStrings = NULL},
    {.name = "Spread", .min = 0,  .max = 12, .def = 4, .unit = kNT_unitSemitones, .scaling = 0, .enumStrings = NULL},
    
    //detune should always be positive
    {.name = "Detune", .min = 0,  .max = 100, .def = 0, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL},

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
#ifdef LEARN_ENABLED
    {.name = "Learn", .min = 0, .max = 1, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = enumLearn},
    {.name = "Crossfade", .min = 0, .max = 100, .def = 12, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL}, // 12% approx 0.125 internal
    {.name = "New Note MIDI", .min = 0, .max = 127, .def = 60, .unit = kNT_unitMIDINote, .scaling = 0, .enumStrings = NULL},
    {.name = "Fine Tune", .min = -100, .max = 100, .def = 0, .unit = kNT_unitSemitones, .scaling = kNT_scaling100, .enumStrings = NULL},
    {.name = "Add Note", .min = 0, .max = 1, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = enumAction},
    {.name = "Remove Last Note", .min = 0, .max = 1, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = enumAction},
    {.name = "Reset Scale", .min = 0, .max = 1, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = enumAction},
    {.name = "Manual Learn", .min = 0, .max = 1, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = enumLearn},
#endif
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
  int16_t pitch_pot_base = 0;         // Initialized from kParamPitch.def (MIDI 0-127)
  int16_t root_panel_midi_note = 0;    // Initialized from kParamRoot.def (MIDI 0-21)

  int16_t prev_kParamLearn_val = 0; // Stores the previous state of kParamLearn (0 or 1)
  int16_t dtc_manual_learn_offset = 0; // Stores difference between pitch_pot_base and root_panel_midi_note (semitones)

  _ntEnosc_DTC(_NT_algorithm *self) : osc(params) {
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
  int16_t val = self->v[p]; // Use int16_t directly

  // All parameters are directly handled, no UI modes needed
  switch (p) {
#ifdef LEARN_ENABLED
  case kParamLearn: {
    int16_t prev_learn_val = a->dtc->prev_kParamLearn_val;
    if (val == 1 && prev_learn_val == 0) { // Learn parameter turned on (val is 0 or 1)
      a->dtc->osc.enable_learn();
      a->dtc->osc.enable_pre_listen();
      // Calculate dtc_manual_learn_offset: difference between pitch knob and root offset knob
      a->dtc->dtc_manual_learn_offset = a->dtc->pitch_pot_base - a->dtc->root_panel_midi_note; 
    } else if (val == 0 && prev_learn_val == 1) { // Learn parameter turned off
      a->dtc->osc.disable_learn();
    }
    a->dtc->prev_kParamLearn_val = val; 
    break;
  }
  case kParamManualLearn: {
    if (bool(val)) { // Manual Learn turned on (val is 0 or 1)
      a->dtc->osc.enable_pre_listen();
      a->dtc->osc.enable_follow_new_note();
      // Initialize new_note to current pitch pot base when entering manual learn
      params.new_note = f(a->dtc->pitch_pot_base); 
    } else { // Manual Learn turned off
      a->dtc->osc.disable_follow_new_note();
    }
    break;
  }
#endif
  case kParamFreeze: {
    a->dtc->osc.set_freeze(bool(val)); // val is 0 or 1
    break;
  }
  case kParamBalance: {
    // Original hardware logic: f(val) -> -1..1 -> cubic -> *4 -> exp2 -> 0.0625..16
    f balance = f(val / 100.f); // val is -100 to 100 -> f is -1.0 to 1.0
    balance *= balance * balance;     // Apply cubic curve
    balance *= 4.0_f;                 // Scale to -4..4
    params.balance = Math::fast_exp2(balance); // Apply exponential curve
    break;
  }
  case kParamRoot:
    a->dtc->root_panel_midi_note = val; 
    // params.root is calculated in step() by combining this with CV
    break;
  case kParamPitch:
    // params.pitch = f(val); // This is calculated in step() from base, CV, and fine_tune
    a->dtc->pitch_pot_base = val; // Store panel value for CV calculation and learn offset
    break;
  case kParamSpread: {
     // Original hardware logic: f(val) -> 0..1 -> scale by 10/16 -> * kSpreadRange (12)
    f spread_val = f(val); // val is 0-12
    // The original hardware scaled this by a factor based on NumOsc,
    // let's apply a similar scaling.
    spread_val *= f(10.0f / 16.0f);
    params.spread = spread_val;
    break;
  }
  case kParamDetune: {
    f detune_val = f(val / 100.f); // val is 0-100 -> f is 0.0-1.0
    detune_val = (detune_val * detune_val) * (detune_val * detune_val); // Apply x^4 curve
    detune_val *= f(10.0f / 16.0f); // Scale like the original (kMaxNumOsc is 16)
    params.detune = detune_val;
    break;
  }
  case kParamModMode:
    params.modulation.mode = ModulationMode(val); // Explicit cast to enum is good
    break;
  case kParamModValue: {
    params.modulation.value = f(val / 100.f); // val promoted to float for division
    break;
  }
  case kParamScaleMode: {
    params.scale.mode = ScaleMode(val); // Explicit cast to enum is good
    break;
  }
  case kParamScaleValue: {
    params.scale.value = val; // Implicit conversion from int16_t to int is fine
    break;
  }
  case kParamTwistMode:
    params.twist.mode = TwistMode(val); // Explicit cast to enum is good
    break;
  case kParamTwistValue:
  {
    params.twist.value = f(val / 100.f); // val promoted to float for division
    break;
  }
  case kParamWarpMode:
    params.warp.mode = WarpMode(val); // Explicit cast to enum is good
    break;
  case kParamWarpValue:
  {
    params.warp.value = f(val / 100.f); // val promoted to float for division
    break;
  }
  case kParamNumOsc: {
    params.alt.numOsc = val; // Implicit conversion from int16_t to int is fine
    break;
  }
  case kParamStereoMode: {
    params.alt.stereo_mode = static_cast<SplitMode>(val); // Explicit cast to enum is good
    break;
  }
  case kParamFreezeMode: {
    params.alt.freeze_mode = static_cast<SplitMode>(val); // Explicit cast to enum is good
    break;
  }
#ifdef LEARN_ENABLED
  case kParamCrossfade: {
    params.alt.crossfade_factor = f(val / 100.f); // val promoted to float for division
    break;
  }
  case kParamNewNote:
    params.new_note = f(val); // val implicitly converted to float for f() argument
    break;
  case kParamFineTune:
    params.fine_tune = f(val / 100.f); // val promoted to float for division
    break;
  case kParamAddNote:
    if (bool(val)) { // val is 0 or 1
      // params.new_note is f (MIDI 0-127)
      // dtc_manual_learn_offset is int16_t (semitone difference)
      // params.fine_tune is f (semitone offset +/-1)
      float note_to_add_float = params.new_note.repr() + static_cast<float>(a->dtc->dtc_manual_learn_offset) + params.fine_tune.repr();
      float clamped_note_float = std::clamp(note_to_add_float, 0.0f, 127.0f);
      a->dtc->osc.new_note(f(clamped_note_float));
    }
    break;
  case kParamRemoveLastNote:
    if (bool(val)) { // val is 0 or 1
      a->dtc->osc.remove_last_note();
    }
    break;
  case kParamResetScale:
    if (bool(val)) { // val is 0 or 1
      a->dtc->osc.reset_current_scale();
    }
    break;
#endif
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
        // Read CV input values. Assume busFrames provide direct voltage readings.
        // Convert 1V/Oct to semitone offset (voltage * 12.0f)
        f current_pitch_cv_value = f(busFrames[pitch_cv_bus_idx * numFrames + frame] * 12.0f);
        f current_root_cv_value = f(busFrames[root_cv_bus_idx * numFrames + frame] * 12.0f); 

        // Calculate final pitch for the oscillator (MIDI note 0-127)
        // pitch_pot_base is float (panel MIDI note), CV and fine_tune are f (semitone offsets)
        float calculated_pitch_float = dtc->pitch_pot_base + current_pitch_cv_value.repr() + dtc->params.fine_tune.repr();
        dtc->params.pitch = f(std::clamp(calculated_pitch_float, 0.0f, 127.0f));

        // The "Pitch" knob (0-127) now acts as a bipolar offset around a center point (e.g., C4/MIDI 60)
        // This mimics the original hardware's behavior.
        const float pitch_range = 72.0f; // 6 octaves, same as kPitchPotRange
        float pitch_offset = (dtc->pitch_pot_base / 127.f) * pitch_range;
        pitch_offset -= pitch_range / 2.0f; // Center the range, making it -36 to +36

        // The final pitch combines a fixed center note, the bipolar offset, CV, and fine tune
        dtc->params.pitch = f(60.0f + pitch_offset) + current_pitch_cv_value + dtc->params.fine_tune;
        
        // The "Root" knob (0-21) is also a bipolar offset.
        const float root_range = 21.0f;
        float root_offset = (dtc->root_panel_midi_note / 21.f) * root_range;

        // The final root is the offset plus CV.
        // The Scale::Process function will handle wrapping this into the correct octave.
        dtc->params.root = f(root_offset) + current_root_cv_value;

        // produce BS stereo samples in dtc->blk
        dtc->osc.Process(dtc->blk);

        int valid = std::min(BS, numFrames - frame);
        for (int i = 0; i < valid; ++i)
        {
            // convert fixed-point to float
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
