/**
 * @name sirius_attributes.h
 * 
 * @author 胡益华
 * 
 * @date 2024-09-25
 * 
 * @brief attribute class macro definition
 */

#ifndef __SIRIUS_ATTRIBUTES_H__
#define __SIRIUS_ATTRIBUTES_H__

/**
 * @brief compiler version checking
 */
/* gcc */
#if defined(__GNUC__)
#define gcc_version_check_at_least(x,y) \
    (__GNUC__ > (x) || \
    __GNUC__ == (x) && __GNUC_MINOR__ >= (y))
#define gcc_version_check_at_most(x,y) \
    (__GNUC__ < (x) || \
    __GNUC__ == (x) && __GNUC_MINOR__ <= (y))
#else
#define gcc_version_check_at_least(x,y) 0
#define gcc_version_check_at_most(x,y)  0
#endif // __GNUC__

/* clang */
#if defined(__clang__)
#define clang_version_check_at_least(x,y) \
    (__clang_major__ > (x) || \
    __clang_major__ == (x) && __clang_minor__ >= (y))
#define clang_version_check_at_most(x,y) \
    (__clang_major__ < (x) || \
    __clang_major__ == (x) && __clang_minor__ <= (y))
#else
#define clang_version_check_at_least(x,y) 0
#define clang_version_check_at_most(x,y)  0
#endif // __clang__

/**
 * @brief force inline
 */
#ifndef force_inline
#if gcc_version_check_at_least(3, 4) || \
    defined(__clang__)
#define force_inline \
inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define force_inline __forceinline
#else
#define force_inline inline
#endif
#endif // force_inline

/**
 * @brief function if symbol
 */
#ifndef weak_symbol
#if gcc_version_check_at_least(3, 0) || \
    clang_version_check_at_least(3, 0)
#define weak_symbol __attribute__((weak))
#elif defined(_MSC_VER)
#define weak_symbol __declspec(selectany)
#else
#define weak_symbol
#endif
#endif // weak_symbol

/**
 * @brief symbol hiding and exporting
 */
#if gcc_version_check_at_least(3, 4) || \
    defined(__clang__)
#ifndef export_symbol
#define export_symbol __attribute__((visibility("default")))
#endif
#ifndef hide_symbol
#define hide_symbol __attribute__((visibility("hidden")))
#endif
#elif defined(_MSC_VER)
#ifndef export_symbol
#define export_symbol __declspec(dllexport)
#endif
#ifndef hide_symbol
#define hide_symbol __declspec(dllimport)
#endif
#else
#ifndef export_symbol
#define export_symbol
#endif
#ifndef hide_symbol
#define hide_symbol
#endif
#endif // compiler

/**
 * @brief the probability of selecting a branch is high
 */
#if defined(__GNUC__) || defined (__clang__)
#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif
#else
#ifndef likely
#define likely(x)   !!(x)
#endif
#ifndef unlikely
#define unlikely(x) !!(x)
#endif
#endif // compiler


#endif // __SIRIUS_ATTRIBUTES_H__
