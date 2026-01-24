#pragma once

#include "sirius/thread/mutex.h"
#include "utils/attributes.h"
#include "utils/decls.h"

#if defined(_WIN32) || defined(_WIN64)
typedef struct {
  sirius_mutex_type_t type;
  union {
    SRWLOCK srw_lock;
    CRITICAL_SECTION critical_section;
  } handle;
} sirius_mutex_s;

internal_check_sizeof(sirius_mutex_t, sirius_mutex_s);
internal_check_alignof(sirius_mutex_t, sirius_mutex_s);
#else
internal_check_sizeof(sirius_mutex_t, pthread_mutex_t);
internal_check_alignof(sirius_mutex_t, pthread_mutex_t);
#endif
