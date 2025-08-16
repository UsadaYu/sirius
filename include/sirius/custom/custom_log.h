#ifndef CUSTOM_LOG_H
#define CUSTOM_LOG_H

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline const char *_custom_basename(
    const char *path) {
  const char *slash = strrchr(path, '/');
  const char *backslash = strrchr(path, '\\');
  const char *f = slash > backslash ? slash : backslash;
  return f ? f + 1 : path;
}

#ifdef __cplusplus
}
#endif

#endif  // CUSTOM_LOG_H
