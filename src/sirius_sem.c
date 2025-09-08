#include "sirius_sem.h"

#include "internal/internal_log.h"
#include "internal/internal_sys.h"
#include "sirius_errno.h"

sirius_api int sirius_sem_init(sirius_sem_handle *handle,
                               int pshared,
                               unsigned int value) {
  if (unlikely(!handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

#ifdef _WIN32
  if (pshared) {
    internal_warn("Invalid argument [pshared: %d]\n",
                  pshared);
  }

  *handle =
      CreateSemaphore(nullptr, value, LONG_MAX, nullptr);
  if (!*handle) {
    internal_win_fmt_error(GetLastError(),
                           "CreateSemaphore");
    return sirius_err_resource_alloc;
  }

#else
  if (sem_init(handle, pshared, value)) {
    internal_str_error(errno, "sem_init");
    return sirius_err_resource_alloc;
  }

#endif

  return 0;
}

sirius_api int sirius_sem_destroy(
    sirius_sem_handle *handle) {
  if (unlikely(!handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

#ifdef _WIN32
  if (!CloseHandle(*handle)) {
    internal_win_fmt_error(GetLastError(), "CloseHandle");
    return sirius_err_resource_free;
  }

#else
  if (sem_destroy(handle)) {
    internal_str_error(errno, "sem_destroy");
    return sirius_err_resource_free;
  }

#endif

  return 0;
}

sirius_api int sirius_sem_wait(sirius_sem_handle *handle) {
  if (unlikely(!handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

#ifdef _WIN32
  DWORD ret = WaitForSingleObject(*handle, INFINITE);
  if (WAIT_OBJECT_0 != ret) {
    internal_error("WaitForSingleObject: %d\n", ret);
    if (WAIT_FAILED == ret) {
      internal_win_fmt_error(GetLastError(),
                             "WaitForSingleObject");
    }
    return -1;
  }

#else
  if (sem_wait(handle)) {
    internal_str_error(errno, "sem_wait");
    return -1;
  }

#endif

  return 0;
}

sirius_api int sirius_sem_trywait(
    sirius_sem_handle *handle) {
  if (unlikely(!handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

#ifdef _WIN32
  DWORD ret = WaitForSingleObject(*handle, 0);
  if (WAIT_OBJECT_0 != ret) {
    if (WAIT_TIMEOUT == ret) {
      return sirius_err_timeout;
    }
    internal_error("WaitForSingleObject: %d\n", ret);
    if (WAIT_FAILED == ret) {
      internal_win_fmt_error(GetLastError(),
                             "WaitForSingleObject");
    }
    return -1;
  }

#else
  if (sem_trywait(handle)) {
    if (errno == EAGAIN) {
      return sirius_err_timeout;
    }
    internal_str_error(errno, "sem_trywait");
    return -1;
  }

#endif

  return 0;
}

sirius_api int sirius_sem_timedwait(
    sirius_sem_handle *handle, uint64_t milliseconds) {
  if (unlikely(!handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

#ifdef _WIN32
  DWORD timeout_ms;
  uint64_t tm_prev, tm_cur, tm_elapsed;

  tm_prev = GetTickCount64();
  while (milliseconds > 0) {
    timeout_ms = milliseconds > (uint64_t)(INFINITE - 1)
                     ? INFINITE - 1
                     : (DWORD)milliseconds;

    DWORD wait_ret =
        WaitForSingleObject(*handle, timeout_ms);
    switch (wait_ret) {
      case WAIT_OBJECT_0:
        return 0;
      case WAIT_TIMEOUT:
        tm_cur = GetTickCount64();
        tm_elapsed = tm_cur - tm_prev;
        if (tm_elapsed >= milliseconds) {
          return sirius_err_timeout;
        }
        milliseconds -= tm_elapsed;
        tm_prev = tm_cur;
        break;
      case WAIT_FAILED:
      default:
        internal_error("WaitForSingleObject: %d\n",
                       wait_ret);
        internal_win_fmt_error(GetLastError(),
                               "WaitForSingleObject");
        return -1;
    }
  }

  return sirius_err_timeout;

#else
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += milliseconds / 1000;
  ts.tv_nsec += (milliseconds % 1000) * (long)1e6;
  if (ts.tv_nsec >= (long)1e9) {
    ts.tv_sec += ts.tv_nsec / (long)1e9;
    ts.tv_nsec %= (long)1e9;
  }

  if (sem_timedwait(handle, &ts)) {
    if (errno == ETIMEDOUT) {
      return sirius_err_timeout;
    }
    internal_str_error(errno, "sem_timedwait");
    return -1;
  }

  return 0;
#endif
}

sirius_api int sirius_sem_post(sirius_sem_handle *handle) {
  if (unlikely(!handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

#ifdef _WIN32
  if (!ReleaseSemaphore(*handle, 1, nullptr)) {
    internal_win_fmt_error(GetLastError(),
                           "ReleaseSemaphore");
    return -1;
  }

#else
  if (sem_post(handle)) {
    internal_str_error(errno, "sem_post");
    return -1;
  }

#endif

  return 0;
}

#ifndef _WIN32
sirius_api int sirius_sem_getvalue(
    sirius_sem_handle *handle, int *sval) {
  if (unlikely(!handle) || unlikely(!sval)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

  if (sem_getvalue(handle, sval)) {
    internal_str_error(errno, "sem_getvalue");
    return -1;
  }

  return 0;
}
#endif
