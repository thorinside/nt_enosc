#pragma GCC push_options
#pragma GCC optimize("Os")

#include "dynamic_data.hh"

const Buffer<f, 17> Data::normalization_factors = {{{
    f(1.000000),
    f(1.000000),
    f(1.000000),
    f(1.000000),
    f(0.816824),
    f(0.577721),
    f(0.408248),
    f(0.288675),
    f(0.204124),
    f(0.144338),
    f(0.102062),
    f(0.072169),
    f(0.051031),
    f(0.036084),
    f(0.025515),
    f(0.018042),
    f(0.012758),
}}};

/* triangles */
Buffer<Buffer<s8_0, 9>, 8> triangles_12ths = {{{
    {{{
        -12._s8_0,
        -9._s8_0,
        -6._s8_0,
        -3._s8_0,
        0._s8_0,
        3._s8_0,
        6._s8_0,
        9._s8_0,
        12._s8_0,
    }}},
    {{{
        -12._s8_0,
        -12._s8_0,
        -8._s8_0,
        -4._s8_0,
        0._s8_0,
        4._s8_0,
        8._s8_0,
        12._s8_0,
        12._s8_0,
    }}},
    {{{
        -12._s8_0,
        -12._s8_0,
        -12._s8_0,
        -6._s8_0,
        0._s8_0,
        6._s8_0,
        12._s8_0,
        12._s8_0,
        12._s8_0,
    }}},
    {{{
        -12._s8_0,
        -12._s8_0,
        -12._s8_0,
        -12._s8_0,
        0._s8_0,
        12._s8_0,
        12._s8_0,
        12._s8_0,
        12._s8_0,
    }}},
    {{{
        -12._s8_0,
        -6._s8_0,
        -12._s8_0,
        -6._s8_0,
        0._s8_0,
        6._s8_0,
        12._s8_0,
        6._s8_0,
        12._s8_0,
    }}},
    {{{
        -12._s8_0,
        -6._s8_0,
        -0._s8_0,
        -12._s8_0,
        0._s8_0,
        12._s8_0,
        0._s8_0,
        6._s8_0,
        12._s8_0,
    }}},
    {{{
        -12._s8_0,
        -6._s8_0,
        12._s8_0,
        -12._s8_0,
        0._s8_0,
        12._s8_0,
        -12._s8_0,
        6._s8_0,
        12._s8_0,
    }}},
    {{{
        12._s8_0,
        -12._s8_0,
        12._s8_0,
        -12._s8_0,
        0._s8_0,
        12._s8_0,
        -12._s8_0,
        12._s8_0,
        -12._s8_0,
    }}},
}}};

Buffer<std::pair<s1_15, s1_15>, sine_size> DynamicData::sine;
Buffer<Buffer<f, cheby_size>, cheby_tables> DynamicData::cheby;
Buffer<std::pair<f, f>, fold_size> DynamicData::fold;
Buffer<f, (fold_size - 1) / 2 + 1> DynamicData::fold_max;
Buffer<Buffer<f, 9>, 8> DynamicData::triangles;

DynamicData::DynamicData() {
  /*
  using namespace easiglib::numtypes;
  // sine + difference
  {
    MagicSine magic(1_f / f(sine_size - 1));
    s1_15 previous = s1_15::inclusive(magic.Process());
    for (auto &[v, d] : sine)
    {
      v = previous;
      previous = s1_15::inclusive(magic.Process());
      d = previous - v;
    }
  }

  // cheby[1] = [-1..1]
  for (int i = 0; i < cheby_size; i++)
    cheby[0][i] = f(i * 2) / f(cheby_size - 1) - 1_f;

  // cheby[2] = 2 * cheby[1] * cheby[1] - 1
  for (int i = 0; i < cheby_size; i++)
    cheby[1][i] = 2_f * cheby[0][i] * cheby[0][i] - 1_f;

  // cheby[n] = 2 * cheby[1] * cheby[n-1] - cheby[n-2]
  for (int n = 2; n < cheby_tables; n++)
    for (int i = 0; i < cheby_size; i++)
      cheby[n][i] = 2_f * cheby[0][i] * cheby[n - 1][i] - cheby[n - 2][i];

  // fold
  {
    f folds = 6_f;
    f previous = 0_f;
    for (int i = 0; i < fold_size; ++i)
    {
      // TODO: this -3 make the wavefolding curve symmetrical; why?
      f x = f(i) / f(fold_size - 3); // 0..1
      x = folds * (2_f * x - 1_f);   // -folds..folds
      f g = 1_f / (1_f + x.abs());   // 0..0.5
      f p = 16_f / (2_f * Math::pi) * x * g;
      while (p > 1_f)
        p--;
      while (p < 0_f)
        p++;
      x = -g * (x + Math::fast_sine(p));
      fold[i] = i == 0 ? std::pair(x, 0_f) : std::pair(previous, x - previous);
      previous = x;
    }
  }

  // fold_max
  {
    f max = 0_f;
    int start = (fold_size - 1) / 2;
    for (int i = 0; i < fold_max.size(); ++i)
    {
      max = fold[i + start].first.abs().max(max);
      // the attenuation factor accounts for interpolation error, so
      // we don't overestimate the 1/x curve and amplify to clipping
      fold_max[i] = (max * 1.05_f).reciprocal();
    }
  }

  // triangles
  {
    // This is a direct copy of the pre-calculated triangle tables from the
  original source. static const s8_0 triangles_12ths[8][9] = {
        {{-12_s8_0, -9_s8_0, -6_s8_0, -3_s8_0, 0_s8_0, 3_s8_0, 6_s8_0, 9_s8_0,
  12_s8_0}},
        {{-12_s8_0, -12_s8_0, -8_s8_0, -4_s8_0, 0_s8_0, 4_s8_0, 8_s8_0, 12_s8_0,
  12_s8_0}},
        {{-12_s8_0, -12_s8_0, -12_s8_0, -6_s8_0, 0_s8_0, 6_s8_0, 12_s8_0,
  12_s8_0, 12_s8_0}},
        {{-12_s8_0, -12_s8_0, -12_s8_0, -12_s8_0, 0_s8_0, 12_s8_0, 12_s8_0,
  12_s8_0, 12_s8_0}},
        {{-12_s8_0, -6_s8_0, -12_s8_0, -6_s8_0, 0_s8_0, 6_s8_0, 12_s8_0, 6_s8_0,
  12_s8_0}},
        {{-12_s8_0, -6_s8_0, 0_s8_0, -12_s8_0, 0_s8_0, 12_s8_0, 0_s8_0, 6_s8_0,
  12_s8_0}},
        {{-12_s8_0, -6_s8_0, 12_s8_0, -12_s8_0, 0_s8_0, 12_s8_0, -12_s8_0,
  6_s8_0, 12_s8_0}},
        {{12_s8_0, -12_s8_0, 12_s8_0, -12_s8_0, 0_s8_0, 12_s8_0, -12_s8_0,
  12_s8_0, -12_s8_0}},
    };
    for (int i = 0; i < 8; i++)
      for (int j = 0; j < 9; j++)
        triangles[i][j] = (f)(triangles_12ths[i][j]) / 12.0_f;
  }
  */
}

#pragma GCC pop_options
