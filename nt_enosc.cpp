#include "enosc_plugin_stubs.h"

#include <atomic>
#include <distingnt/api.h>
#include <distingnt/serialisation.h>
#include <math.h>
#include <new>

#include "./enosc/data.hh"
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

  kParamOutput,
  kParamOutputMode,

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

// Parameter definitions
// clang-format off
static const _NT_parameter parameters[] = {
    NT_PARAMETER_CV_INPUT("Pitch CV Input", 0, 1)
    NT_PARAMETER_CV_INPUT("Root CV Input", 0, 2)
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Output", 1, 13)
    {.name = "Balance", .min = -100, .max = 100, .def = 0, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL},
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
  _ntEnosc_DTC(_NT_algorithm *self) : osc(params) {
  }
};

struct _ntEnosc_Alg : public _NT_algorithm
{
  _ntEnosc_Alg(_ntEnosc_DTC *d) : dtc(d) {}
  _ntEnosc_DTC *dtc;
};

void calculateStaticRequirements(_NT_staticRequirements &req)
{
  req.dram = 0;
}

void initialise(_NT_staticMemoryPtrs &ptrs, const _NT_staticRequirements &req)
{
  // All data is now static, so no initialisation is needed.
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
  switch (p)
  {
  case kParamBalance: {
    f bb = f(v / 100.f) * 2_f - 1_f;      // -1..1
    bb *= bb * bb;                        // -1..1 cubic
    bb *= 4_f;                            // -4..4
    params.balance = Math::fast_exp2(bb); // 0.0625..16
    break;
  }
  case kParamRoot:
    params.root = f(v);
    break;
  case kParamPitch:
    params.pitch = f(v);
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

    /* 1. Let 0 % really be silence but give low end more resolution          *
     *    (square curve – same trick we used for Twist/Warp).                 */
    m *= m;

    /* 2. Normalise for polyphony so the same 50 % sounds similar whether     *
     *    you run 1 or 16 oscillators.                                        */
    m *= 4_f / f(params.alt.numOsc);   // 4 instead of 6 → gentler overall

    /* 3. Mode-specific trims. These are now *relative* multipliers,          *
     *    not 6 × 0.9 × 4 swings.                                             */
    switch (params.modulation.mode) {
      case ONE:    m *= 2.5_f; break;   // bright metallic, like the original
      case TWO:    m *= 1.0_f; break;   // classic "DX" cross-mod
      case THREE:  m *= 1.8_f; break;   // richer but still controlled
    }

    /* 4. Safety – the engine stays well-behaved < 5. */
    if (m > 4.999_f) m = 4.999_f;

    params.modulation.value = m;
    break;

}  case kParamScaleMode: {
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
    /* 0 … 100 % from the UI → 0 … 1.0 internal */
    f tw = f(v / 100.f);

    /* No forced floor: let 0 % really be 0 for a clean sine. */
    if (params.twist.mode == FEEDBACK)
    {
      /* gentle quadratic curve, same as before but floor removed */
      tw *= tw * 0.7_f;
    }
    else if (params.twist.mode == PULSAR)
    {
      /* 0 … 1  →  1 … 64  (exp2(6 · x)) */
      tw = Math::fast_exp2(tw * 6_f);
      if (tw < 1_f)    /* guarantee spec range 1…64 */
        tw = 1_f;
    }
    else /* CRUSH */
    {
      tw = tw * tw * 0.5_f;
    }
    params.twist.value = tw;
    break;
  }

  /* ──────────────────────────────────── WARP ──────────────────────────────────────────────────── */
  case kParamWarpMode:
    params.warp.mode = WarpMode(int(v));
    break;

  case kParamWarpValue:
  {
    /* 0 … 100 % → 0 … 1.0 */
    f wr = f(v / 100.f);

    if (params.warp.mode == FOLD)
    {
      /* keep the old curve, but now it really starts at 0 % = 0  */
      wr = wr * wr;            // square for finer low-end control
      wr = wr * 0.9_f + 0.004_f;
      if (wr > 0.999_f)        // absolute safety limit
        wr = 0.999_f;
    }
    else  /* CHEBY or SEGMENT */
    {
      /* Clamp strictly below 1.0 to stay inside the lookup-table bounds */
      if (wr >= 0.999_f)
        wr = 0.999_f;
    }
    params.warp.value = wr;
    break;
  }
  case kParamNumOsc:
    params.alt.numOsc = static_cast<int>(v);
    break;
  case kParamStereoMode:
    params.alt.stereo_mode = SplitMode(int(v));
    break;
  case kParamFreezeMode:
    params.alt.freeze_mode = SplitMode(int(v));
    break;
  case kParamFreeze:
    a->dtc->osc.set_freeze(bool(v));
    break;
  case kParamLearn:
    if (bool(v)) {
      a->dtc->osc.enable_learn();
      a->dtc->osc.enable_pre_listen();
    } else {
      a->dtc->osc.disable_learn();
    }
    break;
  case kParamCrossfade:
    params.alt.crossfade_factor = f(v / 100.f);
    break;
  case kParamNewNote:
    params.new_note = f(v);
    break;
  case kParamFineTune:
    params.fine_tune = f(v / 100.f);
    break;
  case kParamAddNote:
    if (bool(v)) {
      a->dtc->osc.new_note(params.new_note + params.fine_tune);
      a->dtc->osc.enable_follow_new_note();
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

// // ────────────────────────────────────────────────────────────────
// //  TEST STEP  –  fixed 440 Hz reference sine
// // ────────────────────────────────────────────────────────────────
// #include <math.h>   // for sinf, M_PI
//
// void step( _NT_algorithm* self, float* busFrames, int numFramesBy4 )
// {
//   constexpr float kSampleRate = 48000.0f;      // NT runs at 48 kHz
//   constexpr float kFreq       = 440.0f;        // reference A4
//   constexpr float kAmp        = 0.5f;          // –6 dBFS → plenty of headroom
//   constexpr float kTwoPiOverSR= 2.0f * M_PI / kSampleRate;
//
//   static float phase = 0.0f;                   // keeps continuity between calls
//
//   const int numFrames = numFramesBy4 * 4;
//
//   /* ----------------- choose which stereo pair to write to --------------- */
//   const int chan  = self->v[kParamOutput] - 1; // 0-based index (UI is 1-based)
//   float* outL = busFrames + chan * numFrames;
//   float* outR = outL      + numFrames;
//
//   const bool replace = self->v[kParamOutputMode];  // 0 = mix, 1 = replace
//
//   /* ---------------------------- generate tone --------------------------- */
//   for (int n = 0; n < numFrames; ++n)
//   {
//     const float sample = kAmp * sinf( phase );
//     phase += kTwoPiOverSR * kFreq;
//     if (phase > 2.0f * M_PI)       // wrap to avoid precision loss
//       phase -= 2.0f * M_PI;
//
//     if (replace) {
//       outL[n] = sample;
//       outR[n] = sample;
//     } else {
//       outL[n] += sample;
//       outR[n] += sample;
//     }
//   }
// }

void step( _NT_algorithm* self, float* busFrames, int numFramesBy4 )
{
    auto* alg = (_ntEnosc_Alg*)self;
    auto* dtc = alg->dtc;

    const int numFrames = numFramesBy4 * 4;
    const int chan      = self->v[kParamOutput] - 1;        // 1-based in UI
    float* outL = busFrames + chan * numFrames;             // block-per-channel layout
    float* outR = outL      + numFrames;
    const bool replace = self->v[kParamOutputMode];         // 0 = mix, 1 = replace

    /* these three don't need smoothing but *can* change block-by-block      */
    dtc->params.alt.numOsc = dtc->next_num_osc.load(std::memory_order_relaxed);
    dtc->params.root       = f( self->v[kParamRoot]  );
    dtc->params.pitch      = f( self->v[kParamPitch] );

    /* --------------------------------------------------------------------
     * 2.  Generate audio exactly in 8-sample chunks (kBlockSize) – the same
     *     cadence the original STM32 firmware uses.
     * ------------------------------------------------------------------ */
    constexpr int BS = kBlockSize;

    for (int frame = 0; frame < numFrames; frame += BS)
    {
        dtc->osc.Process( dtc->blk );                  // fills blk[0 .. BS-1]

        const int valid = std::min( BS, numFrames - frame );
        for (int i = 0; i < valid; ++i)
        {
            // Convert s9_23 → float; *do not* add any extra scaling.
            const float l = Float( dtc->blk[i].l ).repr();
            const float r = Float( dtc->blk[i].r ).repr();

            if (replace)
            {
                outL[frame + i] = l;
                outR[frame + i] = r;
            }
            else
            {
                outL[frame + i] += l;
                outR[frame + i] += r;
            }
        }
    }
}

void serialise(_NT_algorithm *self, _NT_jsonStream &stream)
{
  // All parameters, so no need to serialise
}

bool deserialise(_NT_algorithm *self, _NT_jsonParse &parse)
{
  // All parameters, so no need to deserialise
  return false;
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
    .serialise = serialise,
    .deserialise = deserialise,
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
