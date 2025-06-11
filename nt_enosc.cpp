#include <math.h>
#include <new>
#include <distingnt/api.h>
#include <distingnt/serialisation.h>

// Include EnOSC core engine and DSP support
#include "parameters.hh"
#include "util.hh"
#include "buffer.hh"
#include "bitfield.hh"
#include "units.hh"
#include "filter.hh"
#include "dsp.hh"
#include "signal.hh"
#include "math.hh"
#include "numtypes.hh"
#include "persistent_storage.hh"
#include "dynamic_data.hh"
#include "dsp.cc"
#include "numtypes.cc"
#include "dynamic_data.cc"
#include "math.cc"
#include "oscillator.hh"
#include "distortion.hh"
#include "quantizer.hh"

#include "polyptic_oscillator.hh"

// Plugin parameters mapping to EnOSC engine Parameters
enum
{
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
    {.name = "Spread", .min = 0, .max = 36, .def = 0, .unit = kNT_unitSemitones, .scaling = 0, .enumStrings = NULL},
    {.name = "Detune", .min = -12, .max = 12, .def = 0, .unit = kNT_unitSemitones, .scaling = 0, .enumStrings = NULL},

    {.name = "Mod mode", .min = 0, .max = 2, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = enumMod},
    {.name = "Cross FM", .min = 0, .max = 100, .def = 0, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL},

    {.name = "Scale Mode", .min = 0, .max = 2, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = enumScale},
    {.name = "Scale Preset", .min = 0, .max = 9, .def = 0, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},

    {.name = "Twist mode", .min = 0, .max = 2, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = enumTwist},
    {.name = "Twist", .min = 0, .max = 100, .def = 0, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL},

    {.name = "Warp mode", .min = 0, .max = 2, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = enumWarp},
    {.name = "Warp", .min = 0, .max = 100, .def = 0, .unit = kNT_unitPercent, .scaling = 0, .enumStrings = NULL},

    {.name = "Num Osc", .min = 1, .max = kMaxNumOsc, .def = kMaxNumOsc, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL},
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

// DTC struct holds algorithm state and oscillator instance
struct _ntEnosc_DTC
{
  Parameters params;
  PolypticOscillator<kBlockSize> osc;
  Scale *current_scale;
  _ntEnosc_DTC() : osc(params) {}
};

struct _ntEnosc_Alg : public _NT_algorithm
{
  _ntEnosc_Alg(_ntEnosc_DTC *d) : dtc(d) {}
  _ntEnosc_DTC *dtc;
};

void calculateStaticRequirements(_NT_staticRequirements &req)
{
  req.dram = sizeof(DynamicData);
}

void initialise(_NT_staticMemoryPtrs &ptrs, const _NT_staticRequirements &req)
{
  // Populate lookup tables in DRAM region
  new (ptrs.dram) DynamicData;
  // Keep a global pointer to the DRAM-held DynamicData instance
  auto *dd = reinterpret_cast<DynamicData *>(ptrs.dram);
  Data::data = dd;
  // Init normalization factors as unity
  for (int i = 0; i <= kMaxNumOsc; ++i)
    dd->normalization_factors[i] = 1_f;
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
  auto *d = new (ptrs.dtc) _ntEnosc_DTC();
  // Seed default FREE-mode scales to ensure valid default scale tables
  for (int i = 0; i < kScaleNr; ++i)
  {
    Parameters::Scale ss{ScaleMode::FREE, i};
    d->osc.reset_scale(ss);
  }
  auto *alg = new (ptrs.sram) _ntEnosc_Alg(d);
  alg->parameters = parameters;
  alg->parameterPages = nullptr;
  return alg;
}

void parameterChanged(_NT_algorithm *self, int p)
{
  _ntEnosc_Alg *a = (_ntEnosc_Alg *)self;
  auto &params = a->dtc->params;
  float v = self->v[p];
  switch (p)
  {
  case kParamBalance:
    params.balance = f(v / 100.f);
    break;
  case kParamRoot:
    params.root = f(v);
    break;
  case kParamPitch:
    params.pitch = f(v);
    break;
  case kParamSpread:
    params.spread = f(v);
    break;
  case kParamDetune:
    params.detune = f(v);
    break;
  case kParamModMode:
    params.modulation.mode = ModulationMode(int(v));
    break;
  case kParamModValue:
    params.modulation.value = f(v / 100.f);
    break;
  case kParamScaleMode:
    params.scale.mode = ScaleMode(int(v));
    a->dtc->osc.reset_scale(params.scale);
    break;
  case kParamScaleValue:
    params.scale.value = int(v);
    a->dtc->osc.reset_scale(params.scale);
    break;
  case kParamTwistMode:
    params.twist.mode = TwistMode(int(v));
    break;
  case kParamTwistValue:
    params.twist.value = f(v / 100.f);
    break;
  case kParamWarpMode:
    params.warp.mode = WarpMode(int(v));
    break;
  case kParamWarpValue:
    params.warp.value = f(v / 100.f);
    break;
  case kParamNumOsc:
    params.alt.numOsc = int(v);
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

void step(_NT_algorithm *self, float *busFrames, int numFramesBy4)
{
  _ntEnosc_Alg *a = (_ntEnosc_Alg *)self;
  auto *d = a->dtc;
  int numFrames = numFramesBy4 * 4;

  const float* pitchCV = busFrames + ( self->v[kParamPitchCV] - 1 ) * numFrames;
  const float* rootCV = busFrames + ( self->v[kParamRootCV] - 1 ) * numFrames;

  int pitchCVBus = self->v[kParamPitchCV];
  if (pitchCVBus > 0)
  {
    d->params.pitch = Float(pitchCV[0] * 12.0f);
  }
  int rootCVBus = self->v[kParamRootCV];
  if (rootCVBus > 0)
  {
    d->params.root = Float(rootCV[0] * 12.0f);
  }

  int output = self->v[kParamOutput] - 1;
  float *outL = busFrames + output * numFrames;
  float *outR = outL + numFrames;
  bool replace = self->v[kParamOutputMode] != 0;

  Buffer<Frame, kBlockSize> block;

  for (int i = 0; i < numFrames; i += kBlockSize)
  {
    d->osc.Process(block);
    for (int j = 0; j < kBlockSize; ++j)
    {
      int frameIndex = i + j;
      if (frameIndex < numFrames)
      {
        float sL = f(block[j].l).repr();
        float sR = f(block[j].r).repr();

        if (replace)
        {
          outL[frameIndex] = sL;
          outR[frameIndex] = sR;
        }
        else
        {
          outL[frameIndex] += sL;
          outR[frameIndex] += sR;
        }
      }
    }
  }
}

// JSON (de)serialization for custom FREE-mode scales
void serialise(_NT_algorithm *self, _NT_jsonStream &stream)
{
  auto *a = (_ntEnosc_Alg *)self;
  auto &params = a->dtc->params;
  if (params.scale.mode == FREE)
  {
    stream.addMemberName("scale_value");
    stream.addNumber(params.scale.value);
    stream.addMemberName("scale_data");
    stream.openArray();
    Scale *s = a->dtc->osc.get_scale(params.scale);
    int n = s->size();
    for (int i = 0; i < n; ++i)
    {
      stream.addNumber(s->get(i).repr());
    }
    stream.closeArray();
  }
}

bool deserialise(_NT_algorithm *self, _NT_jsonParse &parse)
{
  auto *a = (_ntEnosc_Alg *)self;
  int numMembers;
  if (!parse.numberOfObjectMembers(numMembers))
    return false;
  for (int m = 0; m < numMembers; ++m)
  {
    if (parse.matchName("scale_value"))
    {
      int idx = 0;
      if (!parse.number(idx))
        return false;
      a->dtc->params.scale.value = idx;
    }
    else if (parse.matchName("scale_data"))
    {
      int count = 0;
      if (!parse.numberOfArrayElements(count))
        return false;
      PreScale p;
      for (int i = 0; i < count; ++i)
      {
        float v = 0.0f;
        if (!parse.number(v))
          return false;
        p.add(f(v));
      }
      bool wrap = (a->dtc->params.scale.mode == OCTAVE);
      p.copy_to(a->dtc->osc.get_scale(a->dtc->params.scale), wrap);
    }
    else
    {
      if (!parse.skipMember())
        return false;
    }
  }
  return true;
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
