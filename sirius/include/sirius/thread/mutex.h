#pragma once

#include "sirius/attributes.h"
#include "sirius/inner/common.h"

typedef struct {
  ss_alignas(void *) unsigned char __data[64];
} ss_mutex_t;

#define SS_MUTEX_INITIALIZER {{0}}

#ifdef __cplusplus
extern "C" {
#endif

enum SsMutexType {
  /**
   * @brief Default, non-recursive mutex.
   * On Windows, this uses `SRWLOCK` for high performance.
   * On POSIX, this uses `PTHREAD_MUTEX_NORMAL`.
   */
  kSsMutexTypeNormal = 0,

  /**
   * @brief A recursive mutex. The same thread can lock it multiple times.
   * On Windows, this uses `CRITICAL_SECTION`.
   * On POSIX, this uses `PTHREAD_MUTEX_RECURSIVE`.
   */
  kSsMutexTypeRecursive = 1,
};

/**
 * @brief Initialize a mutex.
 *
 * @param[out] mutex A pointer to the mutex object to be initialized.
 * @param[in] type  A pointer to the mutex type. If `nullptr`, a default
 * (normal) mutex is created.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
SIRIUS_API int ss_mutex_init(ss_mutex_t *__restrict mutex,
                             const enum SsMutexType *__restrict type);

/**
 * @brief Destroy the mutex.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
SIRIUS_API int ss_mutex_destroy(ss_mutex_t *mutex);

/**
 * @brief Lock the mutex.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
SIRIUS_API int ss_mutex_lock(ss_mutex_t *mutex);

/**
 * @brief Unlock the mutex.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
SIRIUS_API int ss_mutex_unlock(ss_mutex_t *mutex);

/**
 * @brief Try to lock the mutex without blocking.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
SIRIUS_API int ss_mutex_trylock(ss_mutex_t *mutex);

#ifdef __cplusplus
}
#endif
