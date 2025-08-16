/**
 * @name sirius_queue.h
 *
 * @author UsadaYu
 *
 * @date
 *  Create: 2024-09-12
 *  Update: 2025-02-10
 *
 * @brief Queue.
 */

#ifndef SIRIUS_QUEUE_H
#define SIRIUS_QUEUE_H

#include <stddef.h>

#include "sirius_attributes.h"
#include "sirius_macro.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *sirius_que_handle;

typedef enum {
  /* Queue with mutex, default. */
  sirius_que_type_mtx = 0,

  /* Queue without mutex. */
  sirius_que_type_no_mtx = 1,
} sirius_que_type_t;

typedef struct {
  /* Number of queue members. */
  unsigned short elem_nr;

  /**
   * Mechanism in the queue, refer to `sirius_que_type_t`.
   */
  sirius_que_type_t que_type;
} sirius_que_t;

/**
 * @brief Allocate a queue handle, the resulting handle
 *  must be deleted using `sirius_que_free`.
 *
 * @param[in] cr: Queue creation parameters.
 * @param[out] handle: Queue handle.
 *
 * @return 0 on success, error code otherwise.
 */
sirius_api int sirius_que_alloc(sirius_que_t *cr,
                                sirius_que_handle *handle);

/**
 * @brief Free the queue handle.
 *
 * @param[in] handle: Queue handle.
 *
 * @return 0 on success, error code otherwise.
 */
sirius_api int sirius_que_free(sirius_que_handle handle);

/**
 * @brief Get an element from the queue.
 *
 * @param[in] handle: Queue handle.
 * @param[out] ptr: An obtained queue element.
 * @param[in] milliseconds: Timeout duration, unit: ms.
 *  Takes effect only when the queue with mutex.
 *  Setting the value to `sirius_timeout_none` means no
 *  wait, and setting it to `sirius_timeout_infinite` means
 *  infinite wait.
 *
 * @return
 *  - (1) 0 on success;
 *
 *  - (2) `sirius_err_timeout` on timeout;
 *
 *  - (3) error code otherwise.
 */
sirius_api int sirius_que_get(
    sirius_que_handle handle, size_t *ptr,
    unsigned long int milliseconds);

/**
 * @brief Put an element into the queue.
 *
 * @param[in] handle: Queue handle.
 * @param[out] ptr: The element which will be added to the
 *  queue.
 * @param[in] milliseconds: Timeout duration, unit: ms.
 *  Takes effect only when the queue with mutex.
 *  Setting the value to `sirius_timeout_none` means no
 *  wait, and setting it to `sirius_timeout_infinite` means
 *  infinite wait.
 *
 * @return
 *  - (1) 0 on success;
 *
 *  - (2) `sirius_err_timeout` on timeout;
 *
 *  - (3) error code otherwise.
 */
sirius_api int sirius_que_put(
    sirius_que_handle handle, size_t ptr,
    unsigned long int milliseconds);

/**
 * @brief Reset the queue, empty the cached elements.
 *
 * @param[in] handle: Queue handle.
 *
 * @return 0 on success, error code otherwise.
 */
sirius_api int sirius_que_reset(sirius_que_handle handle);

/**
 * @brief Get the number of members of the current queue
 *  cache.
 *
 * @param[in] handle: Queue handle.
 * @param[out] num: The number of members of the current
 *  queue cache.
 *
 * @return 0 on success, error code otherwise.
 */
sirius_api int sirius_que_cache_num(
    sirius_que_handle handle, size_t *num);

#ifdef __cplusplus
}
#endif

#endif  // SIRIUS_QUEUE_H
