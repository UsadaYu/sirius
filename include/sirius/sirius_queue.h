#ifndef SIRIUS_QUEUE_H
#define SIRIUS_QUEUE_H

#include <stddef.h>

#include "sirius/sirius_attributes.h"
#include "sirius/sirius_macro.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sirius_queue_t sirius_queue_t;

typedef enum {
  /**
   * @brief Queue with mutex, default.
   */
  sirius_queue_type_mtx = 0,

  /**
   * @brief Queue without mutex.
   */
  sirius_queue_type_no_mtx = 1,
} sirius_queue_type_t;

typedef struct {
  /**
   * @brief Number of queue members.
   */
  size_t elem_count;

  /**
   * @brief Mechanism in the queue, refer to `sirius_queue_type_t`.
   */
  sirius_queue_type_t que_type;
} sirius_queue_arg_t;

/**
 * @brief Allocate a queue handle, the resulting handle must be deleted using
 * `sirius_queue_free`.
 *
 * @param[out] queue Queue handle.
 * @param[in] arg Queue creation parameters.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_queue_alloc(sirius_queue_t **__restrict queue,
                                  const sirius_queue_arg_t *__restrict arg);

/**
 * @brief Free the queue handle.
 *
 * @param[in] queue Queue handle.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_queue_free(sirius_queue_t *queue);

/**
 * @brief Get an element from the queue.
 *
 * @param[in] queue Queue handle.
 * @param[out] ptr An obtained queue element.
 * @param[in] milliseconds Timeout duration, unit: ms. Takes effect only when
 * the queue with mutex. Setting the value to `sirius_timeout_none` means no
 * wait, and setting it to `sirius_timeout_infinite` means infinite wait.
 *
 * @return
 * - (1) 0 on success;
 *
 * - (2) `ETIMEDOUT` on timeout;
 *
 * - (3) error code otherwise.
 */
sirius_api int sirius_queue_get(sirius_queue_t *queue, size_t *ptr,
                                uint64_t milliseconds);

/**
 * @brief Put an element into the queue.
 *
 * @param[in] queue Queue handle.
 * @param[out] ptr The element which will be added to the queue.
 * @param[in] milliseconds Timeout duration, unit: ms. Takes effect only when
 * the queue with mutex. Setting the value to `sirius_timeout_none` (0) means no
 * wait, and setting it to `sirius_timeout_infinite` (UINT64_MAX) means infinite
 * wait.
 *
 * @return
 * - (1) 0 on success;
 *
 * - (2) `ETIMEDOUT` on timeout;
 *
 * - (3) error code otherwise.
 */
sirius_api int sirius_queue_put(sirius_queue_t *queue, size_t ptr,
                                uint64_t milliseconds);

/**
 * @brief Reset the queue, empty the cached elements.
 *
 * @param[in] queue Queue handle.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_queue_reset(sirius_queue_t *queue);

/**
 * @brief Get the number of members of the current queue
 *  cache.
 *
 * @param[in] queue Queue handle.
 * @param[out] num The number of members of the current queue cache.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_queue_cache_num(sirius_queue_t *queue, size_t *num);

#ifdef __cplusplus
}
#endif

#endif // SIRIUS_QUEUE_H
