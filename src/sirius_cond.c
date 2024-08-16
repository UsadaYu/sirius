#include "sirius_cond.h"

#include "internal/internal_log.h"
#include "internal/internal_sys.h"
#include "sirius_errno.h"

int sirius_cond_init(sirius_cond_handle *handle,
                     const sirius_cond_attr_t *attr) {
  if (unlikely(!handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

#ifdef _WIN32
  if (attr) {
    switch (*attr) {
      case sirius_cond_process_private:
        break;
      default:
        internal_warn("Invalid condition attr: %d\n",
                      *attr);
    }
  }

  InitializeConditionVariable(handle);

#else
  int ret = -1;
  pthread_condattr_t cond_attr;
  if (attr) {
    ret = pthread_condattr_init(&cond_attr);
    if (ret) {
      internal_str_warn(ret, "pthread_condattr_init");
    } else {
      ret = pthread_condattr_setpshared(
          &cond_attr, *attr == sirius_cond_process_shared
                          ? PTHREAD_PROCESS_SHARED
                          : PTHREAD_PROCESS_PRIVATE);
      if (ret) {
        internal_str_warn(ret,
                          "pthread_condattr_setpshared");
        pthread_condattr_destroy(&cond_attr);
      }
    }
  }

  pthread_condattr_t *cond_attr_ptr =
      ret ? NULL : &cond_attr;
  ret = pthread_cond_init(handle, cond_attr_ptr);
  if (cond_attr_ptr) {
    pthread_condattr_destroy(&cond_attr);
  }
  if (ret) {
    internal_str_error(ret, "pthread_cond_init");
    return sirius_err_resource_alloc;
  }

#endif

  return 0;
}

int sirius_cond_destroy(sirius_cond_handle *handle) {
  if (unlikely(!handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

#ifdef _WIN32
  /**
   * Windows MSVC condition variables do not require
   * explicit destruction
   */

#else
  int ret = pthread_cond_destroy(handle);
  if (ret) {
    internal_str_error(ret, "pthread_cond_destroy");
    return sirius_err_resource_free;
  }

#endif

  return 0;
}

int sirius_cond_wait(sirius_cond_handle *handle,
                     sirius_mutex_handle *mutex) {
  if (unlikely(!handle || !mutex)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

#ifdef _WIN32
  if (unlikely(!SleepConditionVariableCS(handle, mutex,
                                         INFINITE))) {
    internal_win_fmt_error(GetLastError(),
                           "SleepConditionVariableCS");
    return -1;
  }

#else
  int ret = pthread_cond_wait(handle, mutex);
  if (unlikely(ret)) {
    internal_str_error(ret, "pthread_cond_wait");
    return -1;
  }

#endif

  return 0;
}

int sirius_cond_timedwait(sirius_cond_handle *handle,
                          sirius_mutex_handle *mutex,
                          unsigned long int milliseconds) {
  if (unlikely(!handle || !mutex || milliseconds < 0)) {
    internal_error("Invalid arguments\n");
    return sirius_err_entry;
  }

#ifdef _WIN32
  if (!SleepConditionVariableCS(handle, mutex,
                                milliseconds)) {
    if (ERROR_TIMEOUT == GetLastError()) {
      return sirius_err_timeout;
    } else {
      internal_win_fmt_error(GetLastError(),
                             "SleepConditionVariableCS");
    }
    return -1;
  }

#else
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += milliseconds / 1000;
  ts.tv_nsec += (milliseconds % 1000) * (long)1e6;
  if (ts.tv_nsec > (long)1e9) {
    ts.tv_sec += ts.tv_nsec / (long)1e9;
    ts.tv_nsec = ts.tv_nsec % (long)1e9;
  }

  int ret = pthread_cond_timedwait(handle, mutex, &ts);
  if (ret) {
    if (ETIMEDOUT == ret) {
      return sirius_err_timeout;
    }

    internal_str_error(ret, "pthread_cond_timedwait");
    return -1;
  }

#endif

  return 0;
}

int sirius_cond_signal(sirius_cond_handle *handle) {
  if (unlikely(!handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

#ifdef _WIN32
  WakeConditionVariable(handle);

#else
  int ret = pthread_cond_signal(handle);
  if (unlikely(ret)) {
    internal_str_error(ret, "pthread_cond_signal");
    return -1;
  }

#endif

  return 0;
}

int sirius_cond_broadcast(sirius_cond_handle *handle) {
  if (unlikely(!handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

#ifdef _WIN32
  WakeAllConditionVariable(handle);

#else
  int ret = pthread_cond_broadcast(handle);
  if (unlikely(ret)) {
    internal_str_error(ret, "pthread_cond_broadcast");
    return -1;
  }

#endif

  return 0;
}
