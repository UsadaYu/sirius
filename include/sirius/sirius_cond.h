#ifndef SIRIUS_COND_H
#define SIRIUS_COND_H

#include "sirius/internal/errno.h"
#include "sirius/sirius_attributes.h"
#include "sirius/sirius_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  /**
   * @brief Thread sharing within a single process.
   */
  sirius_cond_process_private = 0,

#if defined(_WIN32) || defined(_WIN64)
  sirius_cond_process_shared = sirius_cond_process_private,
#else
  /**
   * @brief Sharing within multiple processes.
   *
   * @note Supported on POSIX systems only.
   */
  sirius_cond_process_shared = 1,
#endif
} sirius_cond_type_t;

/**
 * @brief Cross-platform condition variable handle.
 */
#if defined(_WIN32) || defined(_WIN64)
typedef CONDITION_VARIABLE sirius_cond_t;
#else
typedef pthread_cond_t sirius_cond_t;
#endif

/**
 * @brief Initializes a condition variable.
 *
 * @param[out] cond A pointer to the condition variable object to be
 * initialized.
 * @param[in] type A pointer to the condition variable type. If `nullptr`,
 * a default (process private) condition variable is created.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
static inline int sirius_cond_init(sirius_cond_t *__restrict cond,
                                   const sirius_cond_type_t *__restrict type);

/**
 * @brief Destroy the condition variable.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
static inline int sirius_cond_destroy(sirius_cond_t *cond);

/**
 * @brief Wait the condition variable.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
static inline int sirius_cond_wait(sirius_cond_t *__restrict cond,
                                   sirius_mutex_t *__restrict mutex);

/**
 * @brief Wait the condition variable, but limit the waiting time.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
static inline int sirius_cond_timedwait(sirius_cond_t *__restrict cond,
                                        sirius_mutex_t *__restrict mutex,
                                        uint64_t milliseconds);

/**
 * @brief Wake up a condition variable.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
static inline int sirius_cond_signal(sirius_cond_t *cond);

/**
 * @brief Wake up all condition variable.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
static inline int sirius_cond_broadcast(sirius_cond_t *cond);

#ifdef __cplusplus
}
#endif

#if defined(_WIN32) || defined(_WIN64)

static inline int sirius_cond_init(sirius_cond_t *__restrict cond,
                                   const sirius_cond_type_t *__restrict type) {
  if (unlikely(!cond))
    return EINVAL;

  /**
   * @note Windows condition variables don't support process sharing and
   * don't have configurable attributes, so here ignore the type parameter.
   */
  (void)type;
  InitializeConditionVariable(cond);
  return 0;
}

static inline int sirius_cond_destroy(sirius_cond_t *cond) {
  if (unlikely(!cond))
    return EINVAL;

  /**
   * @note Windows condition variables don't require explicit destruction.
   */
  (void)cond;
  return 0;
}

static inline int sirius_cond_wait(sirius_cond_t *__restrict cond,
                                   sirius_mutex_t *__restrict mutex) {
  if (unlikely(!cond || !mutex))
    return EINVAL;

  if (mutex->type == sirius_mutex_recursive) {
    if (!SleepConditionVariableCS(cond, &mutex->handle.critical_section,
                                  INFINITE)) {
      return sirius_internal_winerr_to_errno(GetLastError());
    }
  } else {
    if (!SleepConditionVariableSRW(cond, &mutex->handle.srw_lock, INFINITE,
                                   0)) {
      return sirius_internal_winerr_to_errno(GetLastError());
    }
  }
  return 0;
}

static inline int sirius_cond_timedwait(sirius_cond_t *__restrict cond,
                                        sirius_mutex_t *__restrict mutex,
                                        uint64_t milliseconds) {
  if (unlikely(!cond || !mutex))
    return EINVAL;

  DWORD timeout_ms;
  uint64_t tm_prev, tm_cur, tm_elapsed;

  tm_prev = GetTickCount64();

#  define CL \
    do { \
      while (milliseconds > 0) { \
        timeout_ms = milliseconds >= (uint64_t)(INFINITE - 1) \
          ? INFINITE - 1 \
          : (DWORD)milliseconds; \
        if (CLT) { \
          return 0; \
        } \
        DWORD dw_err = GetLastError(); \
        if (dw_err == ERROR_TIMEOUT) { \
          tm_cur = GetTickCount64(); \
          tm_elapsed = tm_cur - tm_prev; \
          if (tm_elapsed >= milliseconds) \
            return ETIMEDOUT; \
          milliseconds -= tm_elapsed; \
          tm_prev = tm_cur; \
        } else { \
          return sirius_internal_winerr_to_errno(dw_err); \
        } \
      } \
    } while (0)

  if (mutex->type == sirius_mutex_recursive) {
#  define CLT \
    SleepConditionVariableCS(cond, &mutex->handle.critical_section, timeout_ms)
    CL;
#  undef CLT
  } else {
#  define CLT \
    SleepConditionVariableSRW(cond, &mutex->handle.srw_lock, timeout_ms, 0)
    CL;
#  undef CLT
  }

  return ETIMEDOUT;
}

static inline int sirius_cond_signal(sirius_cond_t *cond) {
  if (unlikely(!cond))
    return EINVAL;

  WakeConditionVariable(cond);
  return 0;
}

static inline int sirius_cond_broadcast(sirius_cond_t *cond) {
  if (unlikely(!cond))
    return EINVAL;

  WakeAllConditionVariable(cond);
  return 0;
}

#else

static inline int sirius_cond_init(sirius_cond_t *__restrict cond,
                                   const sirius_cond_type_t *__restrict type) {
  if (unlikely(!cond))
    return EINVAL;

  /**
   * @note If no type is specified or if we want process private (default),
   * we can use the simple initialization.
   */
  if (!type || *type == sirius_cond_process_private)
    return pthread_cond_init(cond, NULL);

  int ret;
  pthread_condattr_t attr;

  ret = pthread_condattr_init(&attr);
  if (ret != 0)
    return ret;

  ret = pthread_condattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
  if (ret != 0) {
    pthread_condattr_destroy(&attr);
    return ret;
  }

  ret = pthread_cond_init(cond, &attr);
  pthread_condattr_destroy(&attr);
  return ret;
}

static inline int sirius_cond_destroy(sirius_cond_t *cond) {
  if (unlikely(!cond))
    return EINVAL;
  return pthread_cond_destroy(cond);
}

static inline int sirius_cond_wait(sirius_cond_t *__restrict cond,
                                   sirius_mutex_t *__restrict mutex) {
  if (unlikely(!cond || !mutex))
    return EINVAL;
  return pthread_cond_wait(cond, mutex);
}

static inline int sirius_cond_timedwait(sirius_cond_t *__restrict cond,
                                        sirius_mutex_t *__restrict mutex,
                                        uint64_t milliseconds) {
  if (unlikely(!cond || !mutex))
    return EINVAL;

  struct timespec ts;
#  if defined(CLOCK_REALTIME)
  if (unlikely(clock_gettime(CLOCK_REALTIME, &ts) != 0))
    return errno;
#  else
  time_t now = time(NULL);
  if (unlikely(now == (time_t)-1))
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

  return pthread_cond_timedwait(cond, mutex, &ts);
}

static inline int sirius_cond_signal(sirius_cond_t *cond) {
  if (unlikely(!cond))
    return EINVAL;
  return pthread_cond_signal(cond);
}

static inline int sirius_cond_broadcast(sirius_cond_t *cond) {
  if (unlikely(!cond))
    return EINVAL;
  return pthread_cond_broadcast(cond);
}

#endif

#endif // SIRIUS_COND_H
