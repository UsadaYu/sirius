#pragma once

#if (defined(__cplusplus) && __cplusplus >= 202002L) || \
  (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L) || \
  (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L)
#  if defined(__FILE_NAME__)
#    define SIRIUS_FILE_NAME (__FILE_NAME__)
#  endif
#endif

#ifndef SIRIUS_FILE_NAME
#  if defined(__has_builtin)
#    if __has_builtin(__builtin_FILE_NAME)
#      define SIRIUS_FILE_NAME (__builtin_FILE_NAME())
#    endif
#  endif
#endif

#ifndef SIRIUS_FILE_NAME
#  include <string.h>

static inline const char *_sirius_basename(const char *path) {
  const char *p = path;
  const char *last = path;

  while (*p) {
    if (*p == '/'
#  if defined(_WIN32) || defined(_WIN64)
        || *p == '\\'
#  endif
    ) {
      last = p + 1;
    }
    p++;
  }
  return last;
}

#  define SIRIUS_FILE_NAME (_sirius_basename(__FILE__))
#endif
