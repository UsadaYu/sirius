#ifndef SIRIUS_INTERNAL_THREAD_H
#define SIRIUS_INTERNAL_THREAD_H

#include "sirius/internal/common.h"
#include "sirius/sirius_attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

sirius_api uint64_t sirius_internal_get_tid();

#ifdef __cplusplus
}
#endif

static inline uint64_t sirius_internal_thread_id() {
#if defined(__cplusplus)
  static thread_local uint64_t cache_tid = 0;
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
  static _Thread_local uint64_t cache_tid = 0;
#elif defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)
  static __declspec(thread) uint64_t cache_tid = 0;
#else
#  error "Thread local storage not supported"
#endif

  if (unlikely(cache_tid == 0)) {
    cache_tid = sirius_internal_get_tid();
  }
  return cache_tid;
}

#endif // SIRIUS_INTERNAL_THREAD_H
