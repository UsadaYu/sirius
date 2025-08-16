#include "sirius_thread.h"

#include "internal/internal_log.h"
#include "internal/internal_sys.h"
#include "sirius_errno.h"

sirius_api int sirius_thread_create(
    sirius_thread_handle *handle,
    const sirius_thread_attr_t *attr,
    void *(*start_routine)(void *), void *arg) {
  if (unlikely(!handle) || unlikely(!start_routine)) {
    internal_error("null pointer\n");
    return sirius_err_entry;
  }

  int attr_ret = 0;
  size_t s_stack =
      attr ? ((attr->stacksize > 0) ? attr->stacksize : 0)
           : 0;

#ifdef _WIN32
  DWORD thread_id;

  *handle =
      CreateThread(nullptr, s_stack,
                   (LPTHREAD_START_ROUTINE)start_routine,
                   arg, 0, &thread_id);
  if (!*handle) {
    internal_win_fmt_error(GetLastError(), "CreateThread");
    return sirius_err_resource_alloc;
  }

#else
  pthread_attr_t thread_attr;
  pthread_attr_init(&thread_attr);

#define E(f)                    \
  internal_str_error(errno, f); \
  attr_ret |= -1;
  if (attr) {
    if (pthread_attr_setdetachstate(
            &thread_attr,
            attr->detach_state == sirius_thread_detached
                ? PTHREAD_CREATE_DETACHED
                : PTHREAD_CREATE_JOINABLE)) {
      E("pthread_attr_setdetachstate")
    }

    if (pthread_attr_setinheritsched(
            &thread_attr,
            attr->inherit_sched ==
                    sirius_thread_explicit_sched
                ? PTHREAD_EXPLICIT_SCHED
                : PTHREAD_INHERIT_SCHED)) {
      E("pthread_attr_setinheritsched")
    }

    if (pthread_attr_setscope(
            &thread_attr,
            attr->scope == sirius_thread_scope_process
                ? PTHREAD_SCOPE_PROCESS
                : PTHREAD_SCOPE_SYSTEM)) {
      E("pthread_attr_setscope")
    }

    if (attr->stackaddr && s_stack > 0) {
      if (pthread_attr_setstack(
              &thread_attr, attr->stackaddr, s_stack)) {
        E("pthread_attr_setstack")
      }
    } else if (attr->stackaddr && s_stack <= 0) {
      internal_warn(
          "Invalid arguments: [stack size: %d]\n",
          s_stack);
      attr_ret |= -1;
    } else if (!attr->stackaddr && s_stack > 0) {
      if (pthread_attr_setstacksize(&thread_attr,
                                    s_stack)) {
        E("pthread_attr_setstacksize")
      }
    }

    if (pthread_attr_setguardsize(&thread_attr,
                                  attr->guardsize)) {
      E("pthread_attr_setguardsize")
    }
  }
#undef E

  int ret = pthread_create(handle, &thread_attr,
                           start_routine, arg);
  pthread_attr_destroy(&thread_attr);
  if (ret) {
    internal_str_error(ret, "pthread_create");
    return sirius_err_resource_alloc;
  }

#endif

  if (attr) {
    attr_ret |= sirius_thread_setschedparam(
        *handle, &(attr->sched_param));
    if (attr_ret) {
      internal_warn(
          "the thread was created, but failed to set its "
          "attributes\n");
    }
  }

  (void)handle;
  return 0;
}

sirius_api int sirius_thread_join(
    sirius_thread_handle handle,
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100)
#endif
    void **retval
#ifdef _MSC_VER
#pragma warning(pop)
#endif
) {
#ifdef _WIN32
  DWORD ret = WaitForSingleObject(handle, INFINITE);
  if (ret != WAIT_OBJECT_0) {
    internal_error("WaitForSingleObject: %d\n", ret);
    if (WAIT_FAILED == ret) {
      internal_win_fmt_error(GetLastError(),
                             "WaitForSingleObject");
    }
    ret = (DWORD)-1;
  }

  if (!CloseHandle(handle)) {
    internal_win_fmt_error(GetLastError(), "CloseHandle");
    ret = (DWORD)-1;
  }

  return (int)ret;

#else
  if (unlikely(pthread_join(handle, retval))) {
    internal_str_error(errno, "pthread_join");
    return -1;
  }
  return 0;

#endif
}

#ifndef _WIN32
sirius_api int sirius_thread_detach(
    sirius_thread_handle handle) {
  if (unlikely(pthread_detach(handle))) {
    internal_str_error(errno, "pthread_detach");
    return -1;
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

sirius_api int sirius_thread_cancel(
    sirius_thread_handle handle) {
#ifdef _WIN32
  if (!TerminateThread(handle, (DWORD)-1)) {
    internal_win_fmt_error(GetLastError(),
                           "TerminateThread");
    return -1;
  }

#else
  if (pthread_cancel(handle)) {
    internal_str_error(errno, "pthread_cancel");
    return -1;
  }

#endif

  return 0;
}

sirius_api sirius_thread_handle sirius_thread_self() {
#ifdef _WIN32
  return GetCurrentThread();

#else
  return pthread_self();

#endif
}

#ifndef _WIN32
static bool get_current_policy(sirius_thread_handle handle,
                               int *policy) {
  struct sched_param thread_param = {0};
  if (pthread_getschedparam(handle, policy,
                            &thread_param)) {
    internal_str_error(errno, "pthread_getschedparam");
    return false;
  }

  if (*policy != SCHED_OTHER && *policy != SCHED_FIFO &&
      *policy != SCHED_RR) {
    internal_error("Invalid argument: [policy: %d]\n",
                   *policy);
    return false;
  }

  return true;
}
#endif

sirius_api int sirius_thread_sched_get_priority_max(
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100)
#endif
    sirius_thread_handle handle
#ifdef _MSC_VER
#pragma warning(pop)
#endif
) {
#ifdef _WIN32
  switch (GetPriorityClass(GetCurrentProcess())) {
    case IDLE_PRIORITY_CLASS:
    case BELOW_NORMAL_PRIORITY_CLASS:
    case NORMAL_PRIORITY_CLASS:
    case ABOVE_NORMAL_PRIORITY_CLASS:
    case HIGH_PRIORITY_CLASS:
      return 15;
    case REALTIME_PRIORITY_CLASS:
      return sirius_thread_priority_max;
    default:
      internal_win_fmt_error(GetLastError(),
                             "GetPriorityClass");
      return -1;
  }

#else
  int policy;
  if (!get_current_policy(handle, &policy)) return -1;
  switch (policy) {
    case SCHED_OTHER:
      return 0;
    default:
      return sirius_thread_priority_max;
  }

#endif
}

sirius_api int sirius_thread_sched_get_priority_min(
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100)
#endif
    sirius_thread_handle handle
#ifdef _MSC_VER
#pragma warning(pop)
#endif
) {
#ifdef _WIN32
  switch (GetPriorityClass(GetCurrentProcess())) {
    case IDLE_PRIORITY_CLASS:
    case BELOW_NORMAL_PRIORITY_CLASS:
    case NORMAL_PRIORITY_CLASS:
    case ABOVE_NORMAL_PRIORITY_CLASS:
    case HIGH_PRIORITY_CLASS:
      return 1;
    case REALTIME_PRIORITY_CLASS:
      return 16;
    default:
      internal_win_fmt_error(GetLastError(),
                             "GetPriorityClass");
      return -1;
  }

#else
  int policy;
  if (!get_current_policy(handle, &policy)) return -1;
  switch (policy) {
    case SCHED_OTHER:
      return 0;
    default:
      return 1;
  }

#endif
}

#ifdef _WIN32
static bool set_thread_priority(
    sirius_thread_handle handle, int priority) {
  int thread_priority = -1;

  switch (GetPriorityClass(GetCurrentProcess())) {
    case IDLE_PRIORITY_CLASS:
      if (priority <= 1) {
        thread_priority = THREAD_PRIORITY_IDLE;
      } else if (2 == priority) {
        thread_priority = THREAD_PRIORITY_LOWEST;
      } else if (3 == priority) {
        thread_priority = THREAD_PRIORITY_BELOW_NORMAL;
      } else if (4 == priority) {
        thread_priority = THREAD_PRIORITY_NORMAL;
      } else if (5 == priority) {
        thread_priority = THREAD_PRIORITY_ABOVE_NORMAL;
      } else if (priority >= 6 && priority < 15) {
        thread_priority = THREAD_PRIORITY_HIGHEST;
      } else if (priority >= 15) {
        thread_priority = THREAD_PRIORITY_TIME_CRITICAL;
      }
      break;
    case BELOW_NORMAL_PRIORITY_CLASS:
      if (priority <= 1) {
        thread_priority = THREAD_PRIORITY_IDLE;
      } else if (4 == priority) {
        thread_priority = THREAD_PRIORITY_LOWEST;
      } else if (5 == priority) {
        thread_priority = THREAD_PRIORITY_BELOW_NORMAL;
      } else if (6 == priority) {
        thread_priority = THREAD_PRIORITY_NORMAL;
      } else if (7 == priority) {
        thread_priority = THREAD_PRIORITY_ABOVE_NORMAL;
      } else if (priority >= 8 && priority < 15) {
        thread_priority = THREAD_PRIORITY_HIGHEST;
      } else if (priority >= 15) {
        thread_priority = THREAD_PRIORITY_TIME_CRITICAL;
      }
      break;
    case NORMAL_PRIORITY_CLASS:
      if (priority <= 5) {
        thread_priority = THREAD_PRIORITY_IDLE;
      } else if (6 == priority) {
        thread_priority = THREAD_PRIORITY_LOWEST;
      } else if (7 == priority) {
        thread_priority = THREAD_PRIORITY_BELOW_NORMAL;
      } else if (8 == priority) {
        thread_priority = THREAD_PRIORITY_NORMAL;
      } else if (9 == priority) {
        thread_priority = THREAD_PRIORITY_ABOVE_NORMAL;
      } else if (priority >= 10 && priority < 15) {
        thread_priority = THREAD_PRIORITY_HIGHEST;
      } else if (priority >= 15) {
        thread_priority = THREAD_PRIORITY_TIME_CRITICAL;
      }
      break;
    case ABOVE_NORMAL_PRIORITY_CLASS:
      if (priority <= 7) {
        thread_priority = THREAD_PRIORITY_IDLE;
      } else if (8 == priority) {
        thread_priority = THREAD_PRIORITY_LOWEST;
      } else if (9 == priority) {
        thread_priority = THREAD_PRIORITY_BELOW_NORMAL;
      } else if (10 == priority) {
        thread_priority = THREAD_PRIORITY_NORMAL;
      } else if (11 == priority) {
        thread_priority = THREAD_PRIORITY_ABOVE_NORMAL;
      } else if (priority >= 12 && priority < 15) {
        thread_priority = THREAD_PRIORITY_HIGHEST;
      } else if (priority >= 15) {
        thread_priority = THREAD_PRIORITY_TIME_CRITICAL;
      }
      break;
    case HIGH_PRIORITY_CLASS:
      if (priority <= 10) {
        thread_priority = THREAD_PRIORITY_IDLE;
      } else if (11 == priority) {
        thread_priority = THREAD_PRIORITY_LOWEST;
      } else if (12 == priority) {
        thread_priority = THREAD_PRIORITY_BELOW_NORMAL;
      } else if (13 == priority) {
        thread_priority = THREAD_PRIORITY_NORMAL;
      } else if (14 == priority) {
        thread_priority = THREAD_PRIORITY_ABOVE_NORMAL;
      } else if (priority == 15) {
        thread_priority = THREAD_PRIORITY_HIGHEST;
      } else if (priority > 15) {
        thread_priority = THREAD_PRIORITY_TIME_CRITICAL;
      }
      break;
    case REALTIME_PRIORITY_CLASS:
      if (priority <= 21) {
        thread_priority = THREAD_PRIORITY_IDLE;
      } else if (22 == priority) {
        thread_priority = THREAD_PRIORITY_LOWEST;
      } else if (23 == priority) {
        thread_priority = THREAD_PRIORITY_BELOW_NORMAL;
      } else if (24 == priority) {
        thread_priority = THREAD_PRIORITY_NORMAL;
      } else if (25 == priority) {
        thread_priority = THREAD_PRIORITY_ABOVE_NORMAL;
      } else if (priority >= 26 && priority < 31) {
        thread_priority = THREAD_PRIORITY_HIGHEST;
      } else if (priority >= 31) {
        thread_priority = THREAD_PRIORITY_TIME_CRITICAL;
      }
      break;
    default:
      internal_win_fmt_error(GetLastError(),
                             "GetPriorityClass");
      return false;
  }

  if (!SetThreadPriority(handle, thread_priority)) {
    internal_win_fmt_error(GetLastError(),
                           "SetThreadPriority");
    return false;
  }

  return true;
}
#endif

sirius_api int sirius_thread_setschedparam(
    sirius_thread_handle handle,
    const sirius_thread_sched_param_t *param) {
  if (unlikely(!param)) {
    internal_error("null pointer\n");
    return sirius_err_entry;
  }

  int priority = param->priority;
  if (priority < sirius_thread_priority_min ||
      priority > sirius_thread_priority_max) {
    internal_warn("Invalid argument: [priority: %d]\n",
                  priority);
  }

#ifdef _WIN32
  return set_thread_priority(handle, priority) ? 0 : -1;

#else
  int policy;
  struct sched_param sched_param = {0};
  sched_param.sched_priority = priority * 3;

#define E                                                \
  internal_warn(                                         \
      "Invalid argument: [policy: %d] [sched_priority: " \
      "%d]\n",                                           \
      param->sched_policy, priority);                    \
  internal_warn("Use default argument\n");

  switch (param->sched_policy) {
    case sirius_thread_sched_other:
      if (0 != sched_param.sched_priority) {
        E sched_param.sched_priority = 0;
      }
      policy = SCHED_OTHER;
      break;
    case sirius_thread_sched_fifo:
    case sirius_thread_sched_rr:
      if (sched_param.sched_priority < 1) {
        E sched_param.sched_priority = 1;
      }
      if (sched_param.sched_priority > 99) {
        E sched_param.sched_priority = 99;
      }
      policy =
          sirius_thread_sched_fifo == param->sched_policy
              ? SCHED_FIFO
              : SCHED_RR;
      break;
    default:
      internal_error(
          "Invalid argument: [sched_policy: %d]\n",
          param->sched_policy);
      return sirius_err_args;
  }
#undef E

  if (pthread_setschedparam(handle, policy,
                            &sched_param)) {
    internal_str_error(errno, "pthread_setschedparam");
    return -1;
  }
  return 0;

#endif
}

sirius_api int sirius_thread_getschedparam(
    sirius_thread_handle handle,
    sirius_thread_sched_param_t *param) {
  if (unlikely(!param)) {
    internal_error("null pointer\n");
    return sirius_err_entry;
  }

#ifdef _WIN32
  int thread_priority = GetThreadPriority(handle);
  if (THREAD_PRIORITY_ERROR_RETURN == thread_priority) {
    internal_win_fmt_error(GetLastError(),
                           "GetThreadPriority");
    return -1;
  }

#define P(p)           \
  param->priority = p; \
  break;
#define PP(p_idle, p_lowest, p_b_nornal, p_normal,     \
           p_a_normal, p_highest, p_critical)          \
  switch (thread_priority) {                           \
    case THREAD_PRIORITY_IDLE:                         \
      P(p_idle)                                        \
    case THREAD_PRIORITY_LOWEST:                       \
      P(p_lowest)                                      \
    case THREAD_PRIORITY_BELOW_NORMAL:                 \
      P(p_b_nornal)                                    \
    case THREAD_PRIORITY_NORMAL:                       \
      P(p_normal)                                      \
    case THREAD_PRIORITY_ABOVE_NORMAL:                 \
      P(p_a_normal)                                    \
    case THREAD_PRIORITY_HIGHEST:                      \
      P(p_highest)                                     \
    case THREAD_PRIORITY_TIME_CRITICAL:                \
      P(p_critical)                                    \
    default:                                           \
      internal_error(                                  \
          "Invalid argument: [thread priority: %d]\n", \
          thread_priority);                            \
      return -1;                                       \
  }
  switch (GetPriorityClass(GetCurrentProcess())) {
    case IDLE_PRIORITY_CLASS:
      PP(1, 2, 3, 4, 5, 6, 15) break;
    case BELOW_NORMAL_PRIORITY_CLASS:
      PP(1, 4, 5, 6, 7, 8, 15) break;
    case NORMAL_PRIORITY_CLASS:
      PP(1, 6, 7, 8, 9, 10, 15) break;
    case ABOVE_NORMAL_PRIORITY_CLASS:
      PP(1, 8, 9, 10, 11, 12, 15) break;
    case HIGH_PRIORITY_CLASS:
      PP(1, 11, 12, 13, 14, 15, 15) break;
    case REALTIME_PRIORITY_CLASS:
      PP(16, 22, 23, 24, 25, 26, 31) break;
    default:
      internal_win_fmt_error(GetLastError(),
                             "GetPriorityClass");
      param->priority = -1;
      return -1;
  }
#undef PP
#undef P

#else
  int policy;
  struct sched_param thread_param = {0};

  if (!get_current_policy(handle, &policy)) return -1;
  switch (policy) {
    case SCHED_OTHER:
      param->sched_policy = sirius_thread_sched_other;
      break;
    case SCHED_FIFO:
      param->sched_policy = sirius_thread_sched_fifo;
      break;
    default:
      param->sched_policy = sirius_thread_sched_rr;
      break;
  }

  if (thread_param.sched_priority >= 0 &&
      thread_param.sched_priority <= 99) {
    param->priority =
        (int)(thread_param.sched_priority / 3);
    param->priority =
        param->priority > sirius_thread_priority_max
            ? sirius_thread_priority_max
            : param->priority;
  } else {
    param->priority = -1;
    internal_error(
        "Invalid argument: [sched_priority: %d]\n",
        thread_param.sched_priority);
    return -1;
  }

#endif

  return 0;
}
