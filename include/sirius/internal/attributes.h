#ifndef SIRIUS_INTERNAL_ATTRIBUTES_H
#define SIRIUS_INTERNAL_ATTRIBUTES_H

#if defined(__GNUC__)
#  define sirius_gcc_version_check_at_least(x, y) \
    (__GNUC__ > (x) || __GNUC__ == (x) && __GNUC_MINOR__ >= (y))
#  define sirius_gcc_version_check_at_most(x, y) \
    (__GNUC__ < (x) || __GNUC__ == (x) && __GNUC_MINOR__ <= (y))
#else
#  define sirius_gcc_version_check_at_least(x, y) 0
#  define sirius_gcc_version_check_at_most(x, y) 0
#endif // __GNUC__

#if defined(__clang__)
#  define sirius_clang_version_check_at_least(x, y) \
    (__clang_major__ > (x) || __clang_major__ == (x) && __clang_minor__ >= (y))
#  define sirius_clang_version_check_at_most(x, y) \
    (__clang_major__ < (x) || __clang_major__ == (x) && __clang_minor__ <= (y))
#else
#  define sirius_clang_version_check_at_least(x, y) 0
#  define sirius_clang_version_check_at_most(x, y) 0
#endif // __clang__

#if defined(_WIN32) || defined(_WIN64)
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
#    if sirius_gcc_version_check_at_least(4, 0) || defined(__clang__)
#      define sirius_api __attribute__((visibility("default")))
#    else
#      define sirius_api
#    endif
#  endif
#endif

#endif // SIRIUS_INTERNAL_ATTRIBUTES_H
