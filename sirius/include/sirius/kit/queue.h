#pragma once

#include "sirius/attributes.h"
#include "sirius/internal/common.h"
#include "sirius/macro.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sirius_queue_t sirius_queue_t;

enum SiriusQueueType {
  /**
   * @brief Queue with mutex, default.
   */
  kSiriusQueueTypeMutex = 0,

  /**
   * @brief Queue without mutex.
   */
  kSiriusQueueTypeNoMutex = 1,
};

typedef struct {
  /**
   * @brief Number of queue members.
   */
  size_t elem_count;

  /**
   * @brief Mechanism in the queue, refer to `enum SiriusQueueType`.
   */
  enum SiriusQueueType queue_type;
} sirius_queue_args_t;

/**
 * @brief Allocate a queue handle, the resulting handle must be deleted using
 * `sirius_queue_free`.
 *
 * @param[out] queue Queue handle.
 * @param[in] args Queue creation parameters.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_queue_alloc(sirius_queue_t **__restrict queue,
                                  const sirius_queue_args_t *__restrict args);

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
 * the queue with mutex. Setting the value to `SIRIUS_TIMEOUT_NO_WAITING` means no
 * wait, and setting it to `SIRIUS_TIMEOUT_INFINITE` means infinite wait.
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
 * the queue with mutex. Setting the value to `SIRIUS_TIMEOUT_NO_WAITING` (0) means no
 * wait, and setting it to `SIRIUS_TIMEOUT_INFINITE` (UINT64_MAX) means infinite
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
sirius_api int sirius_queue_nb_cache(sirius_queue_t *queue, size_t *num);

#ifdef __cplusplus
}
#endif
