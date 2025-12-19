#ifndef SIRIUS_MUTEX_H
#define SIRIUS_MUTEX_H

#include "sirius/sirius_attributes.h"
#include "sirius/sirius_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Defines the behavior type of the mutex.
 */
typedef enum {
  /**
   * @brief Default, non-recursive mutex.
   * On Windows, this uses `SRWLOCK` for high performance.
   * On POSIX, this uses `PTHREAD_MUTEX_NORMAL`.
   */
  sirius_mutex_normal = 0,

  /**
   * @brief A recursive mutex. The same thread can lock it multiple times.
   * On Windows, this uses `CRITICAL_SECTION`.
   * On POSIX, this uses `PTHREAD_MUTEX_RECURSIVE`.
   */
  sirius_mutex_recursive = 1,
} sirius_mutex_type_t;

#if defined(_WIN32) || defined(_WIN64)
/**
 * @brief Cross-platform mutex handle.
 * On Windows, this is a struct to accommodate different underlying lock types.
 */
typedef struct {
  sirius_mutex_type_t type;
  union {
    SRWLOCK srw_lock;
    CRITICAL_SECTION critical_section;
  } handle;
} sirius_mutex_t;
#else
/**
 * @brief Cross-platform mutex handle.
 * On POSIX, this is a direct wrapper around `pthread_mutex_t`.
 */
typedef pthread_mutex_t sirius_mutex_t;
#endif

/**
 * @brief Initialize a mutex.
 *
 * @param[out] mutex A pointer to the mutex object to be initialized.
 * @param[in] type  A pointer to the mutex type. If `nullptr`, a default
 * (normal) mutex is created.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
#if defined(_WIN32) || defined(_WIN64)
static inline
#else
sirius_api
#endif
  int sirius_mutex_init(sirius_mutex_t *__restrict mutex,
                        const sirius_mutex_type_t *__restrict type);

/**
 * @brief Destroy the mutex.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
static inline int sirius_mutex_destroy(sirius_mutex_t *mutex);

/**
 * @brief Lock the mutex.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
static inline int sirius_mutex_lock(sirius_mutex_t *mutex);

/**
 * @brief Unlock the mutex.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
static inline int sirius_mutex_unlock(sirius_mutex_t *mutex);

/**
 * @brief Try to lock the mutex without blocking.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
static inline int sirius_mutex_trylock(sirius_mutex_t *mutex);

#ifdef __cplusplus
}
#endif

/**
 * @implements
 */

#if defined(_WIN32) || defined(_WIN64)

static inline int sirius_mutex_init(sirius_mutex_t *mutex,
                                    const sirius_mutex_type_t *type) {
  if (unlikely(!mutex))
    return EINVAL;

  sirius_mutex_type_t mutex_type = (type) ? *type : sirius_mutex_normal;
  mutex->type = mutex_type;

  if (mutex_type == sirius_mutex_recursive) {
/**
 * @note `CRITICAL_SECTION` can raise an exception on low memory, but
 * doesn't return an error. Here use structured exception handling to be
 * safe, though it's rare.
 */
#  ifdef _MSC_VER
    __try {
#  endif
      InitializeCriticalSection(&mutex->handle.critical_section);
#  ifdef _MSC_VER
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      return ENOMEM;
    }
#  endif
  } else { // sirius_mutex_normal
    InitializeSRWLock(&mutex->handle.srw_lock);
  }
  return 0;
}

static inline int sirius_mutex_destroy(sirius_mutex_t *mutex) {
  if (unlikely(!mutex))
    return EINVAL;

  /**
   * @note `CRITICAL_SECTION` requires `DeleteCriticalSection`; `SRWLOCK`
   * doesn't need explicit destruction.
   */
  if (mutex->type == sirius_mutex_recursive) {
    DeleteCriticalSection(&mutex->handle.critical_section);
  } else {
    /**
     * @note SRWLOCK: no-op.
     */
    (void)mutex;
  }
  return 0;
}

static inline int sirius_mutex_lock(sirius_mutex_t *mutex) {
  if (unlikely(!mutex))
    return EINVAL;

  if (mutex->type == sirius_mutex_recursive) {
    EnterCriticalSection(&mutex->handle.critical_section);
  } else {
    AcquireSRWLockExclusive(&mutex->handle.srw_lock);
  }
  return 0;
}

static inline int sirius_mutex_unlock(sirius_mutex_t *mutex) {
  if (unlikely(!mutex))
    return EINVAL;

  if (mutex->type == sirius_mutex_recursive) {
    LeaveCriticalSection(&mutex->handle.critical_section);
  } else {
    ReleaseSRWLockExclusive(&mutex->handle.srw_lock);
  }
  return 0;
}

static inline int sirius_mutex_trylock(sirius_mutex_t *mutex) {
  if (unlikely(!mutex))
    return EINVAL;

  if (mutex->type == sirius_mutex_recursive) {
    BOOL ok = TryEnterCriticalSection(&mutex->handle.critical_section);
    return ok ? 0 : EBUSY;
  } else {
    BOOLEAN ok = TryAcquireSRWLockExclusive(&mutex->handle.srw_lock);
    return ok ? 0 : EBUSY;
  }
}

#else

static inline int sirius_mutex_destroy(sirius_mutex_t *mutex) {
  if (unlikely(!mutex))
    return EINVAL;
  return pthread_mutex_destroy(mutex);
}

static inline int sirius_mutex_lock(sirius_mutex_t *mutex) {
  if (unlikely(!mutex))
    return EINVAL;
  return pthread_mutex_lock(mutex);
}

static inline int sirius_mutex_unlock(sirius_mutex_t *mutex) {
  if (unlikely(!mutex))
    return EINVAL;
  return pthread_mutex_unlock(mutex);
}

static inline int sirius_mutex_trylock(sirius_mutex_t *mutex) {
  if (unlikely(!mutex))
    return EINVAL;
  return pthread_mutex_trylock(mutex);
}

#endif

#endif // SIRIUS_MUTEX_H
