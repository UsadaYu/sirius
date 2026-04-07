#pragma once
/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

#include "sirius/thread/mutex.h"
#include "utils/attributes.h"

#if defined(_WIN32) || defined(_WIN64)
typedef struct {
  enum SsMutexType type;
  union {
    SRWLOCK srw_lock;
    CRITICAL_SECTION critical_section;
  } handle;
} ss_mutex_s;

utils_check_sizeof(ss_mutex_t, ss_mutex_s);
utils_check_alignof(ss_mutex_t, ss_mutex_s);
#else
utils_check_sizeof(ss_mutex_t, pthread_mutex_t);
utils_check_alignof(ss_mutex_t, pthread_mutex_t);
#endif
