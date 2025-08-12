/**
 * @name sirius_cond.h
 *
 * @author UsadaYu
 *
 * @date
 *  Create: 2025-01-06
 *  Update: 2025-01-21
 *
 * @brief Condition.
 */

#ifndef __SIRIUS_COND_H__
#define __SIRIUS_COND_H__

#include "sirius_common.h"
#include "sirius_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
typedef CONDITION_VARIABLE sirius_cond_handle;
#else
typedef pthread_cond_t sirius_cond_handle;
#endif

typedef enum {
  /**
   * @brief Thread sharing within a single process.
   */
  sirius_cond_process_private = 0,

#ifndef _WIN32
  /**
   * @brief Sharing within multiple processes.
   *
   * @note Only supported under POSIX system.
   */
  sirius_cond_process_shared = 1,
#endif
} sirius_cond_attr_t;

/**
 * @brief Initialize a condition variable, which is used
 *  for thread synchronization.
 *
 * @param[out] handle: Condition handle.
 * @param[in] attr: Condition attribute, refer to the
 *  `sirius_cond_attr_t`.
 *
 * @return 0 on success, error code otherwise.
 */
int sirius_cond_init(sirius_cond_handle *handle,
                     const sirius_cond_attr_t *attr);

/**
 * @brief Destroy the condition handle.
 */
int sirius_cond_destroy(sirius_cond_handle *handle);

/**
 * @brief Block the current thread until another thread
 *  wakes it up.
 *
 * @param[in] handle: Condition handle.
 * @param[in] mutex: The mutex handle.
 *
 * @return 0 on success, error code otherwise.
 */
int sirius_cond_wait(sirius_cond_handle *handle,
                     sirius_mutex_handle *mutex);

/**
 * @brief Block the current thread until another thread
 *  wakes it up or out of the wait time.
 *
 * @param[in] handle: Condition handle.
 * @param[in] mutex: The mutex handle.
 * @param[in] milliseconds: Timeout duration, unit: ms.
 *
 * @return 0 on success;
 *
 *  `sirius_err_timeout` indicates a timeout;
 *
 *  error code otherwise.
 */
int sirius_cond_timedwait(sirius_cond_handle *handle,
                          sirius_mutex_handle *mutex,
                          unsigned long int milliseconds);

/**
 * @brief Wake up a blocked thread.
 *
 * @param[in] handle: Condition handle.
 *
 * @return 0 on success, error code otherwise.
 */
int sirius_cond_signal(sirius_cond_handle *handle);

/**
 * @brief Wake up all blocked threads.
 *
 * @param[in] handle: Condition handle.
 *
 * @return 0 on success, error code otherwise.
 */
int sirius_cond_broadcast(sirius_cond_handle *handle);

#ifdef __cplusplus
}
#endif

#endif  // __SIRIUS_COND_H__