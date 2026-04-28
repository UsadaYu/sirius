#pragma once

#include "sirius/inner/attributes.h"

#if defined(_WIN32) || defined(_WIN64)
#  ifndef FOUNDATION_API
#    if defined(_SIRIUS_BUILDING) && defined(_SIRIUS_FOUNDATION_BUILDING)
#      ifdef _SIRIUS_WIN_DLL
#        define FOUNDATION_API __declspec(dllexport)
#      else
#        define FOUNDATION_API
#      endif
#    else
#      ifdef _SIRIUS_WIN_DLL
#        define FOUNDATION_API __declspec(dllimport)
#      else
#        define FOUNDATION_API
#      endif
#    endif
#  endif
#else
#  ifndef FOUNDATION_API
#    if _SS_INNER_GCC_VERSION_CHECK_AT_LEAST(4, 0) || defined(__clang__)
#      define FOUNDATION_API __attribute__((visibility("default")))
#    else
#      define FOUNDATION_API
#    endif
#  endif
#endif
