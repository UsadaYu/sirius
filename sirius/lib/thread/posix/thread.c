#if defined(_WIN32) || defined(_WIN64)
#  error "Incorrect system. Allowed: POSIX"
#endif
/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

#include "sirius/thread/thread.h"

#include "lib/foundation/log/log.h"

static inline int sched_policy_sirius_to_posix(int *dst, int src) {
  switch (src) {
  case kSsThreadSchedOther:
    *dst = SCHED_OTHER;
    return 0;
  case kSsThreadSchedFifo:
    *dst = SCHED_FIFO;
    return 0;
  case kSsThreadSchedRR:
    *dst = SCHED_RR;
    return 0;
  default:
    break;
  }
  ss_log_warn("Invalid argument. Sirius sched: %d\n", src);
  return EINVAL;
}

static inline int sched_policy_posix_to_sirius(int *dst, int src) {
  switch (src) {
  case SCHED_OTHER:
    *dst = kSsThreadSchedOther;
    return 0;
  case SCHED_FIFO:
    *dst = kSsThreadSchedFifo;
    return 0;
  case SCHED_RR:
    *dst = kSsThreadSchedRR;
    return 0;
  default:
    break;
  }
  ss_log_warn("Invalid argument. POSIX sched: %d\n", src);
  return EINVAL;
}

static inline int sched_check(int posix_policy) {
  if (posix_policy != SCHED_OTHER && posix_policy != SCHED_FIFO &&
      posix_policy != SCHED_RR) {
    ss_log_error("Invalid argument. `policy`: %d\n", posix_policy);
    return EINVAL;
  }
  return 0;
}

static inline int get_thread_policy(pthread_t thread, int *posix_policy) {
  struct sched_param thread_param = {0};
  int ret = pthread_getschedparam(thread, posix_policy, &thread_param);
  if (ret) {
    foundation_errno_error(ret, "pthread_getschedparam");
    return ret;
  }
  return sched_check(*posix_policy);
}

sirius_api int ss_thread_create(ss_thread_t *thread,
                                const ss_thread_attr_t *attr,
                                void *(*start_routine)(void *), void *arg) {
  int ret;
  pthread_attr_t thread_attr;
  size_t stack_size = attr ? attr->stacksize : 0;

  pthread_attr_init(&thread_attr);
  if (attr) {
    int detach_state;
    if (attr->detach_state == kSsThreadCreateJoinable) {
      detach_state = PTHREAD_CREATE_JOINABLE;
    } else if (attr->detach_state == kSsThreadCreateDetached) {
      detach_state = PTHREAD_CREATE_DETACHED;
    } else {
      ss_log_error("Invalid argument: `detach_state`: %d\n",
                   attr->detach_state);
      ret = EINVAL;
      goto label_free;
    }
    ret = pthread_attr_setdetachstate(&thread_attr, detach_state);
    if (ret) {
      foundation_errno_error(ret, "pthread_attr_setdetachstate");
      goto label_free;
    }

    ret = pthread_attr_setinheritsched(
      &thread_attr,
      attr->inherit_sched == kSsThreadExplicitSched ? PTHREAD_EXPLICIT_SCHED
                                                    : PTHREAD_INHERIT_SCHED);
    if (ret) {
      foundation_errno_error(ret, "pthread_attr_setinheritsched");
      goto label_free;
    }

    ret = pthread_attr_setscope(&thread_attr,
                                attr->scope == kSsThreadScopeProcess
                                  ? PTHREAD_SCOPE_PROCESS
                                  : PTHREAD_SCOPE_SYSTEM);
    if (ret) {
      foundation_errno_error(ret, "pthread_attr_setscope");
      goto label_free;
    }

    if (attr->stackaddr && stack_size > 0) {
      ret = pthread_attr_setstack(&thread_attr, attr->stackaddr, stack_size);
      if (ret) {
        foundation_errno_error(ret, "pthread_attr_setstack");
        goto label_free;
      }
    } else if (attr->stackaddr && stack_size <= 0) {
      ss_log_error("Invalid argument. Stack size: %d\n", stack_size);
      ret = EINVAL;
      goto label_free;
    } else if (!attr->stackaddr && stack_size > 0) {
      ret = pthread_attr_setstacksize(&thread_attr, stack_size);
      if (ret) {
        foundation_errno_error(ret, "pthread_attr_setstack");
        goto label_free;
      }
    }

    ret = pthread_attr_setguardsize(&thread_attr, attr->guardsize);
    if (ret) {
      foundation_errno_error(ret, "pthread_attr_setguardsize");
      goto label_free;
    }

    ss_thread_sched_args_t ssp = attr->sched_param;
    struct sched_param psp;
    int policy;
    ret = sched_policy_sirius_to_posix(&policy, ssp.sched_policy);
    if (ret)
      goto label_free;
    ret = pthread_attr_setschedpolicy(&thread_attr, policy);
    if (ret) {
      foundation_errno_error(ret, "pthread_attr_setschedpolicy");
      goto label_free;
    }
    psp.sched_priority = ssp.priority;
    ret = pthread_attr_setschedparam(&thread_attr, &psp);
    if (ret) {
      foundation_errno_error(ret, "pthread_attr_setschedparam");
      goto label_free;
    }
  }

  pthread_t thr;
  ret = pthread_create(&thr, &thread_attr, start_routine, arg);
  pthread_attr_destroy(&thread_attr);
  if (ret) {
    foundation_errno_error(ret, "pthread_create");
    return ret;
  }
  *thread = (ss_thread_t)thr;
  return 0;

label_free:
  pthread_attr_destroy(&thread_attr);
  return ret;
}

sirius_api void ss_thread_exit(void *retval) {
  pthread_exit(retval);
}

sirius_api int ss_thread_join(ss_thread_t thread, void **retval) {
  return pthread_join((pthread_t)(uintptr_t)thread, retval);
}

sirius_api int ss_thread_detach(ss_thread_t thread) {
  return pthread_detach((pthread_t)(uintptr_t)thread);
}

sirius_api int ss_thread_cancel(ss_thread_t thread) {
  return pthread_cancel((pthread_t)(uintptr_t)thread);
}

sirius_api ss_thread_t ss_thread_self() {
  return (ss_thread_t)(uintptr_t)pthread_self();
}

sirius_api int ss_thread_get_priority_max(ss_thread_t thread, int *priority) {
  if (!priority)
    return EINVAL;

  *priority = 0;
  int policy;
  int ret;

  ret = get_thread_policy((pthread_t)(uintptr_t)thread, &policy);
  if (ret)
    return ret;
  *priority = sched_get_priority_max(policy);
  return 0;
}

sirius_api int ss_thread_get_priority_min(ss_thread_t thread, int *priority) {
  if (!priority)
    return EINVAL;

  *priority = 0;
  int ret;
  int policy;

  ret = get_thread_policy((pthread_t)(uintptr_t)thread, &policy);
  if (ret)
    return ret;
  *priority = sched_get_priority_min(policy);
  return 0;
}

sirius_api int ss_thread_setschedparam(ss_thread_t thread,
                                       const ss_thread_sched_args_t *param) {
  if (!param)
    return EINVAL;

  int ret;
  int posix_sched_policy;
  struct sched_param sched_param = {0};

  ret =
    sched_policy_sirius_to_posix(&posix_sched_policy, (int)param->sched_policy);
  if (ret)
    return ret;

  sched_param.sched_priority = param->priority;
  ret = pthread_setschedparam((pthread_t)(uintptr_t)thread, posix_sched_policy,
                              &sched_param);
  if (ret) {
    foundation_errno_error(ret, "pthread_setschedparam");
    return ret;
  }
  return 0;
}

sirius_api int ss_thread_getschedparam(ss_thread_t thread,
                                       ss_thread_sched_args_t *param) {
  if (!param)
    return EINVAL;

  int ret;
  int posix_policy;
  struct sched_param thread_param = {0};
  int posix_priority;

  ret = pthread_getschedparam((pthread_t)(uintptr_t)thread, &posix_policy,
                              &thread_param);
  if (ret) {
    foundation_errno_error(ret, "pthread_getschedparam");
    return ret;
  }

  ret = sched_policy_posix_to_sirius(&param->sched_policy, posix_policy);
  if (ret)
    return ret;

  posix_priority = thread_param.sched_priority;
  param->priority = UTILS_MIN(posix_priority, SS_THREAD_PRIORITY_MAX);
  param->priority = UTILS_MAX(posix_priority, SS_THREAD_PRIORITY_NONE);
  return 0;
}
