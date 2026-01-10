#ifndef SIRIUS_COND_H
#define SIRIUS_COND_H

#include "sirius/sirius_attributes.h"
#include "sirius/sirius_mutex.h"

typedef struct {
  sirius_alignas(void *) unsigned char __data[64];
} sirius_cond_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  /**
   * @brief Thread sharing within a single process.
   */
  sirius_cond_process_private = 0,

#if defined(_WIN32) || defined(_WIN64)
  sirius_cond_process_shared = sirius_cond_process_private,
#else
  /**
   * @brief Sharing within multiple processes.
   *
   * @note Supported on POSIX systems only.
   */
  sirius_cond_process_shared = 1,
#endif
} sirius_cond_type_t;

/**
 * @brief Initializes a condition variable.
 *
 * @param[out] cond A pointer to the condition variable object to be
 * initialized.
 * @param[in] type A pointer to the condition variable type. If `nullptr`,
 * a default (process private) condition variable is created.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_cond_init(sirius_cond_t *__restrict cond,
                                const sirius_cond_type_t *__restrict type);

/**
 * @brief Destroy the condition variable.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_cond_destroy(sirius_cond_t *cond);

/**
 * @brief Wait the condition variable.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_cond_wait(sirius_cond_t *__restrict cond,
                                sirius_mutex_t *__restrict mutex);

/**
 * @brief Wait the condition variable, but limit the waiting time.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_cond_timedwait(sirius_cond_t *__restrict cond,
                                     sirius_mutex_t *__restrict mutex,
                                     uint64_t milliseconds);

/**
 * @brief Wake up a condition variable.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_cond_signal(sirius_cond_t *cond);

/**
 * @brief Wake up all condition variable.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_cond_broadcast(sirius_cond_t *cond);

#ifdef __cplusplus
}
#endif

#endif // SIRIUS_COND_H
