/* enosc_plugin_stubs.h  –– minimal “fake” CMSIS layer for plugin build */

#ifndef ENOSC_PLUGIN_STUBS_H_
#define ENOSC_PLUGIN_STUBS_H_

#include <cstdint>

/* ------------------------------------------------------------------ */
/*  CMSIS-style intrinsics                                             */
/* ------------------------------------------------------------------ */
#define __INLINE        inline
#define __STATIC_INLINE static inline

/* Saturating adds – here we just fall back to plain +                */
#define __QADD(a, b)    ((a) + (b))
#define __QADD16(a, b)  ((a) + (b))
#define __UQADD16(a, b) ((a) + (b))
#define __QADD8(a, b)   ((a) + (b))
#define __UQADD8(a, b)  ((a) + (b))

#define __ASM __asm__

/* Cortex-M4 DSP half-precision type                                   */
using __fp16 = float;

/* ------------------------------------------------------------------ */
/*  Bare-minimum stubs for storage / hardware layers                   */
/* ------------------------------------------------------------------ */
struct ScaleTable { int16_t note[12]; };

template <int CELL_NR, typename T> struct FlashBlock {
  using data_t = T;
  static constexpr int cell_nr_ = CELL_NR;
  static bool Read(data_t*, int)               { return false; }
  static bool IsWriteable(int)                 { return false; }
  static void Erase()                          {}
  static bool Write(data_t*, int)              { return true; }
};

inline void HAL_IncTick()  {}
inline uint32_t HAL_GetTick() { return 0; }

#endif /* ENOSC_PLUGIN_STUBS_H_ */

