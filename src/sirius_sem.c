#include "sirius/sirius_sem.h"

#include "internal/attributes.h"
#include "internal/decls.h"
#include "internal/errno.h"
#include "internal/log.h"

#if defined(_WIN32) || defined(_WIN64)
internal_check_sizeof(sirius_sem_t, HANDLE);
internal_check_alignof(sirius_sem_t, HANDLE);
#else
internal_check_sizeof(sirius_sem_t, sem_t);
internal_check_alignof(sirius_sem_t, sem_t);
#endif

#if defined(_WIN32) || defined(_WIN64)

sirius_api int sirius_sem_init(sirius_sem_t *sem, int pshared,
                               unsigned int value) {
  (void)pshared;

  /**
   * @note Windows doesn't have `pshared`, ignore it. Use large max count.
   */
  LONG initial = (value > (unsigned)LONG_MAX) ? LONG_MAX : (LONG)value;
  LONG maximum = LONG_MAX;

  HANDLE h = CreateSemaphore(nullptr, initial, maximum, nullptr);
  if (!h) {
    DWORD dw_err = GetLastError();
    internal_win_fmt_error(dw_err, "CreateSemaphore");
    return internal_winerr_to_errno(dw_err);
  }

  memcpy((HANDLE *)sem, &h, sizeof(h));

  return 0;
}

sirius_api int sirius_sem_destroy(sirius_sem_t *sem) {
  if (!sem)
    return EINVAL;

  BOOL ok = CloseHandle(*((HANDLE *)sem));
  if (!ok)
    return internal_winerr_to_errno(GetLastError());

  return 0;
}

sirius_api int sirius_sem_wait(sirius_sem_t *sem) {
  DWORD wait_ret = WaitForSingleObject(*((HANDLE *)sem), INFINITE);
  switch (wait_ret) {
  case WAIT_OBJECT_0:
    return 0;
  case WAIT_FAILED:
    return internal_winerr_to_errno(GetLastError());
  case WAIT_ABANDONED:
    return EINVAL;
  default:
    return EINVAL;
  }
}

sirius_api int sirius_sem_trywait(sirius_sem_t *sem) {
  DWORD wait_ret = WaitForSingleObject(*((HANDLE *)sem), 0);
  switch (wait_ret) {
  case WAIT_OBJECT_0:
    return 0;
  case WAIT_TIMEOUT:
    /**
     * @brief Mirror `sem_trywait`: would return `EAGAIN/EBUSY`.
     */
    return EBUSY;
  case WAIT_FAILED:
    return internal_winerr_to_errno(GetLastError());
  case WAIT_ABANDONED:
    return EINVAL;
  default:
    return EINVAL;
  }
}

sirius_api int sirius_sem_timedwait(sirius_sem_t *sem, uint64_t milliseconds) {
  DWORD timeout_ms;
  uint64_t tm_prev, tm_cur, tm_elapsed;

  tm_prev = GetTickCount64();
  while (milliseconds > 0) {
    timeout_ms = milliseconds > (uint64_t)(INFINITE - 1) ? INFINITE - 1
                                                         : (DWORD)milliseconds;

    DWORD wait_ret = WaitForSingleObject(*((HANDLE *)sem), timeout_ms);
    switch (wait_ret) {
    case WAIT_OBJECT_0:
      return 0;
    case WAIT_TIMEOUT:
      tm_cur = GetTickCount64();
      tm_elapsed = tm_cur - tm_prev;
      if (tm_elapsed >= milliseconds)
        return ETIMEDOUT;

      milliseconds -= tm_elapsed;
      tm_prev = tm_cur;
      break;
    case WAIT_FAILED:
      return internal_winerr_to_errno(GetLastError());
    case WAIT_ABANDONED:
      return EINVAL;
    default:
      return EINVAL;
    }
  }

  return ETIMEDOUT;
}

sirius_api int sirius_sem_post(sirius_sem_t *sem) {
  BOOL ok = ReleaseSemaphore(*((HANDLE *)sem), 1, nullptr);
  if (!ok)
    return internal_winerr_to_errno(GetLastError());

  return 0;
}

#else

/**
 * @note On POSIX, the semaphore function returns 0 on success, -1 on error with
 * `errno` set.
 */

sirius_api int sirius_sem_init(sirius_sem_t *sem, int pshared,
                               unsigned int value) {
  return sem_init((sem_t *)sem, pshared, value) == 0 ? 0 : errno;
}

sirius_api int sirius_sem_destroy(sirius_sem_t *sem) {
  return sem_destroy((sem_t *)sem) == 0 ? 0 : errno;
}

sirius_api int sirius_sem_wait(sirius_sem_t *sem) {
  int ret;

  /**
   * @brief Retry if interrupted by signal.
   */
  do {
    ret = sem_wait((sem_t *)sem);
  } while (ret == -1 && errno == EINTR);

  return ret == 0 ? 0 : errno;
}

sirius_api int sirius_sem_trywait(sirius_sem_t *sem) {
  return sem_trywait((sem_t *)sem) == 0 ? 0 : errno;
}

sirius_api int sirius_sem_timedwait(sirius_sem_t *sem, uint64_t milliseconds) {
  struct timespec ts;

  if (clock_gettime(CLOCK_REALTIME, &ts))
    return errno;

  ts.tv_sec += (time_t)(milliseconds / 1000);
  ts.tv_nsec += (long)((milliseconds % 1000) * 1000000L);
  if (ts.tv_nsec >= 1000000000L) {
    ts.tv_sec += ts.tv_nsec / 1000000000L;
    ts.tv_nsec = ts.tv_nsec % 1000000000L;
  }

  int ret;
  do {
    ret = sem_timedwait((sem_t *)sem, &ts);
  } while (ret == -1 && errno == EINTR);

  return ret == 0 ? 0 : errno;
}

sirius_api int sirius_sem_post(sirius_sem_t *sem) {
  return sem_post((sem_t *)sem) == 0 ? 0 : errno;
}

sirius_api int sirius_sem_getvalue(sirius_sem_t *__restrict sem,
                                   int *__restrict sval) {
  return sem_getvalue((sem_t *)sem, sval) == 0 ? 0 : errno;
}

#endif
