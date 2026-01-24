#include "thread/internal/thread.h"

#include "utils/thread/thread.h"

#if defined(_WIN32) || defined(_WIN64)

#  include <atomic>
#  include <mutex>
#  include <set>

struct SiriusThreadExitException {
  void *retval;
};

#endif

// --- Static Initialization ---
#if defined(_WIN32) || defined(_WIN64)

class ThreadResourceManager {
 public:
  std::mutex mtx;
  std::set<sirius_thread_t> active_threads;

  ~ThreadResourceManager() {
    manager_is_alive() = false;

    std::lock_guard<std::mutex> lock(mtx);
    for (auto thr : active_threads) {
      if (thr && !thr->resource_is_free) {
        thr->resource_is_free = true;

        /**
         * @note Don't close the handle here, because the thread might still
         * seem to be running (suspended).
         */

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
    std::lock_guard<std::mutex> lock(mtx);
    active_threads.insert(thr);
  }

  inline void resource_free(sirius_thread_t thr) {
    std::lock_guard<std::mutex> lock(mtx);

    if (thr && !thr->resource_is_free) {
      thr->resource_is_free = true;
      active_threads.erase(thr);

      sirius_spin_destroy(&thr->spin);
      CloseHandle(thr->handle);
      free(thr);
    }
  }
};

static ThreadResourceManager g_manager;

#endif

// --- Static ---
#if defined(_WIN32) || defined(_WIN64)

/**
 * @return If the caller needs to terminate the thread immediately (by calling
 * `_endthreadex`), return 1; Otherwise, return 0.
 */
static inline int win_try_cleanup(sirius_thread_t thr) {
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

static inline bool has_been_destructed() {
  if (ThreadResourceManager::manager_is_alive())
    return false;

  static std::atomic<bool> once_print = false;

  if (once_print.load())
    return true;

  once_print.store(true);

  sirius_log_impl(0, log_level_str_error, log_red, sirius_log_module_name,
                  sirius_file_name, __func__, __LINE__,
                  log_purple
                  "\n"
                  "---------- Fatal Error ----------\n"
                  "- The thread manager has been destructed\n"
                  "- The `main` function may have already ended\n"
                  "- Or some unknown errors occurred\n"
                  "- There should be no further operations on the thread\n"
                  "---------------------------------\n" log_color_none);

  return true;
}

#endif

// --- Internal Interface ---
#if defined(_WIN32) || defined(_WIN64)

extern "C" unsigned __stdcall win_thread_wrapper(void *pv) {
  thread_wrapper_arg_s warg = *(thread_wrapper_arg_s *)pv;
  free(pv);

  sirius_thread_t thr = warg.thr;

  if (!sirius_utils_thread_tls_set_value(thr)) {
    if (has_been_destructed()) {
      /**
       * @note Here it will not return an error unless it's necessary after the
       * main function has ended, lest some tools treat the error code as a
       * fatal error.
       */
      return 0;
    }

    DWORD dw_err = GetLastError();
    utils_win_format_error(dw_err, "TlsSetValue");

    win_try_cleanup(thr);

    return internal_winerr_to_errno(dw_err);
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

#endif

// --- API Interface ---
#if defined(_WIN32) || defined(_WIN64)

extern "C" sirius_api void sirius_thread_exit(void *retval) _sirius_throw_spec {
  /**
   * @note An exception is thrown to trigger stack expansion, and the c++
   * destructor will be executed automatically.
   */
  throw SiriusThreadExitException {retval};
}

extern "C" sirius_api sirius_thread_t sirius_thread_self() {
  return (sirius_thread_t)sirius_utils_thread_tls_get_value();
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
    WaitForSingleObject(thread->handle, INFINITE);

    g_manager.resource_free(thread);
  }

  return 0;
}

#endif
