#pragma once

#include "sirius/inner/attributes.h"

// --- ss_force_inline ---
#ifndef ss_force_inline
#  if _SS_INNER_GCC_VERSION_CHECK_AT_LEAST(3, 4) || defined(__clang__)
#    define ss_force_inline inline __attribute__((always_inline))
#  elif defined(_MSC_VER)
#    define ss_force_inline __forceinline
#  else
#    define ss_force_inline inline
#  endif
#endif

//  --- ss_weak_symbol ---
#ifndef ss_weak_symbol
#  if _SS_INNER_GCC_VERSION_CHECK_AT_LEAST(3, 0) || \
    _SS_INNER_CLANG_VERSION_CHECK_AT_LEAST(3, 0)
#    define ss_weak_symbol __attribute__((weak))
#  elif defined(_MSC_VER)
#    define ss_weak_symbol __declspec(selectany)
#  else
#    define ss_weak_symbol
#  endif
#endif

// --- ss_likely, ss_unlikely ---
#if defined(__GNUC__) || defined(__clang__)
#  ifndef ss_likely
#    define ss_likely(x) __builtin_expect(!!(x), 1)
#  endif
#  ifndef ss_unlikely
#    define ss_unlikely(x) __builtin_expect(!!(x), 0)
#  endif
#else
#  ifndef ss_likely
#    define ss_likely(x) !!(x)
#  endif
#  ifndef ss_unlikely
#    define ss_unlikely(x) !!(x)
#  endif
#endif

// --- ss_alignas ---
#undef ss_alignas

#if defined(__cplusplus)
#  if __cplusplus >= 201103L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201103L)
#    define ss_alignas(align) alignas(align)
#  elif defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER)
#    define ss_alignas(align) __attribute__((aligned(align)))
#  elif defined(_MSC_VER)
#    define ss_alignas(align) __declspec(align(align))
#  else
#    warning "Alignment not supported, may cause performance issues"
#    define ss_alignas(align) ((void)0)
#  endif
#else
#  if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L && \
    defined(__has_include) && __has_include(<stdalign.h>)
#    include <stdalign.h>
#    define ss_alignas(align) alignas(align)
#  elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && \
    defined(__has_include) && __has_include(<stdalign.h>)
#    include <stdalign.h>
#    define ss_alignas(align) _Alignas(align)
#  elif defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER)
#    define ss_alignas(align) __attribute__((aligned(align)))
#  elif defined(_MSC_VER)
#    define ss_alignas(align) __declspec(align(align))
#  else
#    warning "Alignment not supported, may cause performance issues"
#    define ss_alignas(align) ((void)0)
#  endif
#endif

// --- ss_static_assert ---
#undef ss_static_assert

#if defined(__cplusplus)
#  if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#    define ss_static_assert(cond, ...) static_assert((cond), ##__VA_ARGS__)
#  elif __cplusplus >= 201103L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201103L)
#    define ss_static_assert(cond, msg) static_assert(cond, msg)
#  else
#    define ss_static_assert(cond, ...) ((void)0)
#  endif
#else
#  if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#    define ss_static_assert(cond, ...) _Static_assert((cond), ##__VA_ARGS__)
#  elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#    define ss_static_assert(cond, msg) _Static_assert(cond, msg)
#  elif (defined(__GNUC__) || defined(__clang__)) && !defined(__STRICT_ANSI__)
#    define ss_static_assert(cond, msg) __extension__ _Static_assert(cond, msg)
#  else
#    define ss_static_assert(cond, ...) ((void)0)
#  endif
#endif

// --- ss_maybe_unused ---
#ifndef ss_maybe_unused
#  if defined(__cplusplus)
#    if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#      define ss_maybe_unused [[maybe_unused]]
#    elif defined(__GNUC__) || defined(__clang__)
#      define ss_maybe_unused [[gnu::unused]]
#    elif defined(_MSC_VER)
#      define ss_maybe_unused __pragma(warning(suppress: 4100))
#    else
#      define ss_maybe_unused
#    endif
#  else
#    if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#      define ss_maybe_unused [[maybe_unused]]
#    elif defined(__GNUC__) || defined(__clang__)
#      define ss_maybe_unused __attribute__((unused))
#    elif defined(_MSC_VER)
#      define ss_maybe_unused __pragma(warning(suppress: 4100))
#    else
#      define ss_maybe_unused
#    endif
#  endif
#endif

// ---
