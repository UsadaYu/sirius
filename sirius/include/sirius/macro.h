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

#ifdef offsetof
#  undef offsetof
#endif
#ifdef container_of
#  undef container_of
#endif

#define offsetof(TYPE, MEMBER) ((size_t)&((TYPE *)0)->MEMBER)

#ifdef __cplusplus
#  define container_of(ptr, type, member) \
    reinterpret_cast<type *>(reinterpret_cast<char *>(const_cast<void *>( \
                               static_cast<const void *>(ptr))) - \
                             offsetof(type, member))

#else
#  define container_of(ptr, type, member) \
    ({ \
      const typeof(((type *)0)->member) *__mptr = (ptr); \
      (type *)((char *)__mptr - offsetof(type, member)); \
    })

#endif
