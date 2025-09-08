/**
 * @name sirius_sem.h
 *
 * @author UsadaYu
 *
 * @date
 *  Create: 2025-01-06
 *  Update: 2025-01-25
 *
 * @brief Semaphore.
 */

#ifndef SIRIUS_SEM_H
#define SIRIUS_SEM_H

#include "sirius_attributes.h"
#include "sirius_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
typedef HANDLE sirius_sem_handle;
#else
#include <semaphore.h>
typedef sem_t sirius_sem_handle;
#endif

/**
 * @brief Initialize a semaphore.
 *
 * @param[out] handle: Semaphore handle.
 * @param[in] pshared:
 *  In Windows system, this parameter should be set to 0.
 *  In POSIX system, if `pshared` has the value 0, then the
 *  semaphore is shared between the threads of a process;
 *  if `pshared` is nonzero, then the semaphore is shared
 *  between processes.
 * @param[in] value: The `value` argument specifies the
 *  initial value for the semaphore.
 *
 * @return 0 on success, error code otherwise.
 */
sirius_api int sirius_sem_init(sirius_sem_handle *handle,
                               int pshared,
                               unsigned int value);

/**
 * @brief Destroy the semaphore.
 *
 * @param[in] handle: Semaphore handle.
 *
 * @return 0 on success, error code otherwise.
 */
sirius_api int sirius_sem_destroy(
    sirius_sem_handle *handle);

/**
 * @brief If the semaphore's value is greater than zero,
 *  then the decrement proceeds, and the function returns
 *  immediately. If the semaphore currently has the value
 *  zero, then the call blocks until either it becomes
 *  possible to perform the decrement (i.e., the semaphore
 *  value rises above zero), or a signal handler interrupts
 *  the call.
 *
 * @param[in] handle: Semaphore handle.
 *
 * @return 0 on success, error code otherwise.
 */
sirius_api int sirius_sem_wait(sirius_sem_handle *handle);

/**
 * @brief This function is the same as the function
 *  `sirius_sem_wait`, except that if the decrement cannot
 *  be immediately performed, then call returns an error
 *  (sirius_err_timeout) instead of blocking.
 *
 * @param[in] handle: Semaphore handle.
 *
 * @return
 *  - (1) 0 on success;
 *
 *  - (2) `sirius_err_timeout` indicates that obtaining
 *  semaphore fails;
 *
 *  - (3) error code otherwise.
 */
sirius_api int sirius_sem_trywait(
    sirius_sem_handle *handle);

/**
 * @brief This function is the same as the function
 *  `sirius_sem_wait`, except that `milliseconds` specifies
 *  a limit on the amount of time that the call should
 *  block if the decrement cannot be immediately performed.
 *
 * @param[in] handle: Semaphore handle.
 * @param[in] milliseconds: timeout duration, unit: ms.
 *
 * @return
 *  - (1) 0 on success;
 *
 *  - (2) `sirius_err_timeout` indicates that obtaining
 *  semaphore fails;
 *
 *  - (3) error code otherwise.
 */
sirius_api int sirius_sem_timedwait(
    sirius_sem_handle *handle, uint64_t milliseconds);

/**
 * @brief Increments the semaphore pointed to by sem.
 *
 * @param[in] handle: Semaphore handle.
 *
 * @return 0 on success, error code otherwise.
 */
sirius_api int sirius_sem_post(sirius_sem_handle *handle);

#ifndef _WIN32
/**
 * @brief Places the current value of the semaphore pointed
 *  to sem into the integer pointed to by `sval`.
 *
 * @param[in] handle: Semaphore handle.
 * @param[out] sval: The value of the current semaphore.
 *
 * @return 0 on success, error code otherwise.
 */
sirius_api int sirius_sem_getvalue(
    sirius_sem_handle *handle, int *sval);
#else
#define sirius_sem_getvalue(handle, sval) (0)
#endif

#ifdef __cplusplus
}
#endif

#endif  // SIRIUS_SEM_H
