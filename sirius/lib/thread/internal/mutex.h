#pragma once

#include "sirius/thread/mutex.h"
#include "utils/attributes.h"
#include "utils/decls.h"

#if defined(_WIN32) || defined(_WIN64)
typedef struct {
  enum SiriusMutexType type;
  union {
    SRWLOCK srw_lock;
    CRITICAL_SECTION critical_section;
  } handle;
} sirius_mutex_s;

utils_check_sizeof(sirius_mutex_t, sirius_mutex_s);
utils_check_alignof(sirius_mutex_t, sirius_mutex_s);
#else
utils_check_sizeof(sirius_mutex_t, pthread_mutex_t);
utils_check_alignof(sirius_mutex_t, pthread_mutex_t);
#endif
