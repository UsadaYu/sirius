#ifndef SIRIUS_CUSTOM_THREAD_H
#define SIRIUS_CUSTOM_THREAD_H

#include "sirius/sirius_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
  defined(__NetBSD__)
#  include <sys/syscall.h>
#  include <unistd.h>
static inline uint64_t _custom_thread_id() {
  return (uint64_t)syscall(SYS_gettid);
}
#elif defined(_WIN32)
static inline uint64_t _custom_thread_id() {
  return (uint64_t)GetCurrentThreadId();
}
#elif defined(__APPLE__) && defined(__MACH__)
static inline uint64_t _custom_thread_id() {
  uint64_t thread_id;
  if (unlikely(pthread_threadid_np(NULL, &thread_id) != 0))
    return 0;
  return thread_id;
}
#elif defined(sun) || defined(__sun)
static inline uint64_t _custom_thread_id() {
  return (uint64_t)thr_self();
}
#else
#  define _custom_thread_id() 0
#endif

#ifdef __cplusplus
}
#endif

#endif // SIRIUS_CUSTOM_THREAD_H
