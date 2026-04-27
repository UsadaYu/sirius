#if defined(_WIN32) || defined(_WIN64)
#  error "Incorrect system. Allowed: POSIX"
#endif
/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

#include "sirius/thread/thread.h"

#include "utils/io.hpp"

#define ERRNO_ERR(err_code, fn_str) \
  logln_error("{}", \
              sirius::utils::io::Fmt::errno_err(err_code, fn_str, "{0}", \
                                                utils_pretty_fn));

namespace sirius {
namespace {
inline int sched_policy_sirius_to_posix(int *dst, enum SsThreadSched src) {
  switch (src) {
  case SsThreadSched::kSsThreadSchedOther:
    *dst = SCHED_OTHER;
    return 0;
  case SsThreadSched::kSsThreadSchedFifo:
    *dst = SCHED_FIFO;
    return 0;
  case SsThreadSched::kSsThreadSchedRR:
    *dst = SCHED_RR;
    return 0;
  default:
    break;
  }
  logln_warnsp("Invalid argument. Sirius sched: {0}",
               static_cast<int64_t>(src));
  return EINVAL;
}

inline int sched_policy_posix_to_sirius(enum SsThreadSched *dst, int src) {
  switch (src) {
  case SCHED_OTHER:
    *dst = SsThreadSched::kSsThreadSchedOther;
    return 0;
  case SCHED_FIFO:
    *dst = SsThreadSched::kSsThreadSchedFifo;
    return 0;
  case SCHED_RR:
    *dst = SsThreadSched::kSsThreadSchedRR;
    return 0;
  default:
    break;
  }
  logln_warnsp("Invalid argument. POSIX sched: {0}", src);
  return EINVAL;
}

inline int sched_check(int posix_policy) {
  if (posix_policy != SCHED_OTHER && posix_policy != SCHED_FIFO &&
      posix_policy != SCHED_RR) {
    logln_error("Invalid argument. `policy`: {0}", posix_policy);
    return EINVAL;
  }
  return 0;
}

inline int get_thread_policy(pthread_t thread, int *posix_policy) {
  struct sched_param thread_param {};
  int ret = pthread_getschedparam(thread, posix_policy, &thread_param);
  if (ret) {
    ERRNO_ERR(ret, "pthread_getschedparam");
    return ret;
  }
  return sched_check(*posix_policy);
}
} // namespace
} // namespace sirius

using namespace sirius;

extern "C" SIRIUS_API int ss_thread_create(ss_thread_t *thread,
                                           const ss_thread_attr_t *attr,
                                           void *(*start_routine)(void *),
                                           void *arg) {
  int ret;
  pthread_attr_t thread_attr;
  size_t stack_size = attr ? attr->stacksize : 0;

  pthread_attr_init(&thread_attr);
  if (attr) {
    int detach_state;
    if (attr->detach_state == SsThreadCreate::kSsThreadCreateJoinable) {
      detach_state = PTHREAD_CREATE_JOINABLE;
    } else if (attr->detach_state == SsThreadCreate::kSsThreadCreateDetached) {
      detach_state = PTHREAD_CREATE_DETACHED;
    } else {
      logln_error("Invalid argument: `detach_state`: {0}",
                  static_cast<int64_t>(attr->detach_state));
      ret = EINVAL;
      goto label_free;
    }
    ret = pthread_attr_setdetachstate(&thread_attr, detach_state);
    if (ret) {
      ERRNO_ERR(ret, "pthread_attr_setdetachstate");
      goto label_free;
    }

    ret = pthread_attr_setinheritsched(
      &thread_attr,
      attr->inherit_sched == SsThreadInherit::kSsThreadExplicitSched
        ? PTHREAD_EXPLICIT_SCHED
        : PTHREAD_INHERIT_SCHED);
    if (ret) {
      ERRNO_ERR(ret, "pthread_attr_setinheritsched");
      goto label_free;
    }

    ret =
      pthread_attr_setscope(&thread_attr,
                            attr->scope == SsThreadScope::kSsThreadScopeProcess
                              ? PTHREAD_SCOPE_PROCESS
                              : PTHREAD_SCOPE_SYSTEM);
    if (ret) {
      ERRNO_ERR(ret, "pthread_attr_setscope");
      goto label_free;
    }

    if (attr->stackaddr && stack_size > 0) {
      ret = pthread_attr_setstack(&thread_attr, attr->stackaddr, stack_size);
      if (ret) {
        ERRNO_ERR(ret, "pthread_attr_setstack");
        goto label_free;
      }
    } else if (attr->stackaddr && stack_size <= 0) {
      logln_error("Invalid argument. Stack size: {0}", stack_size);
      ret = EINVAL;
      goto label_free;
    } else if (!attr->stackaddr && stack_size > 0) {
      ret = pthread_attr_setstacksize(&thread_attr, stack_size);
      if (ret) {
        ERRNO_ERR(ret, "pthread_attr_setstacksize");
        goto label_free;
      }
    }

    ret = pthread_attr_setguardsize(&thread_attr, attr->guardsize);
    if (ret) {
      ERRNO_ERR(ret, "pthread_attr_setguardsize");
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
      ERRNO_ERR(ret, "pthread_attr_setschedpolicy");
      goto label_free;
    }
    psp.sched_priority = ssp.priority;
    ret = pthread_attr_setschedparam(&thread_attr, &psp);
    if (ret) {
      ERRNO_ERR(ret, "pthread_attr_setschedparam");
      goto label_free;
    }
  }

  pthread_t thr;
  ret = pthread_create(&thr, &thread_attr, start_routine, arg);
  pthread_attr_destroy(&thread_attr);
  if (ret) {
    ERRNO_ERR(ret, "pthread_create");
    return ret;
  }
  *thread = (ss_thread_t)thr;
  return 0;

label_free:
  pthread_attr_destroy(&thread_attr);
  return ret;
}

extern "C" SIRIUS_API void ss_thread_exit(void *retval) {
  pthread_exit(retval);
}

extern "C" SIRIUS_API int ss_thread_join(ss_thread_t thread, void **retval) {
  return pthread_join((pthread_t)(uintptr_t)thread, retval);
}

extern "C" SIRIUS_API int ss_thread_detach(ss_thread_t thread) {
  return pthread_detach((pthread_t)(uintptr_t)thread);
}

extern "C" SIRIUS_API int ss_thread_cancel(ss_thread_t thread) {
  return pthread_cancel((pthread_t)(uintptr_t)thread);
}

extern "C" SIRIUS_API ss_thread_t ss_thread_self() {
  return (ss_thread_t)(uintptr_t)pthread_self();
}

extern "C" SIRIUS_API int ss_thread_get_priority_max(ss_thread_t thread,
                                                     int *priority) {
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

extern "C" SIRIUS_API int ss_thread_get_priority_min(ss_thread_t thread,
                                                     int *priority) {
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

extern "C" SIRIUS_API int
ss_thread_setschedparam(ss_thread_t thread,
                        const ss_thread_sched_args_t *param) {
  if (!param)
    return EINVAL;

  int ret;
  int posix_sched_policy;
  struct sched_param sched_param {};

  ret = sched_policy_sirius_to_posix(&posix_sched_policy, param->sched_policy);
  if (ret)
    return ret;

  sched_param.sched_priority = param->priority;
  ret = pthread_setschedparam((pthread_t)(uintptr_t)thread, posix_sched_policy,
                              &sched_param);
  if (ret) {
    ERRNO_ERR(ret, "pthread_setschedparam");
    return ret;
  }
  return 0;
}

extern "C" SIRIUS_API int
ss_thread_getschedparam(ss_thread_t thread, ss_thread_sched_args_t *param) {
  if (!param)
    return EINVAL;

  int ret;
  int posix_policy;
  struct sched_param thread_param {};
  int posix_priority;

  ret = pthread_getschedparam((pthread_t)(uintptr_t)thread, &posix_policy,
                              &thread_param);
  if (ret) {
    ERRNO_ERR(ret, "pthread_getschedparam");
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
