#ifndef __CUSTOM_LOG_H__
#define __CUSTOM_LOG_H__

#include <string.h>

#include "sirius/sirius_attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

static force_inline const char *_custom_basename(
    const char *path) {
  const char *slash = strrchr(path, '/');
  const char *backslash = strrchr(path, '\\');
  const char *f = slash > backslash ? slash : backslash;
  return f ? f + 1 : path;
}

#ifdef __cplusplus
}
#endif

#endif  // __CUSTOM_LOG_H__
