#ifndef __TEST_H__
#define __TEST_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sirius_log.h"
#include "sirius_time.h"

#ifdef _WIN32
#include <Windows.h>
#include <io.h>
#define t_dprintf(fd, ...)                      \
  do {                                          \
    char msg[1024];                             \
    snprintf(msg, sizeof(msg), ##__VA_ARGS__);  \
    _write(fd, msg, (unsigned int)strlen(msg)); \
  } while (0)
#else
#define t_dprintf dprintf
#endif

#define t_assert(expr)                     \
  do {                                     \
    if (unlikely(!(expr))) {               \
      sirius_error("assert: %s\n", #expr); \
      abort();                             \
    }                                      \
  } while (0)

#ifdef __cplusplus
}
#endif

#endif  // __TEST_H__
