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
#include "oscillator.hh"
#include "distortion.hh"
#include "quantizer.hh"


#include "polyptic_oscillator.hh"

// Plugin parameters mapping to EnOSC engine Parameters
enum {
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
  kParamCrossfade,

  kParamNewNote,
  kParamFineTune,

  kNumParams
};

// Enumeration strings for mode parameters
static const char* const enumMod[]    = { "One",  "Two",   "Three" };
static const char* const enumScale[]  = { "12-TET","Octave","Free" };
static const char* const enumTwist[]  = { "Feedback","Pulsar","Crush" };
static const char* const enumWarp[]   = { "Fold",   "Cheby",  "Segment" };
static const char* const enumSplit[]  = { "Alternate","Lo/Hi","Lowest/Rest" };

// Parameter definitions
static const _NT_parameter parameters[] = {
  NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Output", 1, 13)
  { .name = "Balance",      .min = -1,   .max = 1,   .def = 0,  .unit = kNT_unitNone,        .scaling = 0, .enumStrings = NULL },
  { .name = "Root",         .min = -24,  .max = 24,  .def = 0,  .unit = kNT_unitNone,        .scaling = 0, .enumStrings = NULL },
  { .name = "Pitch",        .min = 0,    .max = 127, .def = 69, .unit = kNT_unitNone,        .scaling = 0, .enumStrings = NULL },
  { .name = "Spread",       .min = 0,    .max = 36,  .def = 0,  .unit = kNT_unitNone,        .scaling = 0, .enumStrings = NULL },
  { .name = "Detune",       .min = -12,  .max = 12,  .def = 0,  .unit = kNT_unitNone,        .scaling = 0, .enumStrings = NULL },

  { .name = "Mod mode",     .min = 0,    .max = 2,   .def = 0,  .unit = kNT_unitEnum,        .scaling = 0, .enumStrings = enumMod },
  { .name = "Modulation",   .min = 0,    .max = 1,   .def = 0,  .unit = kNT_unitNone,        .scaling = 0, .enumStrings = NULL },

  { .name = "Scale mode",   .min = 0,    .max = 2,   .def = 0,  .unit = kNT_unitEnum,        .scaling = 0, .enumStrings = enumScale },
  { .name = "Scale",        .min = 0,    .max = 9,   .def = 0,  .unit = kNT_unitNone,        .scaling = 0, .enumStrings = NULL },

  { .name = "Twist mode",   .min = 0,    .max = 2,   .def = 0,  .unit = kNT_unitEnum,        .scaling = 0, .enumStrings = enumTwist },
  { .name = "Twist",        .min = 0,    .max = 1,   .def = 0,  .unit = kNT_unitNone,        .scaling = 0, .enumStrings = NULL },

  { .name = "Warp mode",    .min = 0,    .max = 2,   .def = 0,  .unit = kNT_unitEnum,        .scaling = 0, .enumStrings = enumWarp },
  { .name = "Warp",         .min = 0,    .max = 1,   .def = 0,  .unit = kNT_unitNone,        .scaling = 0, .enumStrings = NULL },

  { .name = "Num Osc",      .min = 1,    .max = kMaxNumOsc, .def = kMaxNumOsc, .unit = kNT_unitNone, .scaling = 0, .enumStrings = NULL },
  { .name = "Stereo mode",  .min = 0,    .max = 2,   .def = 0,  .unit = kNT_unitEnum,        .scaling = 0, .enumStrings = enumSplit },
  { .name = "Freeze mode",  .min = 0,    .max = 2,   .def = 0,  .unit = kNT_unitEnum,        .scaling = 0, .enumStrings = enumSplit },
  { .name = "Crossfade",    .min = 0,    .max = 1,   .def = 0,      .unit = kNT_unitNone,     .scaling = 0, .enumStrings = NULL },

  { .name = "New note",     .min = 0,    .max = 150, .def = 0,  .unit = kNT_unitNone,        .scaling = 0, .enumStrings = NULL },
  { .name = "Fine tune",    .min = -1,   .max = 1,   .def = 0,  .unit = kNT_unitNone,        .scaling = 0, .enumStrings = NULL },
};

// DTC struct holds algorithm state and oscillator instance
struct _ntEnosc_DTC {
  Parameters params;
  PolypticOscillator<kBlockSize> osc;
  Scale* current_scale;
  _ntEnosc_DTC() : osc(params) {}
};

struct _ntEnosc_Alg : public _NT_algorithm {
  _ntEnosc_Alg(_ntEnosc_DTC* d) : dtc(d) {}
  _ntEnosc_DTC* dtc;
};

void calculateStaticRequirements(_NT_staticRequirements& req) {
  req.dram = sizeof(DynamicData);
}

void initialise(_NT_staticMemoryPtrs& ptrs, const _NT_staticRequirements& req) {
  // Populate lookup tables in DRAM region
  new (ptrs.dram) DynamicData;
  // Init normalization factors as unity in DRAM-held DynamicData
  auto* dd = reinterpret_cast<DynamicData*>(ptrs.dram);
  for (int i = 0; i <= kMaxNumOsc; ++i)
    dd->normalization_factors[i] = 1_f;
}

void calculateRequirements(_NT_algorithmRequirements& req, const int32_t*) {
  req.numParameters = kNumParams;
  req.sram          = sizeof(_ntEnosc_Alg);
  req.dtc           = sizeof(_ntEnosc_DTC);
  req.dram          = 0;
  req.itc           = 0;
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs,
                         const _NT_algorithmRequirements& req,
                         const int32_t*) {
  auto* d = new (ptrs.dtc) _ntEnosc_DTC();
  // Seed default FREE-mode scales to ensure valid default scale tables
  for (int i = 0; i < kScaleNr; ++i) {
    Parameters::Scale ss{ScaleMode::FREE, i};
    d->osc.reset_scale(ss);
  }
  auto* alg = new (ptrs.sram) _ntEnosc_Alg(d);
  alg->parameters = parameters;
  alg->parameterPages = nullptr;
  return alg;
}

void parameterChanged(_NT_algorithm* self, int p) {
  _ntEnosc_Alg* a = (_ntEnosc_Alg*)self;
  auto& params = a->dtc->params;
  float v = self->v[p];
  switch (p) {
    case kParamBalance:      params.balance = f(v); break;
    case kParamRoot:         params.root = f(v); break;
    case kParamPitch:        params.pitch = f(v); break;
    case kParamSpread:       params.spread = f(v); break;
    case kParamDetune:       params.detune = f(v); break;
    case kParamModMode:      params.modulation.mode = ModulationMode(int(v)); break;
    case kParamModValue:     params.modulation.value = f(v); break;
    case kParamScaleMode:    params.scale.mode = ScaleMode(int(v)); break;
    case kParamScaleValue:   params.scale.value = int(v); break;
    case kParamTwistMode:    params.twist.mode = TwistMode(int(v)); break;
    case kParamTwistValue:   params.twist.value = f(v); break;
    case kParamWarpMode:     params.warp.mode = WarpMode(int(v)); break;
    case kParamWarpValue:    params.warp.value = f(v); break;
    case kParamNumOsc:       params.alt.numOsc = int(v); break;
    case kParamStereoMode:   params.alt.stereo_mode = SplitMode(int(v)); break;
    case kParamFreezeMode:   params.alt.freeze_mode = SplitMode(int(v)); break;
    case kParamCrossfade:    params.alt.crossfade_factor = f(v); break;
    case kParamNewNote:      params.new_note = f(v); break;
    case kParamFineTune:     params.fine_tune = f(v); break;
    default: break;
  }
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
  _ntEnosc_Alg* a = (_ntEnosc_Alg*)self;
  auto* d = a->dtc;
  int numFrames = numFramesBy4 * 4;
  // Compute new audio block
  Buffer<Frame, kBlockSize> block;
  d->osc.Process(block);
  // Convert to float and output to selected bus
  float* outL = busFrames + (self->v[kParamOutput] - 1) * numFrames;
  bool replace = self->v[kParamOutputMode] != 0;
  for (int i = 0; i < kBlockSize; ++i) {
    float s = f(block[i].l).repr();
    if (replace) outL[i] = s;
    else         outL[i] += s;
  }
}

// JSON (de)serialization for custom FREE-mode scales
void serialise(_NT_algorithm* self, _NT_jsonStream& stream) {
  auto* a = (_ntEnosc_Alg*)self;
  auto& params = a->dtc->params;
  if (params.scale.mode == FREE) {
    stream.addMemberName("scale_value");
    stream.addNumber(params.scale.value);
    stream.addMemberName("scale_data");
    stream.openArray();
    Scale* s = a->dtc->osc.get_scale(params.scale);
    int n = s->size();
    for (int i = 0; i < n; ++i) {
      stream.addNumber(s->get(i).repr());
    }
    stream.closeArray();
  }
}

bool deserialise(_NT_algorithm* self, _NT_jsonParse& parse) {
  auto* a = (_ntEnosc_Alg*)self;
  int numMembers;
  if (!parse.numberOfObjectMembers(numMembers)) return false;
  for (int m = 0; m < numMembers; ++m) {
    if (parse.matchName("scale_value")) {
      int idx = 0;
      if (!parse.number(idx)) return false;
      a->dtc->params.scale.value = idx;
    } else if (parse.matchName("scale_data")) {
      int count = 0;
      if (!parse.numberOfArrayElements(count)) return false;
      PreScale p;
      for (int i = 0; i < count; ++i) {
        float v = 0.0f;
        if (!parse.number(v)) return false;
        p.add(f(v));
      }
      bool wrap = (a->dtc->params.scale.mode == OCTAVE);
      p.copy_to(a->dtc->osc.get_scale(a->dtc->params.scale), wrap);
    } else {
      if (!parse.skipMember()) return false;
    }
  }
  return true;
}

static const _NT_factory factory = {
  .guid = NT_MULTICHAR('N','T','E','O'),
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

extern "C" uintptr_t pluginEntry(_NT_selector selector, uint32_t index) {
  switch (selector) {
    case kNT_selector_version:       return kNT_apiVersionCurrent;
    case kNT_selector_numFactories:  return 1;
    case kNT_selector_factoryInfo:   return (index == 0) ? (uintptr_t)&factory : 0;
  }
  return 0;
}
