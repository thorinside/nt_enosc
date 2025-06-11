#pragma once

#include "dsp.hh"
#include "parameters.hh"

static constexpr int sine_size = 512 + 1;
static constexpr int cheby_tables = 16;
static constexpr int cheby_size = 512 + 1;
static constexpr int fold_size = 1024 + 1;

struct DynamicData {
  DynamicData();
  Buffer<std::pair<s1_15, s1_15>, sine_size> sine;
  Buffer<Buffer<f, cheby_size>, cheby_tables> cheby;
  Buffer<std::pair<f, f>, fold_size> fold;
  Buffer<f, (fold_size-1)/2 + 1> fold_max;
  Buffer<Buffer<f, 9>, 8> triangles;
  // Normalization factors for per-oscillator amplitude scaling (0..kMaxNumOsc)
  Buffer<f, kMaxNumOsc + 1> normalization_factors;
};

// Pointer to the DynamicData instance placed in DRAM by the plugin
namespace Data {
  extern DynamicData* data;
}
