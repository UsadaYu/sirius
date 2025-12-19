#ifndef SIRIUS_SEM_H
#define SIRIUS_SEM_H

#include "sirius/internal/errno.h"
#include "sirius/internal/sem.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(_WIN64)
typedef HANDLE sirius_sem_t;
#else
typedef sem_t sirius_sem_t;
#endif

/**
 * @brief Initialize a semaphore.
 *
 * @param[out] sem A pointer to the semaphore object to be initialized.
 * @param[in] pshared On POSIX, if non-zero the semaphore is shared between
 * processes; invalid on Windows.
 * @param[in] value Initial value of the semaphore.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
static inline int sirius_sem_init(sirius_sem_t *sem, int pshared,
                                  unsigned int value);

/**
 * @brief Destroy the semaphore.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
static inline int sirius_sem_destroy(sirius_sem_t *sem);

/**
 * @brief Wait the semaphore.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
static inline int sirius_sem_wait(sirius_sem_t *sem);

/**
 * @brief Try to wait the semaphore.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
static inline int sirius_sem_trywait(sirius_sem_t *sem);

/**
 * @brief Wait the semaphore, but limit the waiting time.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
static inline int sirius_sem_timedwait(sirius_sem_t *sem,
                                       uint64_t milliseconds);

/**
 * @brief Post the semaphore.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
static inline int sirius_sem_post(sirius_sem_t *sem);

#if defined(_WIN32) || defined(_WIN64)
/**
 * @note Not a real implementation on Windows.
 */
#  define sirius_sem_getvalue(sem, sval) (0)
#else
/**
 * @brief Obtain the current semaphore value.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
static inline int sirius_sem_getvalue(sirius_sem_t *__restrict sem,
                                      int *__restrict sval);
#endif

#ifdef __cplusplus
}
#endif

/**
 * @implements
 */

#if defined(_WIN32) || defined(_WIN64)

static inline int sirius_sem_init(sirius_sem_t *sem, int pshared,
                                  unsigned int value) {
  if (unlikely(!sem))
    return EINVAL;

  /**
   * @note Windows doesn't have `pshared`, ignore it. Use large max count.
   */
  LONG initial = (value > (unsigned)LONG_MAX) ? LONG_MAX : (LONG)value;
  LONG maximum = LONG_MAX;

  HANDLE h = CreateSemaphore(NULL, initial, maximum, NULL);
  if (!h) {
    DWORD err = GetLastError();
    return sirius_internal_winerr_to_errno(err);
  }

  *sem = h;
  return 0;
}

static inline int sirius_sem_destroy(sirius_sem_t *sem) {
  if (unlikely(!sem || !*sem))
    return EINVAL;

  BOOL ok = CloseHandle(*sem);
  if (!ok) {
    DWORD err = GetLastError();
    return sirius_internal_winerr_to_errno(err);
  }

  *sem = NULL;
  return 0;
}

static inline int sirius_sem_wait(sirius_sem_t *sem) {
  if (unlikely(!sem || !*sem))
    return EINVAL;

  DWORD wait_ret = WaitForSingleObject(*sem, INFINITE);
  switch (wait_ret) {
  case WAIT_OBJECT_0:
    return 0;
  case WAIT_FAILED:
    return sirius_internal_winerr_to_errno(GetLastError());
  case WAIT_ABANDONED:
    return EINVAL;
  default:
    return EINVAL;
  }
}

static inline int sirius_sem_trywait(sirius_sem_t *sem) {
  if (unlikely(!sem || !*sem))
    return EINVAL;

  DWORD wait_ret = WaitForSingleObject(*sem, 0);
  switch (wait_ret) {
  case WAIT_OBJECT_0:
    return 0;
  case WAIT_TIMEOUT:
    /**
     * @brief Mirror `sem_trywait`: would return `EAGAIN/EBUSY`.
     */
    return EBUSY;
  case WAIT_FAILED:
    return sirius_internal_winerr_to_errno(GetLastError());
  case WAIT_ABANDONED:
    return EINVAL;
  default:
    return EINVAL;
  }
}

static inline int sirius_sem_timedwait(sirius_sem_t *sem,
                                       uint64_t milliseconds) {
  if (unlikely(!sem || !*sem))
    return EINVAL;

  DWORD timeout_ms;
  uint64_t tm_prev, tm_cur, tm_elapsed;

  tm_prev = GetTickCount64();
  while (milliseconds > 0) {
    timeout_ms = milliseconds > (uint64_t)(INFINITE - 1) ? INFINITE - 1
                                                         : (DWORD)milliseconds;

    DWORD wait_ret = WaitForSingleObject(*sem, timeout_ms);
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
      return sirius_internal_winerr_to_errno(GetLastError());
    case WAIT_ABANDONED:
      return EINVAL;
    default:
      return EINVAL;
    }
  }

  return ETIMEDOUT;
}

static inline int sirius_sem_post(sirius_sem_t *sem) {
  if (unlikely(!sem || !*sem))
    return EINVAL;

  BOOL ok = ReleaseSemaphore(*sem, 1, NULL);
  if (!ok)
    return sirius_internal_winerr_to_errno(GetLastError());

  return 0;
}

#else

/**
 * @note On POSIX, the semaphore function returns 0 on success, -1 on error with
 * `errno` set.
 */

static inline int sirius_sem_init(sirius_sem_t *sem, int pshared,
                                  unsigned int value) {
  if (unlikely(!sem))
    return EINVAL;
  return sem_init(sem, pshared, value) == 0 ? 0 : errno;
}

static inline int sirius_sem_destroy(sirius_sem_t *sem) {
  if (unlikely(!sem))
    return EINVAL;
  return sem_destroy(sem) == 0 ? 0 : errno;
}

static inline int sirius_sem_wait(sirius_sem_t *sem) {
  if (unlikely(!sem))
    return EINVAL;

  int ret;

  /**
   * @brief Retry if interrupted by signal.
   */
  do {
    ret = sem_wait(sem);
  } while (ret == -1 && errno == EINTR);

  return ret == 0 ? 0 : errno;
}

static inline int sirius_sem_trywait(sirius_sem_t *sem) {
  if (unlikely(!sem))
    return EINVAL;
  return sem_trywait(sem) == 0 ? 0 : errno;
}

static inline int sirius_sem_timedwait(sirius_sem_t *sem,
                                       uint64_t milliseconds) {
  if (unlikely(!sem))
    return EINVAL;

  struct timespec ts;
#  if defined(CLOCK_REALTIME)
  if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
    return errno;
#  else
  time_t now = time(NULL);
  if (now == (time_t)-1)
    return errno;
  ts.tv_sec = now;
  ts.tv_nsec = 0;
#  endif

  ts.tv_sec += (time_t)(milliseconds / 1000);
  ts.tv_nsec += (long)((milliseconds % 1000) * 1000000L);
  if (ts.tv_nsec >= 1000000000L) {
    ts.tv_sec += ts.tv_nsec / 1000000000L;
    ts.tv_nsec = ts.tv_nsec % 1000000000L;
  }

  int ret;
  do {
    ret = sirius_internal_sem_timedwait(sem, &ts);
  } while (ret == -1 && errno == EINTR);

  return ret == 0 ? 0 : errno;
}

static inline int sirius_sem_post(sirius_sem_t *sem) {
  if (unlikely(!sem))
    return EINVAL;
  return sem_post(sem) == 0 ? 0 : errno;
}

static inline int sirius_sem_getvalue(sirius_sem_t *__restrict sem,
                                      int *__restrict sval) {
  if (unlikely(!sem || !sval))
    return EINVAL;
  return sem_getvalue(sem, sval) == 0 ? 0 : errno;
}

#endif

#endif // SIRIUS_SEM_H
