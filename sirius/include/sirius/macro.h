#pragma once

#ifdef __cplusplus
#  include <cstddef>
#  include <cstdint>
#  include <type_traits>
#else
#  include <stdint.h>
#endif

/**
 * @brief Timeout, no waiting.
 */
#define SIRIUS_TIMEOUT_NO_WAITING (0)

/**
 * @brief Timeout, infinite wait.
 */
#define SIRIUS_TIMEOUT_INFINITE (UINT64_MAX)

// --- container_of ---
#ifndef __cplusplus
#  undef offsetof
#  undef container_of

#  define offsetof(TYPE, MEMBER) ((size_t)&((TYPE *)0)->MEMBER)
#  define container_of(ptr, type, member) \
    ({ \
      const typeof(((type *)0)->member) *__mptr = (ptr); \
      (type *)((char *)__mptr - offsetof(type, member)); \
    })
#endif
