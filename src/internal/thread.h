#ifndef INTERNAL_THREAD_H
#define INTERNAL_THREAD_H

#include "internal/decls.h"
#include "internal/errno.h"
#include "internal/log.h"
#include "sirius/sirius_spinlock.h"
#include "sirius/sirius_thread.h"

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

#endif // INTERNAL_THREAD_H
