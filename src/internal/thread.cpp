#include "internal/thread.h"

#include "internal/initializer.h"

#if defined(_WIN32) || defined(_WIN64)

#  include <mutex>
#  include <set>

struct SiriusThreadExitException {
  void *retval;
};

#endif

// --- Constructor ---
#if defined(_WIN32) || defined(_WIN64)

static DWORD g_tls_index = TLS_OUT_OF_INDEXES;

extern "C" void internal_deinit_thread() {
  DWORD dw_err;

  if (g_tls_index != TLS_OUT_OF_INDEXES) {
    if (!TlsFree(g_tls_index)) {
      dw_err = GetLastError();
      internal_win_fmt_error(dw_err, "TlsFree");
    }
    g_tls_index = TLS_OUT_OF_INDEXES;
  }
}

extern "C" bool internal_init_thread() {
  DWORD dw_err;

  if (g_tls_index == TLS_OUT_OF_INDEXES) {
    g_tls_index = TlsAlloc();
    if (g_tls_index == TLS_OUT_OF_INDEXES) {
      dw_err = GetLastError();
      internal_win_fmt_error(dw_err, "TlsAlloc");
      return false;
    }
  }

  return true;
}

#else

extern "C" void internal_deinit_thread() {}

extern "C" bool internal_init_thread() {
  return true;
}

#endif

// --- Static Initialization ---
#if defined(_WIN32) || defined(_WIN64)

class ThreadResourceManager {
 public:
  std::mutex mtx;
  std::set<sirius_thread_t> active_threads;

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

  ~ThreadResourceManager() {
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

#endif

// --- Internal Interface ---
#if defined(_WIN32) || defined(_WIN64)

extern "C" unsigned __stdcall win_thread_wrapper(void *pv) {
  DWORD dw_err;
  thread_wrapper_arg_s warg = *(thread_wrapper_arg_s *)pv;
  sirius_thread_t thr = warg.thr;

  free(pv);

  if (!TlsSetValue(g_tls_index, thr)) {
    /**
     * @note This function usually does not fail to be called. If it does fail,
     * it is usually due to an environmental issue or incorrect usage of the
     * API, such as terminating the entire process directly when the detach
     * thread has not ended. If this happens, the destruction of dependencies on
     * `g_manager` can generally prevent memory leaks.
     */

    dw_err = GetLastError();
    internal_win_fmt_error(dw_err, "TlsSetValue");
    return UINT_MAX;
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

extern "C" sirius_api void
sirius_thread_exit(void *retval) SIRIUS_INTERNAL_THROW_SPEC {
  /**
   * @note An exception is thrown to trigger stack expansion, and the c++
   * destructor will be executed automatically.
   */
  throw SiriusThreadExitException {retval};
}

extern "C" sirius_api sirius_thread_t sirius_thread_self() {
  return (sirius_thread_t)TlsGetValue(g_tls_index);
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
