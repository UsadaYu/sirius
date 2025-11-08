#include "sirius/sirius_thread.h"

#include "sirius/custom/errno.h"
#include "sirius/internal/decls.h"
#include "sirius/internal/log.h"
#include "sirius/sirius_math.h"

#define WIN_THREAD_PRIORITY_MAX (31)
#define PRIORITY_MAP_RATIO \
  ((double)sirius_thread_priority_max / WIN_THREAD_PRIORITY_MAX)

#ifdef _WIN32
static int win_set_thread_priority(sirius_thread_t thread, int posix_priority) {
  int win_priority = -1;
  DWORD dw_err;

#  define E(r) ((int)((r) * PRIORITY_MAP_RATIO))
#  define C(idle, lowest, below_normal, normal, above_normal, highest) \
    if (posix_priority <= E(idle)) { \
      win_priority = THREAD_PRIORITY_IDLE; \
      break; \
    } \
    if (posix_priority <= E(lowest)) { \
      win_priority = THREAD_PRIORITY_LOWEST; \
      break; \
    } \
    if (posix_priority <= E(below_normal)) { \
      win_priority = THREAD_PRIORITY_BELOW_NORMAL; \
      break; \
    } \
    if (posix_priority <= E(normal)) { \
      win_priority = THREAD_PRIORITY_NORMAL; \
      break; \
    } \
    if (posix_priority <= E(above_normal)) { \
      win_priority = THREAD_PRIORITY_ABOVE_NORMAL; \
      break; \
    } \
    if (posix_priority <= E(highest)) { \
      win_priority = THREAD_PRIORITY_HIGHEST; \
      break; \
    } \
    win_priority = THREAD_PRIORITY_TIME_CRITICAL; \
    break;

  switch (GetPriorityClass(GetCurrentProcess())) {
  case IDLE_PRIORITY_CLASS:
    C(1, 2, 3, 4, 5, 6)
  case BELOW_NORMAL_PRIORITY_CLASS:
    C(1, 4, 5, 6, 7, 8)
  case NORMAL_PRIORITY_CLASS:
    C(1, 6, 7, 8, 9, 10)
  case ABOVE_NORMAL_PRIORITY_CLASS:
    C(1, 8, 9, 10, 11, 12)
  case HIGH_PRIORITY_CLASS:
    C(1, 11, 12, 13, 14, 15)
  case REALTIME_PRIORITY_CLASS:
    C(16, 22, 23, 24, 25, 26)
  default:
    dw_err = GetLastError();
    internal_win_fmt_error(dw_err, "GetPriorityClass");
    return sirius_custom_win32err_to_errno(dw_err);
  }

  if (!SetThreadPriority(thread, win_priority)) {
    dw_err = GetLastError();
    internal_win_fmt_error(dw_err, "SetThreadPriority");
    return sirius_custom_win32err_to_errno(dw_err);
  }

  return 0;
#  undef C
#  undef E
}

#else
static int posix_sched_policy_sirius_to_posix(int *dst, int src) {
  switch (src) {
  case sirius_thread_sched_other:
    *dst = SCHED_OTHER;
    return 0;
  case sirius_thread_sched_fifo:
    *dst = SCHED_FIFO;
    return 0;
  case sirius_thread_sched_rr:
    *dst = SCHED_RR;
    return 0;
  default:
    break;
  }

  internal_warn("Invalid argument: [sirius sched: %d]\n", src);
  return EINVAL;
}

static int posix_sched_policy_posix_to_sirius(int *dst, int src) {
  switch (src) {
  case SCHED_OTHER:
    *dst = sirius_thread_sched_other;
    return 0;
  case SCHED_FIFO:
    *dst = sirius_thread_sched_fifo;
    return 0;
  case SCHED_RR:
    *dst = sirius_thread_sched_rr;
    return 0;
  default:
    break;
  }

  internal_warn("Invalid argument: [posix sched: %d]\n", src);
  return EINVAL;
}

static int posix_sched_check(int posix_policy) {
  if (unlikely(posix_policy != SCHED_OTHER && posix_policy != SCHED_FIFO &&
               posix_policy != SCHED_RR)) {
    internal_error("Invalid argument: [policy: %d]\n", posix_policy);
    return EINVAL;
  }

  return 0;
}

static inline int posix_get_thread_policy(sirius_thread_t thread,
                                          int *posix_policy) {
  struct sched_param thread_param = {0};

  int ret = pthread_getschedparam(thread, posix_policy, &thread_param);
  if (unlikely(ret)) {
    internal_str_error(ret, "pthread_getschedparam");
    return ret;
  }

  return posix_sched_check(*posix_policy);
}

#endif

sirius_api int sirius_thread_create(sirius_thread_t *__restrict thread,
                                    const sirius_thread_attr_t *__restrict attr,
                                    void *(*start_routine)(void *),
                                    void *__restrict arg) {
  if (unlikely(!thread) || unlikely(!start_routine)) {
    internal_error("Null pointer\n");
    return EINVAL;
  }

  size_t s_stack = attr ? ((attr->stacksize > 0) ? attr->stacksize : 0) : 0;

#ifdef _WIN32
  int ret;
  DWORD dw_err;
  DWORD thread_id;
  DWORD dw_creation_flags = 0;

  if (attr) {
    dw_creation_flags = CREATE_SUSPENDED;
  }

  *thread =
    CreateThread(nullptr, s_stack, (LPTHREAD_START_ROUTINE)start_routine, arg,
                 dw_creation_flags, &thread_id);
  if (!*thread) {
    dw_err = GetLastError();
    internal_win_fmt_error(dw_err, "CreateThread");
    return sirius_custom_win32err_to_errno(dw_err);
  }

  if (dw_creation_flags == CREATE_SUSPENDED) {
    ret = win_set_thread_priority(*thread, attr->sched_param.priority);
    if (ret) {
      CloseHandle(*thread);
      return ret;
    }
    if (-1 == ResumeThread(*thread)) {
      dw_err = GetLastError();
      internal_win_fmt_error(dw_err, "ResumeThread");
      CloseHandle(*thread);
      return sirius_custom_win32err_to_errno(dw_err);
    }
  }

  return 0;

#else
  int ret;
  pthread_attr_t thread_attr;
  pthread_attr_init(&thread_attr);

  if (attr) {
    int detach_state;
    if (attr->detach_state == sirius_thread_joinable) {
      detach_state = PTHREAD_CREATE_JOINABLE;
    } else if (attr->detach_state == sirius_thread_detached) {
      detach_state = PTHREAD_CREATE_DETACHED;
    } else {
      internal_error("Invalid arguments: [detach state: %d]\n",
                     attr->detach_state);
      ret = EINVAL;
      goto label_free;
    }
    if ((ret = pthread_attr_setdetachstate(&thread_attr, detach_state)) != 0) {
      internal_str_error(ret, "pthread_attr_setdetachstate");
      goto label_free;
    }

    ret = pthread_attr_setinheritsched(
      &thread_attr,
      attr->inherit_sched == sirius_thread_explicit_sched
        ? PTHREAD_EXPLICIT_SCHED
        : PTHREAD_INHERIT_SCHED);
    if (ret) {
      internal_str_error(ret, "pthread_attr_setinheritsched");
      goto label_free;
    }

    ret = pthread_attr_setscope(&thread_attr,
                                attr->scope == sirius_thread_scope_process
                                  ? PTHREAD_SCOPE_PROCESS
                                  : PTHREAD_SCOPE_SYSTEM);
    if (ret) {
      internal_str_error(ret, "pthread_attr_setscope");
      goto label_free;
    }

    if (attr->stackaddr && s_stack > 0) {
      if ((ret = pthread_attr_setstack(&thread_attr, attr->stackaddr,
                                       s_stack)) != 0) {
        internal_str_error(ret, "pthread_attr_setstack");
        goto label_free;
      }
    } else if (attr->stackaddr && s_stack <= 0) {
      internal_error("Invalid arguments: [stack size: %d]\n", s_stack);
      ret = EINVAL;
      goto label_free;
    } else if (!attr->stackaddr && s_stack > 0) {
      if ((ret = pthread_attr_setstacksize(&thread_attr, s_stack)) != 0) {
        internal_str_error(ret, "pthread_attr_setstacksize");
        goto label_free;
      }
    }

    if ((ret = pthread_attr_setguardsize(&thread_attr, attr->guardsize)) != 0) {
      internal_str_error(ret, "pthread_attr_setguardsize");
      goto label_free;
    }

    sirius_thread_sched_param_t ssp = attr->sched_param;
    struct sched_param psp;
    int policy;
    if ((ret = posix_sched_policy_sirius_to_posix(&policy, ssp.sched_policy)) !=
        0)
      goto label_free;
    if ((ret = pthread_attr_setschedpolicy(&thread_attr, policy)) != 0) {
      internal_str_error(ret, "pthread_attr_setschedpolicy");
      goto label_free;
    }
    psp.sched_priority = ssp.priority;
    if ((ret = pthread_attr_setschedparam(&thread_attr, &psp)) != 0) {
      internal_str_error(ret, "pthread_attr_setschedparam");
      goto label_free;
    }
  }

  ret = pthread_create(thread, &thread_attr, start_routine, arg);
  pthread_attr_destroy(&thread_attr);
  if (ret) {
    internal_str_error(ret, "pthread_create");
    return ret;
  }

  return 0;

label_free:
  pthread_attr_destroy(&thread_attr);
  return ret;

#endif
}

sirius_api int sirius_thread_join(sirius_thread_t thread, void **retval) {
#ifdef _WIN32
  (void)retval;
  DWORD dw_err = ERROR_SUCCESS;

  DWORD wait_ret = WaitForSingleObject(thread, INFINITE);
  switch (wait_ret) {
  case WAIT_OBJECT_0:
    break;
  case WAIT_TIMEOUT:
    (void)CloseHandle(thread);
    return ETIMEDOUT;
  case WAIT_FAILED:
    dw_err = GetLastError();
    (void)CloseHandle(thread);
    internal_win_fmt_error(dw_err, "WaitForSingleObject");
    return sirius_custom_win32err_to_errno(dw_err);
  case WAIT_ABANDONED:
    (void)CloseHandle(thread);
    return EINVAL;
  default:
    (void)CloseHandle(thread);
    return EINVAL;
  }

  if (!CloseHandle(thread)) {
    dw_err = GetLastError();
    internal_win_fmt_error(dw_err, "CloseHandle");
    return sirius_custom_win32err_to_errno(dw_err);
  }

  return 0;

#else
  int ret = pthread_join(thread, retval);
  if (unlikely(ret)) {
    internal_str_error(ret, "pthread_join");
    return ret;
  }
  return 0;

#endif
}

#ifndef _WIN32
sirius_api int sirius_thread_detach(sirius_thread_t thread) {
  int ret = pthread_detach(thread);
  if (unlikely(ret)) {
    internal_str_error(ret, "pthread_detach");
    return ret;
  }
  return 0;
}
#endif

sirius_api void sirius_thread_exit(
#ifdef _WIN32
  DWORD *
#else
  void *
#endif
    retval) {
#ifdef _WIN32
  DWORD e_code = retval ? *retval : 0;
  ExitThread(e_code);

#else
  pthread_exit(retval);

#endif
}

sirius_api int sirius_thread_cancel(sirius_thread_t thread) {
#ifdef _WIN32
  if (!TerminateThread(thread, (DWORD)-1)) {
    DWORD dw_err = GetLastError();
    internal_win_fmt_error(dw_err, "TerminateThread");
    return sirius_custom_win32err_to_errno(dw_err);
  }

#else
  int ret = pthread_cancel(thread);
  if (unlikely(ret)) {
    internal_str_error(ret, "pthread_cancel");
    return ret;
  }

#endif

  return 0;
}

sirius_api sirius_thread_t sirius_thread_self() {
#ifdef _WIN32
  return GetCurrentThread();

#else
  return pthread_self();

#endif
}

sirius_api int sirius_thread_get_priority_max(sirius_thread_t thread,
                                              int *priority) {
  if (!priority)
    return EINVAL;
  *priority = 0;

#ifdef _WIN32
  (void)thread;
  DWORD dw_err;

  switch (GetPriorityClass(GetCurrentProcess())) {
  case IDLE_PRIORITY_CLASS:
  case BELOW_NORMAL_PRIORITY_CLASS:
  case NORMAL_PRIORITY_CLASS:
  case ABOVE_NORMAL_PRIORITY_CLASS:
  case HIGH_PRIORITY_CLASS:
    *priority = (int)(PRIORITY_MAP_RATIO * 15);
    return 0;
  case REALTIME_PRIORITY_CLASS:
    *priority = sirius_thread_priority_max;
    return 0;
  default:
    dw_err = GetLastError();
    internal_win_fmt_error(dw_err, "GetPriorityClass");
    return sirius_custom_win32err_to_errno(dw_err);
  }

#else
  int policy;
  int ret = posix_get_thread_policy(thread, &policy);
  if (unlikely(ret))
    return ret;

  *priority = sched_get_priority_max(policy);
  return 0;

#endif
}

sirius_api int sirius_thread_get_priority_min(sirius_thread_t thread,
                                              int *priority) {
  if (!priority)
    return EINVAL;
  *priority = 0;

#ifdef _WIN32
  (void)thread;
  DWORD dw_err;

  switch (GetPriorityClass(GetCurrentProcess())) {
  case IDLE_PRIORITY_CLASS:
  case BELOW_NORMAL_PRIORITY_CLASS:
  case NORMAL_PRIORITY_CLASS:
  case ABOVE_NORMAL_PRIORITY_CLASS:
  case HIGH_PRIORITY_CLASS:
    *priority = sirius_thread_priority_min;
    return 0;
  case REALTIME_PRIORITY_CLASS:
    *priority = (int)(PRIORITY_MAP_RATIO * 16);
    return 0;
  default:
    dw_err = GetLastError();
    internal_win_fmt_error(dw_err, "GetPriorityClass");
    return sirius_custom_win32err_to_errno(dw_err);
  }

#else
  int policy;
  int ret = posix_get_thread_policy(thread, &policy);
  if (unlikely(ret))
    return ret;

  *priority = sched_get_priority_min(policy);
  return 0;

#endif
}

sirius_api int
sirius_thread_setschedparam(sirius_thread_t thread,
                            const sirius_thread_sched_param_t *param) {
  if (unlikely(!param)) {
    internal_error("Null pointer\n");
    return EINVAL;
  }

#ifdef _WIN32
  (void)param->sched_policy;
  return win_set_thread_priority(thread, param->priority);

#else
  int posix_sched_policy;
  int ret = posix_sched_policy_sirius_to_posix(&posix_sched_policy,
                                               (int)param->sched_policy);
  if (ret)
    return ret;

  struct sched_param sched_param = {0};
  sched_param.sched_priority = param->priority;
  ret = pthread_setschedparam(thread, posix_sched_policy, &sched_param);
  if (ret) {
    internal_str_error(ret, "pthread_setschedparam");
    return ret;
  }

  return 0;

#endif
}

sirius_api int sirius_thread_getschedparam(sirius_thread_t thread,
                                           sirius_thread_sched_param_t *param) {
  if (unlikely(!param)) {
    internal_error("Null pointer\n");
    return EINVAL;
  }

#ifdef _WIN32
  DWORD dw_err;

  int thread_priority = GetThreadPriority(thread);
  if (THREAD_PRIORITY_ERROR_RETURN == thread_priority) {
    dw_err = GetLastError();
    internal_win_fmt_error(dw_err, "GetThreadPriority");
    return sirius_custom_win32err_to_errno(dw_err);
  }

#  define E(e) \
    param->priority = e; \
    break;
#  define C(idle, lowest, below_normal, normal, above_normal, highest, \
            critical) \
    switch (thread_priority) { \
    case THREAD_PRIORITY_IDLE: \
      E(idle) \
    case THREAD_PRIORITY_LOWEST: \
      E(lowest) \
    case THREAD_PRIORITY_BELOW_NORMAL: \
      E(below_normal) \
    case THREAD_PRIORITY_NORMAL: \
      E(normal) \
    case THREAD_PRIORITY_ABOVE_NORMAL: \
      E(above_normal) \
    case THREAD_PRIORITY_HIGHEST: \
      E(highest) \
    case THREAD_PRIORITY_TIME_CRITICAL: \
      E(critical) \
    default: \
      internal_error("Invalid argument: [thread priority: %d]\n", \
                     thread_priority); \
      return -1; \
    }

  switch (GetPriorityClass(GetCurrentProcess())) {
  case IDLE_PRIORITY_CLASS:
    C(sirius_thread_priority_min, (int)(2 * PRIORITY_MAP_RATIO),
      (int)(3 * PRIORITY_MAP_RATIO), (int)(4 * PRIORITY_MAP_RATIO),
      (int)(5 * PRIORITY_MAP_RATIO), (int)(6 * PRIORITY_MAP_RATIO),
      (int)(15 * PRIORITY_MAP_RATIO))
    break;
  case BELOW_NORMAL_PRIORITY_CLASS:
    C(sirius_thread_priority_min, (int)(4 * PRIORITY_MAP_RATIO),
      (int)(5 * PRIORITY_MAP_RATIO), (int)(6 * PRIORITY_MAP_RATIO),
      (int)(7 * PRIORITY_MAP_RATIO), (int)(8 * PRIORITY_MAP_RATIO),
      (int)(15 * PRIORITY_MAP_RATIO))
    break;
  case NORMAL_PRIORITY_CLASS:
    C(sirius_thread_priority_min, (int)(6 * PRIORITY_MAP_RATIO),
      (int)(7 * PRIORITY_MAP_RATIO), (int)(8 * PRIORITY_MAP_RATIO),
      (int)(9 * PRIORITY_MAP_RATIO), (int)(10 * PRIORITY_MAP_RATIO),
      (int)(15 * PRIORITY_MAP_RATIO))
    break;
  case ABOVE_NORMAL_PRIORITY_CLASS:
    C(sirius_thread_priority_min, (int)(8 * PRIORITY_MAP_RATIO),
      (int)(9 * PRIORITY_MAP_RATIO), (int)(10 * PRIORITY_MAP_RATIO),
      (int)(11 * PRIORITY_MAP_RATIO), (int)(12 * PRIORITY_MAP_RATIO),
      (int)(15 * PRIORITY_MAP_RATIO))
    break;
  case HIGH_PRIORITY_CLASS:
    C(sirius_thread_priority_min, (int)(11 * PRIORITY_MAP_RATIO),
      (int)(12 * PRIORITY_MAP_RATIO), (int)(13 * PRIORITY_MAP_RATIO),
      (int)(14 * PRIORITY_MAP_RATIO), (int)(15 * PRIORITY_MAP_RATIO),
      (int)(15 * PRIORITY_MAP_RATIO))
    break;
  case REALTIME_PRIORITY_CLASS:
    C((int)(16 * PRIORITY_MAP_RATIO), (int)(22 * PRIORITY_MAP_RATIO),
      (int)(23 * PRIORITY_MAP_RATIO), (int)(24 * PRIORITY_MAP_RATIO),
      (int)(25 * PRIORITY_MAP_RATIO), (int)(26 * PRIORITY_MAP_RATIO),
      sirius_thread_priority_max)
    break;
  default:
    dw_err = GetLastError();
    internal_win_fmt_error(dw_err, "GetPriorityClass");
    return sirius_custom_win32err_to_errno(dw_err);
  }

  return 0;

#  undef C
#  undef E
#else

  int posix_policy;
  struct sched_param thread_param = {0};

  int ret = pthread_getschedparam(thread, &posix_policy, &thread_param);
  if (unlikely(ret)) {
    internal_str_error(ret, "pthread_getschedparam");
    return ret;
  }

  ret = posix_sched_policy_posix_to_sirius(&param->sched_policy, posix_policy);
  if (ret)
    return ret;

  int posix_priority = thread_param.sched_priority;
  param->priority = sirius_min(posix_priority, sirius_thread_priority_max);
  param->priority = sirius_max(posix_priority, sirius_thread_priority_none);

  return 0;

#endif
}
