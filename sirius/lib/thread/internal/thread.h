#pragma once

#include "sirius/thread/spinlock.h"
#include "sirius/thread/thread.h"
#include "utils/decls.h"
#include "utils/errno.h"
#include "utils/log/log.h"

#if defined(_WIN32) || defined(_WIN64)
#  include <process.h>
#endif

#if defined(_WIN32) || defined(_WIN64)

struct sirius_thread_s {
  HANDLE handle;
  uint64_t thread_id;
  void *retval;

  sirius_spinlock_t spin;
  unsigned int detached: 1;

  bool finished;
  bool resource_is_free;
};

typedef struct {
  void *(*start_routine)(void *);
  void *arg;
  sirius_thread_t thr;
} thread_wrapper_arg_s;

#endif

#if defined(_WIN32) || defined(_WIN64)

#  ifdef __cplusplus
extern "C" {
#  endif

unsigned __stdcall win_thread_wrapper(void *pv);
void win_mark_detach(sirius_thread_t thr);

#  ifdef __cplusplus
}
#  endif

#endif
