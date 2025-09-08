/**
 * @name sirius_macro.h
 *
 * @author UsadaYu
 *
 * @date
 * Create: 2024-07-30
 * Update: 2025-07-10
 *
 * @brief Common macro definitions.
 */

#ifndef SIRIUS_MACRO_H
#define SIRIUS_MACRO_H

/**
 * Timeout, no waiting.
 */
#define sirius_timeout_none (0)

/**
 * Timeout, infinite wait.
 */
#define sirius_timeout_infinite (~0U)

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
#define offsetof(TYPE, MEMBER) ((size_t)&((TYPE *)0)->MEMBER)

/**
 * @brief Get the struct head address from the struct member.
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

#if defined(__GNUC__) || defined(__clang__)
/**
 * @brief Struct pointer variable aligned to 8 bytes.
 *
 * @param N: The name of the pointer variable.
 */
#define sirius_pointer_align8(N) unsigned char unused##N[8 - sizeof(void *)];

#else
#define sirius_pointer_align8(N)
#endif

#endif  // SIRIUS_MACRO_H
