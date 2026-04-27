#pragma once

#include "sirius/attributes.h"
#include "sirius/inner/common.h"

typedef struct {
  ss_alignas(void *) unsigned char __data[32];
} ss_sem_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize a semaphore.
 *
 * @param[out] sem A pointer to the semaphore object to be initialized.
 * @param[in] pshared On POSIX, if non-zero the semaphore is shared between
 * processes; invalid on Windows.
 * @param[in] value Initial value of the semaphore.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
SIRIUS_API int ss_sem_init(ss_sem_t *sem, int pshared, unsigned int value);

/**
 * @brief Destroy the semaphore.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
SIRIUS_API int ss_sem_destroy(ss_sem_t *sem);

/**
 * @brief Wait the semaphore.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
SIRIUS_API int ss_sem_wait(ss_sem_t *sem);

/**
 * @brief Try to wait the semaphore.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
SIRIUS_API int ss_sem_trywait(ss_sem_t *sem);

/**
 * @brief Wait the semaphore, but limit the waiting time.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
SIRIUS_API int ss_sem_timedwait(ss_sem_t *sem, uint64_t milliseconds);

/**
 * @brief Post the semaphore.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
SIRIUS_API int ss_sem_post(ss_sem_t *sem);

#if defined(_WIN32) || defined(_WIN64)
/**
 * @note Not a real implementation on Windows.
 */
#  define ss_sem_getvalue(sem, sval) (0)
#else
/**
 * @brief Obtain the current semaphore value.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
SIRIUS_API int ss_sem_getvalue(ss_sem_t *__restrict sem, int *__restrict sval);
#endif

#ifdef __cplusplus
}
#endif
