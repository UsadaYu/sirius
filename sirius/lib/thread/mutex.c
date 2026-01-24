#include "thread/internal/mutex.h"

#if defined(_WIN32) || defined(_WIN64)

sirius_api int sirius_mutex_init(sirius_mutex_t *mutex,
                                 const sirius_mutex_type_t *type) {
  sirius_mutex_s *m = (sirius_mutex_s *)mutex;
  sirius_mutex_type_t mt = type ? *type : sirius_mutex_normal;

  m->type = mt;

  if (mt == sirius_mutex_recursive) {
/**
 * @note `CRITICAL_SECTION` can raise an exception on low memory, but
 * doesn't return an error. Here use structured exception handling to be
 * safe, though it's rare.
 */
#  ifdef _MSC_VER
    __try {
#  endif
      InitializeCriticalSection(&m->handle.critical_section);
#  ifdef _MSC_VER
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      return ENOMEM;
    }
#  endif
  } else { // sirius_mutex_normal
    InitializeSRWLock(&m->handle.srw_lock);
  }

  return 0;
}

sirius_api int sirius_mutex_destroy(sirius_mutex_t *mutex) {
  sirius_mutex_s *m = (sirius_mutex_s *)mutex;

  /**
   * @note `CRITICAL_SECTION` requires `DeleteCriticalSection`; `SRWLOCK`
   * doesn't need explicit destruction.
   */
  if (m->type == sirius_mutex_recursive) {
    DeleteCriticalSection(&m->handle.critical_section);
  } else {
    /**
     * @note SRWLOCK: no-op.
     */
    (void)m;
  }

  return 0;
}

sirius_api int sirius_mutex_lock(sirius_mutex_t *mutex) {
  sirius_mutex_s *m = (sirius_mutex_s *)mutex;

  if (m->type == sirius_mutex_recursive) {
    EnterCriticalSection(&m->handle.critical_section);
  } else {
    AcquireSRWLockExclusive(&m->handle.srw_lock);
  }

  return 0;
}

sirius_api int sirius_mutex_unlock(sirius_mutex_t *mutex) {
  sirius_mutex_s *m = (sirius_mutex_s *)mutex;

  if (m->type == sirius_mutex_recursive) {
    LeaveCriticalSection(&m->handle.critical_section);
  } else {
    ReleaseSRWLockExclusive(&m->handle.srw_lock);
  }

  return 0;
}

sirius_api int sirius_mutex_trylock(sirius_mutex_t *mutex) {
  sirius_mutex_s *m = (sirius_mutex_s *)mutex;

  if (m->type == sirius_mutex_recursive) {
    BOOL ok = TryEnterCriticalSection(&m->handle.critical_section);
    return ok ? 0 : EBUSY;
  } else {
    BOOLEAN ok = TryAcquireSRWLockExclusive(&m->handle.srw_lock);
    return ok ? 0 : EBUSY;
  }
}

#else

sirius_api int sirius_mutex_init(sirius_mutex_t *mutex,
                                 const sirius_mutex_type_t *type) {
  int ret;
  pthread_mutexattr_t attr;

  if (!type)
    return pthread_mutex_init((pthread_mutex_t *)mutex, nullptr);

  ret = pthread_mutexattr_init(&attr);
  if (ret)
    return ret;

  int mt = *type == sirius_mutex_recursive ? PTHREAD_MUTEX_RECURSIVE
                                           : PTHREAD_MUTEX_NORMAL;

  ret = pthread_mutexattr_settype(&attr, mt);
  if (ret) {
    pthread_mutexattr_destroy(&attr);
    return ret;
  }

  ret = pthread_mutex_init((pthread_mutex_t *)mutex, &attr);
  pthread_mutexattr_destroy(&attr);
  return ret;
}

sirius_api int sirius_mutex_destroy(sirius_mutex_t *mutex) {
  return pthread_mutex_destroy((pthread_mutex_t *)mutex);
}

sirius_api int sirius_mutex_lock(sirius_mutex_t *mutex) {
  return pthread_mutex_lock((pthread_mutex_t *)mutex);
}

sirius_api int sirius_mutex_unlock(sirius_mutex_t *mutex) {
  return pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

sirius_api int sirius_mutex_trylock(sirius_mutex_t *mutex) {
  return pthread_mutex_trylock((pthread_mutex_t *)mutex);
}

#endif
