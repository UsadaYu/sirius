/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

#include "lib/thread/inner/mutex.h"

#if defined(_WIN32) || defined(_WIN64)
SIRIUS_API int ss_mutex_init(ss_mutex_t *mutex, const enum SsMutexType *type) {
  ss_mutex_s *m = (ss_mutex_s *)mutex;
  enum SsMutexType mt = type ? *type : kSsMutexTypeNormal;
  m->type = mt;

  if (mt == kSsMutexTypeRecursive) {
/**
 * @note `CRITICAL_SECTION` can raise an exception on low memory, but doesn't
 * return an error. Here use structured exception handling to be safe, though
 * it's rare.
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
  } else { // kSsMutexTypeNormal
    InitializeSRWLock(&m->handle.srw_lock);
  }
  return 0;
}

SIRIUS_API int ss_mutex_destroy(ss_mutex_t *mutex) {
  ss_mutex_s *m = (ss_mutex_s *)mutex;

  /**
   * @note `CRITICAL_SECTION` requires `DeleteCriticalSection`; `SRWLOCK`
   * doesn't need explicit destruction.
   */
  if (m->type == kSsMutexTypeRecursive) {
    DeleteCriticalSection(&m->handle.critical_section);
  } else {
    /**
     * @note SRWLOCK: no-op.
     */
    (void)m;
  }
  return 0;
}

SIRIUS_API int ss_mutex_lock(ss_mutex_t *mutex) {
  ss_mutex_s *m = (ss_mutex_s *)mutex;
  if (m->type == kSsMutexTypeRecursive) {
    EnterCriticalSection(&m->handle.critical_section);
  } else {
    AcquireSRWLockExclusive(&m->handle.srw_lock);
  }
  return 0;
}

SIRIUS_API int ss_mutex_unlock(ss_mutex_t *mutex) {
  ss_mutex_s *m = (ss_mutex_s *)mutex;
  if (m->type == kSsMutexTypeRecursive) {
    LeaveCriticalSection(&m->handle.critical_section);
  } else {
    ReleaseSRWLockExclusive(&m->handle.srw_lock);
  }
  return 0;
}

SIRIUS_API int ss_mutex_trylock(ss_mutex_t *mutex) {
  ss_mutex_s *m = (ss_mutex_s *)mutex;
  if (m->type == kSsMutexTypeRecursive) {
    BOOL ok = TryEnterCriticalSection(&m->handle.critical_section);
    return ok ? 0 : EBUSY;
  } else {
    BOOLEAN ok = TryAcquireSRWLockExclusive(&m->handle.srw_lock);
    return ok ? 0 : EBUSY;
  }
}
#else
SIRIUS_API int ss_mutex_init(ss_mutex_t *mutex, const enum SsMutexType *type) {
  int ret;
  pthread_mutexattr_t attr;

  if (!type)
    return pthread_mutex_init((pthread_mutex_t *)mutex, nullptr);

  ret = pthread_mutexattr_init(&attr);
  if (ret)
    return ret;

  int mt = *type == kSsMutexTypeRecursive ? PTHREAD_MUTEX_RECURSIVE
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

SIRIUS_API int ss_mutex_destroy(ss_mutex_t *mutex) {
  return pthread_mutex_destroy((pthread_mutex_t *)mutex);
}

SIRIUS_API int ss_mutex_lock(ss_mutex_t *mutex) {
  return pthread_mutex_lock((pthread_mutex_t *)mutex);
}

SIRIUS_API int ss_mutex_unlock(ss_mutex_t *mutex) {
  return pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

SIRIUS_API int ss_mutex_trylock(ss_mutex_t *mutex) {
  return pthread_mutex_trylock((pthread_mutex_t *)mutex);
}
#endif
