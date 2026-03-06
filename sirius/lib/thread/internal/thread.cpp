#include "lib/thread/internal/thread.h"

#include "utils/io.hpp"

#ifndef LIB_FOUNDATION_THREAD_THREAD_H_
#  define LIB_FOUNDATION_THREAD_THREAD_H_
#endif
#include "lib/foundation/thread/thread.h"

#if defined(_WIN32) || defined(_WIN64)
#  include <mutex>
#  include <set>
#endif

namespace sirius {
#if defined(_WIN32) || defined(_WIN64)
struct SiriusThreadExitException {
  void *retval;
};
#endif

#if defined(_WIN32) || defined(_WIN64)
/**
 * @note Global static initialization and deinitialization.
 * These operations are generally safe since C++11.
 * `std::mutex`; `std::set`; `malloc/new`; `free/delete`.
 */
class ThreadResourceManager {
 public:
  std::mutex mutex {};
  std::set<sirius_thread_t> active_threads {};

  ~ThreadResourceManager() {
    manager_is_alive() = false;

    std::lock_guard lock(mutex);

    for (auto thr : active_threads) {
      if (thr && !thr->resource_is_free) {
        thr->resource_is_free = true;

        sirius_spin_destroy(&thr->spin);
        free(thr);
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

  inline void mark(sirius_thread_t thr) {
    std::lock_guard lock(mutex);

    active_threads.insert(thr);
  }

  inline void resource_free(sirius_thread_t thr) {
    std::lock_guard lock(mutex);

    if (thr && !thr->resource_is_free) {
      thr->resource_is_free = true;
      active_threads.erase(thr);

      sirius_spin_destroy(&thr->spin);
      CloseHandle(thr->handle);
      free(thr);
    }
  }
};
#endif

namespace {
#if defined(_WIN32) || defined(_WIN64)
// --- Static Initialization ---
static ThreadResourceManager g_manager;

/**
 * @return If the caller needs to terminate the thread immediately (by calling
 * `_endthreadex`), return 1; Otherwise, return 0.
 */
inline int win_try_cleanup(sirius_thread_t thr) {
  bool should_free = false;

  sirius_spin_lock(&thr->spin);

  thr->finished = true;

  if (thr->detached) {
    /**
     * @note Detached threads uniformly release resources through their child
     * threads.
     */
    should_free = true;
  }

  sirius_spin_unlock(&thr->spin);

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

  auto es = IO_E(
    "\n---------- Fatal Error ----------"
    "\nThe thread manager has been destructed"
    "\nThe `main` function may have already ended"
    "\nOr some unknown errors occurred"
    "\nThere should be no further operations on the thread");
  utils::io_ln_fd(STDERR_FILENO, es);

  return true;
}
#endif
} // namespace
} // namespace sirius

using namespace sirius;

#if defined(_WIN32) || defined(_WIN64)
// --- Internal Interface ---

extern "C" unsigned __stdcall win_thread_wrapper(void *pv) {
  thread_wrapper_arg_s warg = *(thread_wrapper_arg_s *)pv;
  free(pv);

  DWORD dw_err = 0;
  sirius_thread_t thr = warg.thr;

  if (!sirius_foundation_thread_tls_set_value(thr, &dw_err)) {
    if (has_been_destructed()) {
      /**
       * @note Here it will not return an error unless it's necessary after the
       * main function has ended, lest some tools treat the error code as a
       * fatal error.
       */
      return 0;
    }

    foundation_win_last_error(dw_err, "TlsSetValue");

    win_try_cleanup(thr);

    return utils_winerr_to_errno(dw_err);
  }

  try {
    if (warg.start_routine) {
      thr->retval = warg.start_routine(warg.arg);
    }
  } catch (const SiriusThreadExitException &e) {
    thr->retval = e.retval;
  } catch (...) {
    // ...
    thr->retval = nullptr;
  }

  win_try_cleanup(thr);

  /**
   * @note `_endthreadex(0)` will be implicitly called.
   */
  return 0;
}

extern "C" void win_mark_detach(sirius_thread_t thr) {
  g_manager.mark(thr);
}

// --- API Interface ---

extern "C" sirius_api void sirius_thread_exit(void *retval) _sirius_throw_spec {
  /**
   * @note An exception is thrown to trigger stack expansion, and the c++
   * destructor will be executed automatically.
   */
  throw SiriusThreadExitException {retval};
}

extern "C" sirius_api sirius_thread_t sirius_thread_self() {
  return (sirius_thread_t)sirius_foundation_thread_tls_get_value(nullptr);
}

extern "C" sirius_api int sirius_thread_detach(sirius_thread_t thread) {
  if (!thread || !thread->handle)
    return EINVAL;

  bool should_free_now = false;

  sirius_spin_lock(&thread->spin);

  if (thread->detached) {
    sirius_spin_unlock(&thread->spin);
    return EINVAL;
  }

  thread->detached = 1;

  if (thread->finished) {
    should_free_now = true;
  }

  sirius_spin_unlock(&thread->spin);

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
#endif
