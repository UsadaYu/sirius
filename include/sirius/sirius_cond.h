/**
 * @name sirius_cond.h
 *
 * @author UsadaYu
 *
 * @date
 * Create: 2025-01-06
 * Update: 2025-01-21
 *
 * @brief Condition.
 */

#ifndef SIRIUS_COND_H
#define SIRIUS_COND_H

#include "sirius_attributes.h"
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
#else
  sirius_cond_process_shared = sirius_cond_process_private,
#endif
} sirius_cond_attr_t;

/**
 * @brief Initialize a condition variable, which is used for thread
 * synchronization.
 *
 * @param[out] handle Condition handle.
 * @param[in] attr Condition attribute, refer to the `sirius_cond_attr_t`.
 *
 * @return 0 on success, error code otherwise.
 */
sirius_api int sirius_cond_init(sirius_cond_handle *handle,
                                const sirius_cond_attr_t *attr);

/**
 * @brief Destroy the condition handle.
 */
sirius_api int sirius_cond_destroy(sirius_cond_handle *handle);

/**
 * @brief Block the current thread until another thread wakes it up.
 *
 * @param[in] handle Condition handle.
 * @param[in] mutex The mutex handle.
 *
 * @return 0 on success, error code otherwise.
 */
sirius_api int sirius_cond_wait(sirius_cond_handle *handle,
                                sirius_mutex_handle *mutex);

/**
 * @brief Block the current thread until another thread wakes it up or out of
 * the wait time.
 *
 * @param[in] handle Condition handle.
 * @param[in] mutex The mutex handle.
 * @param[in] milliseconds Timeout duration, unit: ms.
 *
 * @return
 * - (1) 0 on success;
 *
 * - (2) `sirius_err_timeout` indicates a timeout;
 *
 * - (3) error code otherwise.
 */
sirius_api int sirius_cond_timedwait(sirius_cond_handle *handle,
                                     sirius_mutex_handle *mutex,
                                     uint64_t milliseconds);

/**
 * @brief Wake up a blocked thread.
 *
 * @param[in] handle Condition handle.
 *
 * @return 0 on success, error code otherwise.
 */
sirius_api int sirius_cond_signal(sirius_cond_handle *handle);

/**
 * @brief Wake up all blocked threads.
 *
 * @param[in] handle Condition handle.
 *
 * @return 0 on success, error code otherwise.
 */
sirius_api int sirius_cond_broadcast(sirius_cond_handle *handle);

#ifdef __cplusplus
}
#endif

#endif // SIRIUS_COND_H
