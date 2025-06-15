#pragma once

#include "enosc/lib/easiglib/buffer.hh"
#include "enosc/lib/easiglib/dsp.hh"
#include "enosc/lib/easiglib/numtypes.hh"
#include <utility> // For std::pair

static constexpr int sine_size = 512 + 1;
static constexpr int cheby_tables = 16;
static constexpr int cheby_size = 512 + 1;
static constexpr int fold_size = 1024 + 1;

// Forward declare DynamicDataTables for the global pointer
struct DynamicData;

struct DynamicData {
  static const Buffer<std::pair<s1_15, s1_15>, sine_size> sine;
  static const Buffer<Buffer<f, cheby_size>, cheby_tables> cheby;
  static const Buffer<std::pair<f, f>, fold_size> fold;
  static const Buffer<f, (fold_size - 1) / 2 + 1> fold_max;
  static const Buffer<Buffer<f, 9>, 9> triangles;
};

extern const Buffer<Buffer<s8_0, 9>, 8> triangles_12ths;