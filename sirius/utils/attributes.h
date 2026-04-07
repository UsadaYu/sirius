#pragma once

#include "sirius/attributes.h"

// clang-format off

#define utils_check_sizeof(sirius, platform) \
  ss_static_assert(sizeof(sirius) >= sizeof(platform),\
  "The size of `" #sirius "` is smaller than that of `" #platform "`")

#define utils_check_alignof(sirius, platform) \
  ss_static_assert(alignof(sirius) >= alignof(platform),\
  "The alignment of `" #sirius "` is weaker than that of `" #platform "`")

// clang-format on

// --- utils_pretty_fn ---
#undef utils_pretty_fn

#ifdef __cplusplus
#  if defined(_MSC_VER)
/**
 * @note For MSVC, this can be treated as a macro string.
 */
#    define utils_pretty_fn __FUNCSIG__
#  elif defined(__GNUC__) || defined(__clang__)
/**
 * @note For gcc/clang, this cannot be treated as a macro string.
 */
#    define utils_pretty_fn __PRETTY_FUNCTION__
#  else
#    define utils_pretty_fn __func__
#  endif
#else
#  define utils_pretty_fn __func__
#endif
