#if !defined(_WIN32) && !defined(_WIN64)
#  error "Incorrect system. Allowed: Windows"
#endif

/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

// #  include "lib/thread/inner/thread.h"

#include "lib/foundation/thread/thread.hpp"

#include <process.h>

#include <mutex>
#include <set>
#include <thread>

#include "lib/foundation/log/log.h"
#include "sirius/thread/spinlock.h"
#include "sirius/thread/thread.h"
#include "utils/errno.h"
#include "utils/io.hpp"

struct ss_thread_s {
  HANDLE handle;
  uint64_t thread_id;
  void *retval;

  ss_spinlock_t spin;
  unsigned int detached: 1;

  /**
   * @note This variable should be guarded by the `spin`.
   */
  bool finished;

  /**
   * @note Based on this variable, try to ensure that the thread does not get
   * stuck when the user calls the interface against the specification, even if
   * it is a wild pointer.
   */
  std::atomic<bool> resource_is_free;
};

namespace sirius {
struct SsThreadExitException {
  void *retval;
};

struct thread_wrapper_arg_s {
  void *(*start_routine)(void *);
  void *arg;
  ss_thread_t thr;
};

/**
 * @note Global static initialization and deinitialization.
 * These operations are generally safe since C++11.
 * `std::mutex`; `std::set`; `malloc/new`; `free/delete`.
 */
class ThreadResourceManager {
 public:
  std::mutex mutex {};
  std::set<ss_thread_t> active_threads {};

  ~ThreadResourceManager() {
    manager_is_alive() = false;

    auto lock = std::lock_guard(mutex);
    for (auto thr : active_threads) {
      if (!thr)
        continue;

      if (!thr->resource_is_free.exchange(true)) {
        ss_spin_destroy(&thr->spin);
        delete thr;
        thr = nullptr;
      }
    }
  }

  /**
   * @note The validity of static memory addresses is not affected by
   * destructors.
   */
  static bool &manager_is_alive() {
    static bool alive = true;
    return alive;
  }

  void mark(ss_thread_t thr) {
    auto lock = std::lock_guard(mutex);
    active_threads.insert(thr);
  }

  void resource_free(ss_thread_t thr) {
    auto lock = std::lock_guard(mutex);

    if (!thr)
      return;

    if (!thr->resource_is_free.exchange(true)) {
      active_threads.erase(thr);
      ss_spin_destroy(&thr->spin);
      CloseHandle(thr->handle);
      delete thr;
      thr = nullptr;
    }
  }
};

namespace {
inline constexpr int64_t kWinThreadPriorityMax = 31;
inline constexpr double kPriorityMapRatio =
  (double)SS_THREAD_PRIORITY_MAX / kWinThreadPriorityMax;

// --- Static Initialization ---
static ThreadResourceManager g_manager;

/**
 * @return If the caller needs to terminate the thread immediately (by calling
 * `_endthreadex`), return 1; Otherwise, return 0.
 */
inline int try_cleanup(ss_thread_t thr) {
  bool should_free = false;

  ss_spin_lock(&thr->spin);
  thr->finished = true;
  if (thr->detached) {
    /**
     * @note Detached threads uniformly release resources through their child
     * threads.
     */
    should_free = true;
  }
  ss_spin_unlock(&thr->spin);

  if (should_free) {
    g_manager.resource_free(thr);
    return 1;
  }

  return 0;
}

inline bool has_been_destructed() {
  if (ThreadResourceManager::manager_is_alive())
    return false;

  static std::atomic<bool> once_print = false;
  if (once_print.exchange(true, std::memory_order_relaxed))
    return true;

  logln_warnsp(
    "\n---------- Fatal Error ----------"
    "\nThe thread manager has been destructed"
    "\nThe `main` function may have already ended"
    "\nOr some unknown errors occurred"
    "\nThere should be no further operations on the thread");
  return true;
}

inline unsigned __stdcall thread_wrapper(void *pv) {
  thread_wrapper_arg_s warg = *(thread_wrapper_arg_s *)pv;
  delete static_cast<thread_wrapper_arg_s *>(pv);

  DWORD dw_err = 0;
  ss_thread_t thr = warg.thr;
  if (!inner_thread_tls_set_value(thr, &dw_err)) {
    if (has_been_destructed()) {
      /**
       * @note Here it will not return an error unless it's necessary after the
       * main function has ended, lest some tools treat the error code as a
       * fatal error.
       */
      return 0;
    }

    foundation_win_last_error(dw_err, "TlsSetValue");
    try_cleanup(thr);
    return utils_winerr_to_errno(dw_err);
  }

  try {
    if (warg.start_routine) {
      thr->retval = warg.start_routine(warg.arg);
    }
  } catch (const SsThreadExitException &e) {
    thr->retval = e.retval;
  } catch (...) {
    // ...
    thr->retval = nullptr;
  }

  try_cleanup(thr);

  /**
   * @note `_endthreadex(0)` will be implicitly called.
   */
  return 0;
}

inline int set_thread_priority(HANDLE thread, int posix_priority) {
  int win_priority = -1;
  DWORD dw_err;

#define E(r) ((int)((r) * kPriorityMapRatio))
#define C(idle, lowest, below_normal, normal, above_normal, highest) \
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
    foundation_win_last_error(dw_err, "GetPriorityClass");
    return utils_winerr_to_errno(dw_err);
  }

  if (!SetThreadPriority(thread, win_priority)) {
    dw_err = GetLastError();
    foundation_win_last_error(dw_err, "SetThreadPriority");
    return utils_winerr_to_errno(dw_err);
  }
  return 0;
#undef C
#undef E
}
} // namespace
} // namespace sirius

using namespace sirius;

// --- API Interface ---

extern "C" sirius_api int ss_thread_create(ss_thread_t *thread,
                                           const ss_thread_attr_t *attr,
                                           void *(*start_routine)(void *),
                                           void *arg) {
  int ret = 0;
  DWORD dw_err;
  unsigned int initflags = 0;
  unsigned int thread_id = 0;
  unsigned int stack_size = 0;
  uintptr_t h = 0;
  ss_thread_t thr = nullptr;
  thread_wrapper_arg_s *warg = nullptr;

  if (attr) {
    initflags = CREATE_SUSPENDED;
    stack_size =
      attr->stacksize <= (size_t)UINT_MAX ? (unsigned int)attr->stacksize : 0;
  }
  thr = new ss_thread_s {};
  ss_spin_init(&thr->spin, kSsThreadProcessPrivate);
  if (attr && attr->detach_state != kSsThreadCreateJoinable) {
    thr->detached = 1;
    g_manager.mark(thr);
  }

  warg = new thread_wrapper_arg_s;
  warg->start_routine = start_routine;
  warg->arg = arg;
  warg->thr = thr;

  h = _beginthreadex(nullptr, stack_size, thread_wrapper, (void *)warg,
                     initflags, &thread_id);
  if (h == 0) {
    dw_err = GetLastError();
    foundation_win_last_error(dw_err, "_beginthreadex");
    ret = utils_winerr_to_errno(dw_err);
    goto label_free1;
  }

  thr->handle = (HANDLE)h;
  thr->thread_id = (uint64_t)thread_id;
  if (initflags == CREATE_SUSPENDED && attr) {
    ret = set_thread_priority(thr->handle, attr->sched_param.priority);
    if (ret)
      goto label_free2;

    if ((DWORD)-1 == ResumeThread(thr->handle)) {
      dw_err = GetLastError();
      foundation_win_last_error(dw_err, "ResumeThread");
      ret = utils_winerr_to_errno(dw_err);
      goto label_free2;
    }
  }

  *thread = thr;
  return 0;

label_free2:
  /**
   * @note Not necessarily safe but the only way to kill the thread.
   */
  (void)ss_thread_cancel(thr);
  CloseHandle(thr->handle);
label_free1:
  delete warg;
  ss_spin_destroy(&thr->spin);
  delete thr;
  return ret;
}

extern "C" sirius_api void ss_thread_exit(void *retval) _SS_INNER_THROW_SPEC {
  /**
   * @note An exception is thrown to trigger stack expansion, and the c++
   * destructor will be executed automatically.
   */
  throw SsThreadExitException {retval};
}

extern "C" sirius_api int ss_thread_join(ss_thread_t thread, void **retval) {
  /**
   * @note Here, the `resource_is_free` flag is checked to prevent the user from
   * calling the `join` function after the detached thread has ended. Although
   * at this moment, `thread` may be a dangling pointer.
   */
  if (!thread || !thread->handle ||
      thread->resource_is_free.load(std::memory_order_relaxed)) {
    return ESRCH;
  }

  ss_spin_lock(&thread->spin);
  if (thread->detached) {
    ss_spin_unlock(&thread->spin);
    return EINVAL;
  }
  ss_spin_unlock(&thread->spin);

  DWORD dw_err = ERROR_SUCCESS;
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
    foundation_win_last_error(dw_err, "WaitForSingleObject");
    return utils_winerr_to_errno(dw_err);
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

  if (!thread->resource_is_free.exchange(true, std::memory_order_seq_cst)) {
    CloseHandle(thread->handle);
    ss_spin_destroy(&thread->spin);
    delete thread;
  }

  return 0;
}

extern "C" sirius_api int ss_thread_detach(ss_thread_t thread) {
  if (!thread || !thread->handle ||
      thread->resource_is_free.load(std::memory_order_relaxed)) {
    return EINVAL;
  }

  bool should_free_now = false;

  ss_spin_lock(&thread->spin);
  if (thread->detached) {
    ss_spin_unlock(&thread->spin);
    return EINVAL;
  }
  thread->detached = 1;
  if (thread->finished) {
    should_free_now = true;
  }
  ss_spin_unlock(&thread->spin);

  g_manager.mark(thread);

  if (should_free_now) {
    /**
     * @note Confirm again that the thread has indeed exited (although
     * `finished=true` basically means it has exited, it is waiting for the
     * kernel object for safety).
     */
    WaitForSingleObject(thread->handle, 0);
    g_manager.resource_free(thread);
  }

  return 0;
}

extern "C" sirius_api int ss_thread_cancel(ss_thread_t thread) {
  if (!thread || !thread->handle)
    return 0;

  if (!TerminateThread(thread->handle, (DWORD)-1)) {
    const DWORD dw_err = GetLastError();
    foundation_win_last_error(dw_err, "TerminateThread");
    return utils_winerr_to_errno(dw_err);
  }
  return 0;
}

extern "C" sirius_api ss_thread_t ss_thread_self() {
  return (ss_thread_t)inner_thread_tls_get_value(nullptr);
}

extern "C" sirius_api int ss_thread_get_priority_max(ss_thread_t thread,
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
    *priority = (int)(kPriorityMapRatio * 15);
    return 0;
  case REALTIME_PRIORITY_CLASS:
    *priority = SS_THREAD_PRIORITY_MAX;
    return 0;
  default:
    dw_err = GetLastError();
    foundation_win_last_error(dw_err, "GetPriorityClass");
    return utils_winerr_to_errno(dw_err);
  }
}

extern "C" sirius_api int ss_thread_get_priority_min(ss_thread_t thread,
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
    *priority = SS_THREAD_PRIORITY_MIN;
    return 0;
  case REALTIME_PRIORITY_CLASS:
    *priority = (int)(kPriorityMapRatio * 16);
    return 0;
  default:
    dw_err = GetLastError();
    foundation_win_last_error(dw_err, "GetPriorityClass");
    return utils_winerr_to_errno(dw_err);
  }
}

extern "C" sirius_api int
ss_thread_setschedparam(ss_thread_t thread,
                        const ss_thread_sched_args_t *param) {
  if (!param)
    return EINVAL;
  (void)param->sched_policy;
  return set_thread_priority(thread->handle, param->priority);
}

extern "C" sirius_api int
ss_thread_getschedparam(ss_thread_t thread, ss_thread_sched_args_t *param) {
  if (!param)
    return EINVAL;

  DWORD dw_err;
  int thread_priority;
  thread_priority = GetThreadPriority(thread->handle);
  if (THREAD_PRIORITY_ERROR_RETURN == thread_priority) {
    dw_err = GetLastError();
    foundation_win_last_error(dw_err, "GetThreadPriority");
    return utils_winerr_to_errno(dw_err);
  }

#define E(e) \
  param->priority = e; \
  break;
#define C(idle, lowest, below_normal, normal, above_normal, highest, critical) \
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
    ss_log_error("Invalid argument. Thread priority: %d\n", thread_priority); \
    return -1; \
  }

  switch (GetPriorityClass(GetCurrentProcess())) {
  case IDLE_PRIORITY_CLASS:
    C(SS_THREAD_PRIORITY_MIN, (int)(2 * kPriorityMapRatio),
      (int)(3 * kPriorityMapRatio), (int)(4 * kPriorityMapRatio),
      (int)(5 * kPriorityMapRatio), (int)(6 * kPriorityMapRatio),
      (int)(15 * kPriorityMapRatio))
    break;
  case BELOW_NORMAL_PRIORITY_CLASS:
    C(SS_THREAD_PRIORITY_MIN, (int)(4 * kPriorityMapRatio),
      (int)(5 * kPriorityMapRatio), (int)(6 * kPriorityMapRatio),
      (int)(7 * kPriorityMapRatio), (int)(8 * kPriorityMapRatio),
      (int)(15 * kPriorityMapRatio))
    break;
  case NORMAL_PRIORITY_CLASS:
    C(SS_THREAD_PRIORITY_MIN, (int)(6 * kPriorityMapRatio),
      (int)(7 * kPriorityMapRatio), (int)(8 * kPriorityMapRatio),
      (int)(9 * kPriorityMapRatio), (int)(10 * kPriorityMapRatio),
      (int)(15 * kPriorityMapRatio))
    break;
  case ABOVE_NORMAL_PRIORITY_CLASS:
    C(SS_THREAD_PRIORITY_MIN, (int)(8 * kPriorityMapRatio),
      (int)(9 * kPriorityMapRatio), (int)(10 * kPriorityMapRatio),
      (int)(11 * kPriorityMapRatio), (int)(12 * kPriorityMapRatio),
      (int)(15 * kPriorityMapRatio))
    break;
  case HIGH_PRIORITY_CLASS:
    C(SS_THREAD_PRIORITY_MIN, (int)(11 * kPriorityMapRatio),
      (int)(12 * kPriorityMapRatio), (int)(13 * kPriorityMapRatio),
      (int)(14 * kPriorityMapRatio), (int)(15 * kPriorityMapRatio),
      (int)(15 * kPriorityMapRatio))
    break;
  case REALTIME_PRIORITY_CLASS:
    C((int)(16 * kPriorityMapRatio), (int)(22 * kPriorityMapRatio),
      (int)(23 * kPriorityMapRatio), (int)(24 * kPriorityMapRatio),
      (int)(25 * kPriorityMapRatio), (int)(26 * kPriorityMapRatio),
      SS_THREAD_PRIORITY_MAX)
    break;
  default:
    dw_err = GetLastError();
    foundation_win_last_error(dw_err, "GetPriorityClass");
    return utils_winerr_to_errno(dw_err);
  }

  return 0;
#undef C
#undef E
}
