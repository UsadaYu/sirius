#pragma once

#include "sirius/internal/attributes.h"

// --- force_inline ---
#ifndef force_inline
#  if _sirius_gcc_version_check_at_least(3, 4) || defined(__clang__)
#    define force_inline inline __attribute__((always_inline))
#  elif defined(_MSC_VER)
#    define force_inline __forceinline
#  else
#    define force_inline inline
#  endif
#endif

//  --- weak_symbol ---
#ifndef weak_symbol
#  if _sirius_gcc_version_check_at_least(3, 0) || \
    _sirius_clang_version_check_at_least(3, 0)
#    define weak_symbol __attribute__((weak))
#  elif defined(_MSC_VER)
#    define weak_symbol __declspec(selectany)
#  else
#    define weak_symbol
#  endif
#endif

// --- likely, unlikely ---
#if defined(__GNUC__) || defined(__clang__)
#  ifndef likely
#    define likely(x) __builtin_expect(!!(x), 1)
#  endif
#  ifndef unlikely
#    define unlikely(x) __builtin_expect(!!(x), 0)
#  endif
#else
#  ifndef likely
#    define likely(x) !!(x)
#  endif
#  ifndef unlikely
#    define unlikely(x) !!(x)
#  endif
#endif

// --- sirius_alignas ---
#undef sirius_alignas

#if defined(__cplusplus)
#  if __cplusplus >= 201103L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201103L)
#    define sirius_alignas(align) alignas(align)
#  elif defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER)
#    define sirius_alignas(align) __attribute__((aligned(align)))
#  elif defined(_MSC_VER)
#    define sirius_alignas(align) __declspec(align(align))
#  else
#    warning "Alignment not supported, may cause performance issues"
#    define sirius_alignas(align) ((void)0)
#  endif
#else
#  if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L && \
    defined(__has_include) && __has_include(<stdalign.h>)
#    include <stdalign.h>
#    define sirius_alignas(align) alignas(align)
#  elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && \
    defined(__has_include) && __has_include(<stdalign.h>)
#    include <stdalign.h>
#    define sirius_alignas(align) _Alignas(align)
#  elif defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER)
#    define sirius_alignas(align) __attribute__((aligned(align)))
#  elif defined(_MSC_VER)
#    define sirius_alignas(align) __declspec(align(align))
#  else
#    warning "Alignment not supported, may cause performance issues"
#    define sirius_alignas(align) ((void)0)
#  endif
#endif

// --- sirius_static_assert ---
#undef sirius_static_assert

#if defined(__cplusplus)
#  if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#    define sirius_static_assert(cond, ...) static_assert((cond), ##__VA_ARGS__)
#  elif __cplusplus >= 201103L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201103L)
#    define sirius_static_assert(cond, msg) static_assert(cond, msg)
#  else
#    define sirius_static_assert(cond, ...) ((void)0)
#  endif
#else
#  if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#    define sirius_static_assert(cond, ...) \
      _Static_assert((cond), ##__VA_ARGS__)
#  elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#    define sirius_static_assert(cond, msg) _Static_assert(cond, msg)
#  elif (defined(__GNUC__) || defined(__clang__)) && !defined(__STRICT_ANSI__)
#    define sirius_static_assert(cond, msg) \
      __extension__ _Static_assert(cond, msg)
#  else
#    define sirius_static_assert(cond, ...) ((void)0)
#  endif
#endif

// --- sirius_maybe_unused ---
#ifndef sirius_maybe_unused
#  if defined(__cplusplus)
#    if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#      define sirius_maybe_unused [[maybe_unused]]
#    elif defined(__GNUC__) || defined(__clang__)
#      define sirius_maybe_unused [[gnu::unused]]
#    elif defined(_MSC_VER)
#      define sirius_maybe_unused __pragma(warning(suppress: 4100))
#    else
#      define sirius_maybe_unused
#    endif
#  else
#    if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#      define sirius_maybe_unused [[maybe_unused]]
#    elif defined(__GNUC__) || defined(__clang__)
#      define sirius_maybe_unused __attribute__((unused))
#    elif defined(_MSC_VER)
#      define sirius_maybe_unused __pragma(warning(suppress: 4100))
#    else
#      define sirius_maybe_unused
#    endif
#  endif
#endif

// ---
