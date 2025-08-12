/**
 * @name sirius_mutex.h
 *
 * @author UsadaYu
 *
 * @date
 *  Create: 2025-01-06
 *  Update: 2025-01-21
 *
 * @brief Mutex.
 */

#ifndef __SIRIUS_MUTEX_H__
#define __SIRIUS_MUTEX_H__

#include "sirius_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
typedef CRITICAL_SECTION sirius_mutex_handle;
#else
typedef pthread_mutex_t sirius_mutex_handle;
#endif

typedef enum {
  /**
   * @brief Default mutex, no deadlocks.
   */
  sirius_mutex_normal = 0,

#ifndef _WIN32
  /**
   * @brief Recursive locking.
   */
  sirius_mutex_recursive = 1,

  /**
   * @brief Error-checking mutex.
   */
  sirius_mutex_errorcheck = 2,
#endif
} sirius_mutex_attr_t;

/**
 * @brief Initialize a mutex.
 *
 * @param[out] handle: Mutex handle.
 * @param[in] attr: Mutex attribute.
 *
 * @return 0 on success, error code otherwise.
 */
int sirius_mutex_init(sirius_mutex_handle *handle,
                      const sirius_mutex_attr_t *attr);

/**
 * @brief Destroy the mutex handle.
 *
 * @param[in] handle: Mutex handle.
 *
 * @return 0 on success, error code otherwise.
 */
int sirius_mutex_destroy(sirius_mutex_handle *handle);

/**
 * @brief Lock a mutex.
 *
 * @param[in] handle: Mutex handle.
 *
 * @return 0 on success, error code otherwise.
 */
int sirius_mutex_lock(sirius_mutex_handle *handle);

/**
 * @brief Unlock a mutex.
 *
 * @param[in] handle: Mutex handle.
 *
 * @return 0 on success, error code otherwise.
 */
int sirius_mutex_unlock(sirius_mutex_handle *handle);

/**
 * @brief Try to lock a mutex without blocking.
 *
 * @param[in] handle: Mutex handle.
 *
 * @return 0 on success, the `sirius_mutex_unlock` function
 *  is then called to unlock;
 *
 *  `sirius_err_resource_alloc` indicates that an attempt
 *  to obtain a lock failed;
 *
 *  error code otherwise.
 */
int sirius_mutex_trylock(sirius_mutex_handle *handle);

#ifdef __cplusplus
}
#endif

#endif  // __SIRIUS_MUTEX_H__
