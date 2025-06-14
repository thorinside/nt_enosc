#ifndef DYNAMIC_DATA_LOCAL_HH
#define DYNAMIC_DATA_LOCAL_HH

#pragma once

#include "enosc/lib/easiglib/dsp.hh"

static constexpr int sine_size = 512 + 1;
static constexpr int cheby_tables = 16;
static constexpr int cheby_size = 512 + 1;
static constexpr int fold_size = 1024 + 1;

struct DynamicData {
  DynamicData();
  static Buffer<std::pair<s1_15, s1_15>, sine_size> sine
      __attribute__((section(".sdram_bss")));
  static Buffer<Buffer<f, cheby_size>, cheby_tables> cheby
      __attribute__((section(".sdram_bss")));
  static Buffer<std::pair<f, f>, fold_size> fold
      __attribute__((section(".sdram_bss")));
  static Buffer<f, (fold_size - 1) / 2 + 1> fold_max
      __attribute__((section(".sdram_bss")));
  static Buffer<Buffer<f, 9>, 8> triangles
      __attribute__((section(".sdram_bss")));
};

#endif // DYNAMIC_DATA_LOCAL_HH
