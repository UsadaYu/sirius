#pragma once

#include "sirius/attributes.h"
#include "sirius/inner/common.h"

#ifdef __cplusplus
extern "C" {
#endif

sirius_api uint64_t _ss_inner_get_tid();

#ifdef __cplusplus
}
#endif

static inline uint64_t ss_thread_id() {
#if defined(__cplusplus)
  static thread_local uint64_t cache_tid = 0;
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
  static _Thread_local uint64_t cache_tid = 0;
#elif defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)
  static __declspec(thread) uint64_t cache_tid = 0;
#else
#  error "Thread local storage not supported"
#endif

  if (ss_unlikely(cache_tid == 0)) {
    cache_tid = _ss_inner_get_tid();
  }
  return cache_tid;
}
