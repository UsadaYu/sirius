#ifndef SIRIUS_FILE_H
#define SIRIUS_FILE_H

#if (defined(__cplusplus) && __cplusplus >= 202002L) || \
  (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L) || \
  (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L)

#  if defined(__FILE_NAME__)
#    define sirius_file_name (__FILE_NAME__)
#  endif

#endif

#ifndef sirius_file_name
#  if defined(__has_builtin)
#    if __has_builtin(__builtin_FILE_NAME)
#      define sirius_file_name (__builtin_FILE_NAME())
#    endif
#  endif
#endif

#ifndef sirius_file_name
#  include <string.h>

static inline const char *sirius_internal_basename(const char *path) {
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

#  define sirius_file_name (sirius_internal_basename(__FILE__))
#endif

#endif // SIRIUS_FILE_H
