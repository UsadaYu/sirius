/**
 * @name sirius_spinlock.h
 *
 * @author UsadaYu
 *
 * @date create: 2025-01-11
 * @date update: 2025-03-04
 *
 * @brief Spin lock.
 *
 * @note Spinlock is a lightweight lock, so it is not
 *  encapsulated by a function.
 */

#ifndef __SIRIUS_SPINLOCK__
#define __SIRIUS_SPINLOCK__

#include "sirius_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _MSC_VER
/**
 * @brief Spin locks are shared across multiple threads of
 *  multiple processes.
 */
#define SIRIUS_THREAD_PROCESS_PRIVATE \
  PTHREAD_PROCESS_PRIVATE
/**
 * @brief The spin lock is shared within multiple threads
 *  of the current process.
 */
#define SIRIUS_THREAD_PROCESS_SHARED PTHREAD_PROCESS_SHARED

typedef pthread_spinlock_t sirius_spinlock_t;

#define sirius_spin_init(lock, pshared) \
  pthread_spin_init(lock, pshared)
#define sirius_spin_lock(lock) pthread_spin_lock(lock)
#define sirius_spin_unlock(lock) pthread_spin_unlock(lock)
#define sirius_spin_destroy(lock) \
  pthread_spin_destroy(lock)

#else
typedef volatile LONG sirius_spinlock_t;

#define SIRIUS_THREAD_PROCESS_PRIVATE (0)

#define sirius_spin_init(lock, pshared) (*(lock) = 0, 0)
#define sirius_spin_lock(lock)                          \
  while (InterlockedCompareExchange(lock, 1, 0) != 0) { \
    YieldProcessor();                                   \
  }
#define sirius_spin_unlock(lock) \
  InterlockedExchange(lock, 0)
#define sirius_spin_destroy(lock) (*(lock) = 0, 0)

#endif

#ifdef __cplusplus
}
#endif

#endif  // __SIRIUS_SPINLOCK__
