#include "internal/thread.h"
#include "sirius/sirius_math.h"

#define WIN_THREAD_PRIORITY_MAX (31)
#define PRIORITY_MAP_RATIO \
  ((double)sirius_thread_priority_max / WIN_THREAD_PRIORITY_MAX)

#if defined(__linux__)

sirius_maybe_unused static inline uint64_t linux_thread_id() {
#  if defined(__x86_64__)
  pid_t tid;
  __asm__ volatile(
    "mov $186, %%eax\n"
    "syscall\n"
    : "=a"(tid)
    :
    : "rcx", "r11", "memory");
  return (uint64_t)tid;

#  elif defined(__i386__)
  pid_t tid;
  __asm__ volatile(
    "mov $224, %%eax\n"
    "int $0x80\n"
    : "=a"(tid)
    :
    : "memory");
  return (uint64_t)tid;

#  elif defined(__aarch64__)
  uint64_t tid;
  __asm__ volatile(
    "mov x8, %1\n"
    "svc #0\n"
    "mov %0, x0\n"
    : "=r"(tid)
    : "i"(178)
    : "x0", "x8", "memory");
  return tid;

#  elif defined(__arm__)
  uint32_t tid;
  __asm__ volatile(
    "mov r7, %1\n"
    "svc #0\n"
    "mov %0, r0\n"
    : "=r"(tid)
    : "i"(224)
    : "r0", "r7", "memory");
  return (uint64_t)tid;

#  elif defined(__mips__) && defined(__mips_o32)
  uint32_t tid;
  __asm__ volatile(
    "li $v0, %1\n"
    "syscall\n"
    "move %0, $v0\n"
    : "=r"(tid)
    : "i"(4244)
    : "$v0", "memory");
  return (uint64_t)tid;

#  elif defined(__mips__) && \
    (defined(__mips64) || defined(__mips_n64) || defined(__mips_n32))
  uint64_t tid;
  __asm__ volatile(
    "li $v0, %1\n"
    "syscall\n"
    "move %0, $v0\n"
    : "=r"(tid)
    : "i"(5152)
    : "$v0", "memory");
  return tid;

#  elif defined(__powerpc64__) || defined(__PPC64__)
  uint64_t tid;
  __asm__ volatile(
    "li 0, %1\n"
    "sc\n"
    "mr %0, 3\n"
    : "=r"(tid)
    : "i"(207)
    : "r0", "r3", "memory");
  return tid;

#  elif defined(__powerpc__)
  uint32_t tid;
  __asm__ volatile(
    "li 0, %1\n"
    "sc\n"
    "mr %0, 3\n"
    : "=r"(tid)
    : "i"(207)
    : "r0", "r3", "memory");
  return (uint64_t)tid;

#  elif defined(__riscv) && __riscv_xlen == 64
  uint64_t tid;
  __asm__ volatile(
    "li a7, %1\n"
    "ecall\n"
    "mv %0, a0\n"
    : "=r"(tid)
    : "i"(178)
    : "a0", "a7", "memory");
  return tid;

#  elif defined(__riscv) && __riscv_xlen == 32
  uint32_t tid;
  __asm__ volatile(
    "li a7, %1\n"
    "ecall\n"
    "mv %0, a0\n"
    : "=r"(tid)
    : "i"(178)
    : "a0", "a7", "memory");
  return (uint64_t)tid;

#  elif defined(__s390x__)
  uint64_t tid;
  __asm__ volatile(
    "lghi %%r1, %1\n"
    "svc 0\n"
    "lgr %0, %%r2\n"
    : "=r"(tid)
    : "i"(236)
    : "r1", "r2", "memory");
  return tid;

#  elif defined(__s390__)
  uint32_t tid;
  __asm__ volatile(
    "lr 1, %1\n"
    "svc 0\n"
    "lr %0, 2\n"
    : "=r"(tid)
    : "r"(236)
    : "r1", "r2", "memory");
  return (uint64_t)tid;

#  else
  /**
   * @brief Fallback.
   */
  return (uint64_t)(uintptr_t)pthread_self();

#  endif
}

#endif

sirius_api uint64_t sirius_internal_get_tid() {
#if defined(__linux__)

#  if defined(__GLIBC__) && defined(__GLIBC_PREREQ) && __GLIBC_PREREQ(2, 30)
  pid_t gettid(void);
  return (uint64_t)gettid();

#  elif defined(SYS_gettid) && \
    (defined(_GNU_SOURCE) || \
     (defined(__STDC_VERSION__) && __STDC_VERSION__ <= 201710L))
  return (uint64_t)syscall(SYS_gettid);

#  else
  return linux_thread_id();

#  endif

#elif defined(_WIN32) || defined(_WIN64)
  return (uint64_t)GetCurrentThreadId();

#elif defined(__APPLE__) && defined(__MACH__)
  uint64_t tid = 0;
  pthread_threadid_np(nullptr, &tid);
  return tid;

#elif defined(__FreeBSD__)
  return (uint64_t)pthread_getthreadid_np();

#elif defined(__NetBSD__)
  return (uint64_t)_lwp_self();

#elif defined(__OpenBSD__)
  return (uint64_t)getthrid();

#elif defined(sun) || defined(__sun)
  return (uint64_t)thr_self();

#else
  /**
   * @brief Fallback.
   *
   * @note `sirius_thread_t (pthread_t / HANDLE)` is not guaranteed to be an
   * integer, but in most implementations it is either a pointer or an integer.
   */
  return (uint64_t)(uintptr_t)sirius_thread_self();

#endif
}

#if defined(_WIN32) || defined(_WIN64)

static inline int win_set_thread_priority(HANDLE thread, int posix_priority) {
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
    return internal_winerr_to_errno(dw_err);
  }

  if (!SetThreadPriority(thread, win_priority)) {
    dw_err = GetLastError();
    internal_win_fmt_error(dw_err, "SetThreadPriority");
    return internal_winerr_to_errno(dw_err);
  }

  return 0;
#  undef C
#  undef E
}

#else

static inline int posix_sched_policy_sirius_to_posix(int *dst, int src) {
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

  sirius_warn("Invalid argument. Sirius sched: %d\n", src);
  return EINVAL;
}

static inline int posix_sched_policy_posix_to_sirius(int *dst, int src) {
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

  sirius_warn("Invalid argument. Posix sched: %d\n", src);
  return EINVAL;
}

static inline int posix_sched_check(int posix_policy) {
  if (posix_policy != SCHED_OTHER && posix_policy != SCHED_FIFO &&
      posix_policy != SCHED_RR) {
    sirius_error("Invalid argument. `policy`: %d\n", posix_policy);
    return EINVAL;
  }

  return 0;
}

static inline int posix_get_thread_policy(pthread_t thread, int *posix_policy) {
  struct sched_param thread_param = {0};

  int ret = pthread_getschedparam(thread, posix_policy, &thread_param);
  if (ret) {
    internal_str_error(ret, "pthread_getschedparam");
    return ret;
  }

  return posix_sched_check(*posix_policy);
}

#endif

#if defined(_WIN32) || defined(_WIN64)

sirius_api int sirius_thread_create(sirius_thread_t *thread,
                                    const sirius_thread_attr_t *attr,
                                    void *(*start_routine)(void *), void *arg) {
  int ret = 0;
  DWORD dw_err;
  unsigned int initflags = 0;
  unsigned int thread_id = 0;
  unsigned int stack_size = 0;
  uintptr_t h = 0;
  sirius_thread_t thr = nullptr;
  thread_wrapper_arg_s *warg = nullptr;

  if (attr) {
    initflags = CREATE_SUSPENDED;
    stack_size =
      attr->stacksize <= (size_t)UINT_MAX ? (unsigned int)attr->stacksize : 0;
  }

  thr = (sirius_thread_t)calloc(1, sizeof(struct sirius_thread_s));
  if (!thr) {
    sirius_error("calloc\n");
    return ENOMEM;
  }

  sirius_spin_init(&thr->spin, SIRIUS_THREAD_PROCESS_PRIVATE);
  if (attr && attr->detach_state != sirius_thread_joinable) {
    thr->detached = 1;
    win_mark_detach(thr);
  }

  warg = (thread_wrapper_arg_s *)malloc(sizeof(thread_wrapper_arg_s));
  if (!warg) {
    sirius_error("malloc\n");
    ret = ENOMEM;
    goto label_free1;
  }
  warg->start_routine = start_routine;
  warg->arg = arg;
  warg->thr = thr;

  h = _beginthreadex(nullptr, stack_size, win_thread_wrapper, (void *)warg,
                     initflags, &thread_id);
  if (h == 0) {
    dw_err = GetLastError();
    internal_win_fmt_error(dw_err, "_beginthreadex");
    ret = internal_winerr_to_errno(dw_err);
    goto label_free2;
  }

  thr->handle = (HANDLE)h;
  thr->thread_id = (uint64_t)thread_id;

  if (initflags == CREATE_SUSPENDED && attr) {
    ret = win_set_thread_priority(thr->handle, attr->sched_param.priority);
    if (ret)
      goto label_free3;

    if ((DWORD)-1 == ResumeThread(thr->handle)) {
      dw_err = GetLastError();
      internal_win_fmt_error(dw_err, "ResumeThread");
      ret = internal_winerr_to_errno(dw_err);
      goto label_free3;
    }
  }

  *thread = thr;

  return 0;

label_free3:
  /**
   * @note Not necessarily safe but the only way to kill the thread.
   */
  (void)sirius_thread_cancel(thr);
  CloseHandle(thr->handle);
label_free2:
  free(warg);
label_free1:
  sirius_spin_destroy(&thr->spin);
  free(thr);

  return ret;
}

#else

sirius_api int sirius_thread_create(sirius_thread_t *thread,
                                    const sirius_thread_attr_t *attr,
                                    void *(*start_routine)(void *), void *arg) {
  int ret;
  size_t stack_size = 0;
  pthread_attr_t thread_attr;

  stack_size = attr ? attr->stacksize : 0;

  pthread_attr_init(&thread_attr);

  if (attr) {
    int detach_state;
    if (attr->detach_state == sirius_thread_joinable) {
      detach_state = PTHREAD_CREATE_JOINABLE;
    } else if (attr->detach_state == sirius_thread_detached) {
      detach_state = PTHREAD_CREATE_DETACHED;
    } else {
      sirius_error("Invalid argument: `detach_state`: %d\n",
                   attr->detach_state);
      ret = EINVAL;
      goto label_free;
    }
    ret = pthread_attr_setdetachstate(&thread_attr, detach_state);
    if (ret) {
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

    if (attr->stackaddr && stack_size > 0) {
      ret = pthread_attr_setstack(&thread_attr, attr->stackaddr, stack_size);
      if (ret) {
        internal_str_error(ret, "pthread_attr_setstack");
        goto label_free;
      }
    } else if (attr->stackaddr && stack_size <= 0) {
      sirius_error("Invalid argument. Stack size: %d\n", stack_size);
      ret = EINVAL;
      goto label_free;
    } else if (!attr->stackaddr && stack_size > 0) {
      ret = pthread_attr_setstacksize(&thread_attr, stack_size);
      if (ret) {
        internal_str_error(ret, "pthread_attr_setstack");
        goto label_free;
      }
    }

    ret = pthread_attr_setguardsize(&thread_attr, attr->guardsize);
    if (ret) {
      internal_str_error(ret, "pthread_attr_setguardsize");
      goto label_free;
    }

    sirius_thread_sched_param_t ssp = attr->sched_param;
    struct sched_param psp;
    int policy;
    ret = posix_sched_policy_sirius_to_posix(&policy, ssp.sched_policy);
    if (ret)
      goto label_free;
    ret = pthread_attr_setschedpolicy(&thread_attr, policy);
    if (ret) {
      internal_str_error(ret, "pthread_attr_setschedpolicy");
      goto label_free;
    }
    psp.sched_priority = ssp.priority;
    ret = pthread_attr_setschedparam(&thread_attr, &psp);
    if (ret) {
      internal_str_error(ret, "pthread_attr_setschedparam");
      goto label_free;
    }
  }

  pthread_t thr;
  ret = pthread_create(&thr, &thread_attr, start_routine, arg);
  pthread_attr_destroy(&thread_attr);
  if (ret) {
    internal_str_error(ret, "pthread_create");
    return ret;
  }

  *thread = (sirius_thread_t)thr;

  return 0;

label_free:
  pthread_attr_destroy(&thread_attr);

  return ret;
}

#endif

#if !defined(_WIN32) && !defined(_WIN64)

sirius_api void sirius_thread_exit(void *retval) {
  pthread_exit(retval);
}

#endif

#if defined(_WIN32) || defined(_WIN64)

sirius_api int sirius_thread_join(sirius_thread_t thread, void **retval) {
  /**
   * @note Here, the `resource_is_free` flag is checked to prevent the user from
   * calling the `join` function after the detached thread has ended. Although
   * at this moment, `thread` may be a dangling pointer.
   */
  if (unlikely(!thread || !thread->handle || thread->resource_is_free))
    return ESRCH;

  DWORD dw_err = ERROR_SUCCESS;

  sirius_spin_lock(&thread->spin);
  if (thread->detached) {
    sirius_spin_unlock(&thread->spin);
    return EINVAL;
  }
  sirius_spin_unlock(&thread->spin);

  DWORD wait_ret = WaitForSingleObject(thread->handle, INFINITE);
  switch (wait_ret) {
  case WAIT_OBJECT_0:
    break;
  case WAIT_TIMEOUT:
    // (void)CloseHandle(thread->handle);
    return ETIMEDOUT;
  case WAIT_FAILED:
    dw_err = GetLastError();
    (void)CloseHandle(thread->handle);
    internal_win_fmt_error(dw_err, "WaitForSingleObject");
    return internal_winerr_to_errno(dw_err);
  case WAIT_ABANDONED:
    (void)CloseHandle(thread->handle);
    return EINVAL;
  default:
    (void)CloseHandle(thread->handle);
    return EINVAL;
  }

  if (retval) {
    *retval = thread->retval;
  }

  /**
   * @note Here, operate without a lock.
   */
  if (!thread->resource_is_free) {
    thread->resource_is_free = true;
    CloseHandle(thread->handle);
    sirius_spin_destroy(&thread->spin);
    free(thread);
  }

  return 0;
}

#else

sirius_api int sirius_thread_join(sirius_thread_t thread, void **retval) {
  return pthread_join((pthread_t)(uintptr_t)thread, retval);
}

#endif

#if !defined(_WIN32) || !defined(_WIN64)

sirius_api int sirius_thread_detach(sirius_thread_t thread) {
  return pthread_detach((pthread_t)(uintptr_t)thread);
}

#endif

#if defined(_WIN32) || defined(_WIN64)

sirius_api int sirius_thread_cancel(sirius_thread_t thread) {
  if (unlikely(!thread || !thread->handle))
    return 0;

  if (!TerminateThread(thread->handle, (DWORD)-1)) {
    DWORD dw_err = GetLastError();
    internal_win_fmt_error(dw_err, "TerminateThread");
    return internal_winerr_to_errno(dw_err);
  }

  return 0;
}

#else

sirius_api int sirius_thread_cancel(sirius_thread_t thread) {
  return pthread_cancel((pthread_t)(uintptr_t)thread);
}

#endif

#if !defined(_WIN32) && !defined(_WIN64)

sirius_api sirius_thread_t sirius_thread_self() {
  return (sirius_thread_t)(uintptr_t)pthread_self();
}

#endif

#if defined(_WIN32) || defined(_WIN64)

sirius_api int sirius_thread_get_priority_max(sirius_thread_t thread,
                                              int *priority) {
  if (!priority)
    return EINVAL;

  *priority = 0;

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
    return internal_winerr_to_errno(dw_err);
  }
}

#else

sirius_api int sirius_thread_get_priority_max(sirius_thread_t thread,
                                              int *priority) {
  if (!priority)
    return EINVAL;

  *priority = 0;

  int policy;
  int ret;

  ret = posix_get_thread_policy((pthread_t)(uintptr_t)thread, &policy);
  if (ret)
    return ret;

  *priority = sched_get_priority_max(policy);

  return 0;
}

#endif

#if defined(_WIN32) || defined(_WIN64)

sirius_api int sirius_thread_get_priority_min(sirius_thread_t thread,
                                              int *priority) {
  if (!priority)
    return EINVAL;

  *priority = 0;

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
    return internal_winerr_to_errno(dw_err);
  }
}

#else

sirius_api int sirius_thread_get_priority_min(sirius_thread_t thread,
                                              int *priority) {
  if (!priority)
    return EINVAL;

  *priority = 0;

  int ret;
  int policy;

  ret = posix_get_thread_policy((pthread_t)(uintptr_t)thread, &policy);
  if (ret)
    return ret;

  *priority = sched_get_priority_min(policy);

  return 0;
}

#endif

#if defined(_WIN32) || defined(_WIN64)

sirius_api int
sirius_thread_setschedparam(sirius_thread_t thread,
                            const sirius_thread_sched_param_t *param) {
  if (!param)
    return EINVAL;

  (void)param->sched_policy;
  return win_set_thread_priority(thread->handle, param->priority);
}

#else

sirius_api int
sirius_thread_setschedparam(sirius_thread_t thread,
                            const sirius_thread_sched_param_t *param) {
  if (!param)
    return EINVAL;

  int ret;
  int posix_sched_policy;
  struct sched_param sched_param = {0};

  ret = posix_sched_policy_sirius_to_posix(&posix_sched_policy,
                                           (int)param->sched_policy);
  if (ret)
    return ret;

  sched_param.sched_priority = param->priority;
  ret = pthread_setschedparam((pthread_t)(uintptr_t)thread, posix_sched_policy,
                              &sched_param);
  if (ret) {
    internal_str_error(ret, "pthread_setschedparam");
    return ret;
  }

  return 0;
}

#endif

#if defined(_WIN32) || defined(_WIN64)

sirius_api int sirius_thread_getschedparam(sirius_thread_t thread,
                                           sirius_thread_sched_param_t *param) {
  if (!param)
    return EINVAL;

  DWORD dw_err;
  int thread_priority;

  thread_priority = GetThreadPriority(thread->handle);
  if (THREAD_PRIORITY_ERROR_RETURN == thread_priority) {
    dw_err = GetLastError();
    internal_win_fmt_error(dw_err, "GetThreadPriority");
    return internal_winerr_to_errno(dw_err);
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
      sirius_error("Invalid argument. Thread priority: %d\n", \
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
    return internal_winerr_to_errno(dw_err);
  }

  return 0;
#  undef C
#  undef E
}

#else

sirius_api int sirius_thread_getschedparam(sirius_thread_t thread,
                                           sirius_thread_sched_param_t *param) {
  if (!param)
    return EINVAL;

  int ret;
  int posix_policy;
  struct sched_param thread_param = {0};
  int posix_priority;

  ret = pthread_getschedparam((pthread_t)(uintptr_t)thread, &posix_policy,
                              &thread_param);
  if (ret) {
    internal_str_error(ret, "pthread_getschedparam");
    return ret;
  }

  ret = posix_sched_policy_posix_to_sirius(&param->sched_policy, posix_policy);
  if (ret)
    return ret;

  posix_priority = thread_param.sched_priority;
  param->priority = sirius_min(posix_priority, sirius_thread_priority_max);
  param->priority = sirius_max(posix_priority, sirius_thread_priority_none);

  return 0;
}

#endif
