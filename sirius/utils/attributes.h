#pragma once

#include "sirius/attributes.h"

// clang-format off

#define utils_check_sizeof(sirius, platform) \
  sirius_static_assert(sizeof(sirius) >= sizeof(platform),\
  "The size of `" #sirius "` is smaller than that of `" #platform "`")

#define utils_check_alignof(sirius, platform) \
  sirius_static_assert(alignof(sirius) >= alignof(platform),\
  "The alignment of `" #sirius "` is weaker than that of `" #platform "`")

// clang-format on

#ifdef __cplusplus
// --- utils_pretty_function ---
#  ifdef utils_pretty_function
#    undef utils_pretty_function
#  endif
#  if defined(__GNUC__) || defined(__clang__)
#    define utils_pretty_function __PRETTY_FUNCTION__
#  elif defined(_MSC_VER)
#    define utils_pretty_function __FUNCSIG__
#  else
#    define utils_pretty_function __func__
#  endif
#endif
