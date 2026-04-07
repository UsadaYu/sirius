/**
 * @note
 * - (1) `enum SsThreadProcess`: On Windows, inter-process sharing is not
 * supported.
 */

#pragma once

#include "sirius/attributes.h"
#include "sirius/thread/macro.h"
#include "sirius/thread/mutex.h"

typedef struct {
  ss_alignas(void *) unsigned char __data[64];
} ss_cond_t;

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
sirius_api int ss_cond_init(ss_cond_t *__restrict cond,
                            const enum SsThreadProcess *__restrict type);

/**
 * @brief Destroy the condition variable.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int ss_cond_destroy(ss_cond_t *cond);

/**
 * @brief Wait the condition variable.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int ss_cond_wait(ss_cond_t *__restrict cond,
                            ss_mutex_t *__restrict mutex);

/**
 * @brief Wait the condition variable, but limit the waiting time.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int ss_cond_timedwait(ss_cond_t *__restrict cond,
                                 ss_mutex_t *__restrict mutex,
                                 uint64_t milliseconds);

/**
 * @brief Wake up a condition variable.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int ss_cond_signal(ss_cond_t *cond);

/**
 * @brief Wake up all condition variable.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int ss_cond_broadcast(ss_cond_t *cond);

#ifdef __cplusplus
}
#endif
