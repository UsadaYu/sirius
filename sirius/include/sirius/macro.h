#pragma once

#ifdef __cplusplus
#  include <cstddef>
#  include <cstdint>
#  include <type_traits>
#else
#  include <stdint.h>
#endif

#ifdef __cplusplus
static constexpr uint64_t kSsTimeoutNoWaiting = 0;
static constexpr uint64_t kSsTimeoutInfinite = UINT64_MAX;
#else
enum SsTimeout {
  kSsTimeoutNoWaiting = 0,
  kSsTimeoutInfinite = 0xffffffffffffffffULL,
};
#endif

// --- ss_container_of ---
#ifndef __cplusplus
#  undef ss_offsetof
#  undef ss_container_of

#  define ss_offsetof(TYPE, MEMBER) ((size_t)&((TYPE *)0)->MEMBER)
#  define ss_container_of(ptr, type, member) \
    ({ \
      const typeof(((type *)0)->member) *__mptr = (ptr); \
      (type *)((char *)__mptr - ss_offsetof(type, member)); \
    })
#endif
