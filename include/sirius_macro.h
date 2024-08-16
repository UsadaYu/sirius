/**
 * @name sirius_macro.h
 *
 * @author UsadaYu
 *
 * @date create: 2024-07-30
 * @date update: 2025-02-17
 *
 * @brief Common macro definitions.
 */

#ifndef __SIRIUS_MACRO_H__
#define __SIRIUS_MACRO_H__

#ifndef sirius_timeout_none
/* Timeout, no waiting. */
#define sirius_timeout_none (0)
#else
#undef sirius_timeout_none
#define sirius_timeout_none (0)
#endif

#ifndef sirius_timeout_infinite
/* Timeout, infinite wait. */
#define sirius_timeout_infinite (~0U)
#else
#undef sirius_timeout_infinite
#define sirius_timeout_infinite (~0U)
#endif

#ifndef container_of
#ifdef offsetof
#undef offsetof
#endif
/**
 * @brief Get the address offset of the struct member.
 *
 * @param TYPE: Struct type.
 * @param MEMBER: Struct member.
 */
#define offsetof(TYPE, MEMBER) \
  ((size_t)&((TYPE *)0)->MEMBER)

/**
 * @brief Get the struct head address from the struct
 *  member.
 *
 * @param ptr: The address of the struct member.
 * @param type: Struct type.
 * @param member: Struct member name.
 */
#define container_of(ptr, type, member)                \
  ({                                                   \
    const typeof(((type *)0)->member) *__mptr = (ptr); \
    (type *)((char *)__mptr - offsetof(type, member)); \
  })
#endif  // container_of

#ifndef sirius_pointer_align8
#if defined(__GNUC__) || defined(__clang__)
/**
 * @brief Struct pointer variable aligned to 8 bytes.
 *
 * @param N: The name of the pointer variable.
 */
#define sirius_pointer_align8(N) \
  unsigned char unused##N[8 - sizeof(void *)];

#else
#define sirius_pointer_align8(N)
#endif
#endif  // sirius_pointer_align8

#endif  // __SIRIUS_MACRO_H__
