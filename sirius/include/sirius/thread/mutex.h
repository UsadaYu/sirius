#pragma once

#include "sirius/attributes.h"
#include "sirius/internal/common.h"

typedef struct {
  sirius_alignas(void *) unsigned char __data[64];
} sirius_mutex_t;

#define SIRIUS_MUTEX_INITIALIZER {{0}}

#ifdef __cplusplus
extern "C" {
#endif

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

/**
 * @brief Initialize a mutex.
 *
 * @param[out] mutex A pointer to the mutex object to be initialized.
 * @param[in] type  A pointer to the mutex type. If `nullptr`, a default
 * (normal) mutex is created.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_mutex_init(sirius_mutex_t *__restrict mutex,
                                 const sirius_mutex_type_t *__restrict type);

/**
 * @brief Destroy the mutex.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_mutex_destroy(sirius_mutex_t *mutex);

/**
 * @brief Lock the mutex.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_mutex_lock(sirius_mutex_t *mutex);

/**
 * @brief Unlock the mutex.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_mutex_unlock(sirius_mutex_t *mutex);

/**
 * @brief Try to lock the mutex without blocking.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_mutex_trylock(sirius_mutex_t *mutex);

#ifdef __cplusplus
}
#endif
