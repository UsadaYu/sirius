#ifndef SIRIUS_MACRO_H
#define SIRIUS_MACRO_H

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
#define sirius_timeout_none (0)

/**
 * @brief Timeout, infinite wait.
 */
#define sirius_timeout_infinite (UINT64_MAX)

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

#if defined(__GNUC__) || defined(__clang__)
/**
 * @brief Struct pointer variable aligned to 8 bytes.
 *
 * @param N The name of the pointer variable.
 */
#  define sirius_pointer_align8(N) unsigned char unused##N[8 - sizeof(void *)];

#else
#  define sirius_pointer_align8(N)
#endif

#endif // SIRIUS_MACRO_H
