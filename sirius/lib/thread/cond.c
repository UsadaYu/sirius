#include "sirius/thread/cond.h"

#include "thread/internal/mutex.h"
#include "utils/errno.h"

#if defined(_WIN32) || defined(_WIN64)
internal_check_sizeof(sirius_cond_t, CONDITION_VARIABLE);
internal_check_alignof(sirius_cond_t, CONDITION_VARIABLE);
#else
internal_check_sizeof(sirius_cond_t, pthread_cond_t);
internal_check_alignof(sirius_cond_t, pthread_cond_t);
#endif

#if defined(_WIN32) || defined(_WIN64)

sirius_api int sirius_cond_init(sirius_cond_t *__restrict cond,
                                const sirius_cond_type_t *__restrict type) {
  /**
   * @note Windows condition variables don't support process sharing and
   * don't have configurable attributes, so here ignore the type parameter.
   */
  (void)type;
  InitializeConditionVariable((CONDITION_VARIABLE *)cond);
  return 0;
}

sirius_api int sirius_cond_destroy(sirius_cond_t *cond) {
  /**
   * @note Windows condition variables don't require explicit destruction.
   */
  (void)cond;
  return 0;
}

sirius_api int sirius_cond_wait(sirius_cond_t *__restrict cond,
                                sirius_mutex_t *__restrict mutex) {
  CONDITION_VARIABLE *cv = (CONDITION_VARIABLE *)cond;
  sirius_mutex_s *m = (sirius_mutex_s *)mutex;

  if (m->type == sirius_mutex_recursive) {
    if (!SleepConditionVariableCS(cv, &m->handle.critical_section, INFINITE)) {
      return internal_winerr_to_errno(GetLastError());
    }
  } else {
    if (!SleepConditionVariableSRW(cv, &m->handle.srw_lock, INFINITE, 0)) {
      return internal_winerr_to_errno(GetLastError());
    }
  }

  return 0;
}

sirius_api int sirius_cond_timedwait(sirius_cond_t *__restrict cond,
                                     sirius_mutex_t *__restrict mutex,
                                     uint64_t milliseconds) {
  CONDITION_VARIABLE *cv = (CONDITION_VARIABLE *)cond;
  sirius_mutex_s *m = (sirius_mutex_s *)mutex;
  DWORD timeout_ms;
  uint64_t tm_prev, tm_cur, tm_elapsed;

  tm_prev = GetTickCount64();

#  define E \
    do { \
      while (milliseconds > 0) { \
        timeout_ms = milliseconds >= (uint64_t)(INFINITE - 1) \
          ? INFINITE - 1 \
          : (DWORD)milliseconds; \
        if (C) { \
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
          return internal_winerr_to_errno(dw_err); \
        } \
      } \
    } while (0)

  if (m->type == sirius_mutex_recursive) {
#  define C \
    SleepConditionVariableCS(cv, &m->handle.critical_section, timeout_ms)
    E;
#  undef C
  } else {
#  define C SleepConditionVariableSRW(cv, &m->handle.srw_lock, timeout_ms, 0)
    E;
#  undef C
  }

#  undef E

  return ETIMEDOUT;
}

sirius_api int sirius_cond_signal(sirius_cond_t *cond) {
  WakeConditionVariable((CONDITION_VARIABLE *)cond);

  return 0;
}

sirius_api int sirius_cond_broadcast(sirius_cond_t *cond) {
  WakeAllConditionVariable((CONDITION_VARIABLE *)cond);

  return 0;
}

#else

sirius_api int sirius_cond_init(sirius_cond_t *__restrict cond,
                                const sirius_cond_type_t *__restrict type) {
  /**
   * @note If no type is specified or if we want process private (default),
   * we can use the simple initialization.
   */
  if (!type || *type == sirius_cond_process_private)
    return pthread_cond_init((pthread_cond_t *)cond, nullptr);

  int ret;
  pthread_condattr_t attr;

  ret = pthread_condattr_init(&attr);
  if (ret)
    return ret;

  ret = pthread_condattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
  if (ret) {
    pthread_condattr_destroy(&attr);
    return ret;
  }

  ret = pthread_cond_init((pthread_cond_t *)cond, &attr);
  pthread_condattr_destroy(&attr);
  return ret;
}

sirius_api int sirius_cond_destroy(sirius_cond_t *cond) {
  return pthread_cond_destroy((pthread_cond_t *)cond);
}

sirius_api int sirius_cond_wait(sirius_cond_t *__restrict cond,
                                sirius_mutex_t *__restrict mutex) {
  return pthread_cond_wait((pthread_cond_t *)cond, (pthread_mutex_t *)mutex);
}

sirius_api int sirius_cond_timedwait(sirius_cond_t *__restrict cond,
                                     sirius_mutex_t *__restrict mutex,
                                     uint64_t milliseconds) {
  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts))
    return errno;

  ts.tv_sec += (time_t)(milliseconds / 1000);
  ts.tv_nsec += (long)((milliseconds % 1000) * 1000000L);
  if (ts.tv_nsec >= 1000000000L) {
    ts.tv_sec += ts.tv_nsec / 1000000000L;
    ts.tv_nsec = ts.tv_nsec % 1000000000L;
  }

  return pthread_cond_timedwait((pthread_cond_t *)cond,
                                (pthread_mutex_t *)mutex, &ts);
}

sirius_api int sirius_cond_signal(sirius_cond_t *cond) {
  return pthread_cond_signal((pthread_cond_t *)cond);
}

sirius_api int sirius_cond_broadcast(sirius_cond_t *cond) {
  return pthread_cond_broadcast((pthread_cond_t *)cond);
}

#endif
