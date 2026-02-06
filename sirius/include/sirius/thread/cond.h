/**
 * @note
 * - (1) `enum SiriusThreadProcess`: On Windows, inter-process sharing is not
 * supported.
 */

#pragma once

#include "sirius/attributes.h"
#include "sirius/thread/macro.h"
#include "sirius/thread/mutex.h"

typedef struct {
  sirius_alignas(void *) unsigned char __data[64];
} sirius_cond_t;

#ifdef __cplusplus
extern "C" {
#endif

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
sirius_api int
sirius_cond_init(sirius_cond_t *__restrict cond,
                 const enum SiriusThreadProcess *__restrict type);

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
