#ifndef SIRIUS_INTERNAL_LOG_H
#define SIRIUS_INTERNAL_LOG_H

#include <string.h>

#include "sirius/sirius_common.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline const char *sirius_internal_basename(const char *path) {
  const char *slash = strrchr(path, '/');
  const char *backslash = strrchr(path, '\\');
  const char *f = slash > backslash ? slash : backslash;
  return f ? f + 1 : path;
}

#ifdef __cplusplus
}
#endif

#endif // SIRIUS_INTERNAL_LOG_H
