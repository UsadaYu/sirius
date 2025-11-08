#ifndef SIRIUS_ATTRIBUTES_H
#define SIRIUS_ATTRIBUTES_H

/**
 * @brief Compiler version checking.
 */
#if defined(__GNUC__)
#  define gcc_version_check_at_least(x, y) \
    (__GNUC__ > (x) || __GNUC__ == (x) && __GNUC_MINOR__ >= (y))
#  define gcc_version_check_at_most(x, y) \
    (__GNUC__ < (x) || __GNUC__ == (x) && __GNUC_MINOR__ <= (y))
#else
#  define gcc_version_check_at_least(x, y) 0
#  define gcc_version_check_at_most(x, y) 0
#endif // __GNUC__

#if defined(__clang__)
#  define clang_version_check_at_least(x, y) \
    (__clang_major__ > (x) || __clang_major__ == (x) && __clang_minor__ >= (y))
#  define clang_version_check_at_most(x, y) \
    (__clang_major__ < (x) || __clang_major__ == (x) && __clang_minor__ <= (y))
#else
#  define clang_version_check_at_least(x, y) 0
#  define clang_version_check_at_most(x, y) 0
#endif // __clang__

#ifndef force_inline
#  if gcc_version_check_at_least(3, 4) || defined(__clang__)
#    define force_inline inline __attribute__((always_inline))
#  elif defined(_MSC_VER)
#    define force_inline __forceinline
#  else
#    define force_inline inline
#  endif
#endif // force_inline

#ifndef weak_symbol
#  if gcc_version_check_at_least(3, 0) || clang_version_check_at_least(3, 0)
#    define weak_symbol __attribute__((weak))
#  elif defined(_MSC_VER)
#    define weak_symbol __declspec(selectany)
#  else
#    define weak_symbol
#  endif
#endif // weak_symbol

#ifdef _WIN32
#  ifndef sirius_api
#    ifdef SIRIUS_BUILDING
#      ifdef SIRIUS_WIN_DLL
#        define sirius_api __declspec(dllexport)
#      else
#        define sirius_api
#      endif
#    else
#      ifdef SIRIUS_WIN_DLL
#        define sirius_api __declspec(dllimport)
#      else
#        define sirius_api
#      endif
#    endif
#  endif
#else
#  ifndef sirius_api
#    if gcc_version_check_at_least(4, 0) || defined(__clang__)
#      define sirius_api __attribute__((visibility("default")))
#    else
#      define sirius_api
#    endif
#  endif
#endif

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

#if gcc_version_check_at_least(3, 4)
#  define sirius_warn_unused_result __attribute__((warn_unused_result))
#else
#  define sirius_warn_unused_result
#endif

#endif // SIRIUS_ATTRIBUTES_H
