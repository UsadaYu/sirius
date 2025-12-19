#ifndef SIRIUS_SPINLOCK_H
#define SIRIUS_SPINLOCK_H

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && \
  !defined(__STDC_NO_ATOMICS__)
#  include <stdatomic.h>
#  include <stdbool.h>
#endif

#include "sirius/sirius_cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @note Define shared attribute constants (for compatibility with API
 * signatures, but usually ignored in atomic lock implementations).
 */
#define SIRIUS_THREAD_PROCESS_PRIVATE 0
#define SIRIUS_THREAD_PROCESS_SHARED 1

#if defined(_MSC_VER)

typedef volatile long sirius_spinlock_t;

static inline int sirius_spin_init(sirius_spinlock_t *lock, int pshared) {
  (void)pshared;
  *lock = 0;
  return 0;
}

static inline int sirius_spin_destroy(sirius_spinlock_t *lock) {
  *lock = 0;
  return 0;
}

static inline int sirius_spin_lock(sirius_spinlock_t *lock) {
  while (_InterlockedCompareExchange(lock, 1, 0) != 0) {
    while (*lock != 0) {
      sirius_cpu_relax();
    }
  }
  return 0;
}

static inline int sirius_spin_unlock(sirius_spinlock_t *lock) {
  _InterlockedExchange(lock, 0);
  return 0;
}

#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && \
  !defined(__STDC_NO_ATOMICS__)

typedef _Atomic bool sirius_spinlock_t;

static inline int sirius_spin_init(sirius_spinlock_t *lock, int pshared) {
  (void)pshared;
  atomic_store_explicit(lock, false, memory_order_release);
  return 0;
}

static inline int sirius_spin_destroy(sirius_spinlock_t *lock) {
  (void)lock;
  return 0;
}

static inline int sirius_spin_lock(sirius_spinlock_t *lock) {
  while (atomic_exchange_explicit(lock, true, memory_order_acquire)) {
    while (atomic_load_explicit(lock, memory_order_relaxed)) {
      sirius_cpu_relax();
    }
  }
  return 0;
}

static inline int sirius_spin_unlock(sirius_spinlock_t *lock) {
  atomic_store_explicit(lock, false, memory_order_release);
  return 0;
}

#elif defined(__GNUC__) || defined(__clang__)

typedef volatile int sirius_spinlock_t;

static inline int sirius_spin_init(sirius_spinlock_t *lock, int pshared) {
  (void)pshared;
  *lock = 0;
  return 0;
}

static inline int sirius_spin_destroy(sirius_spinlock_t *lock) {
  return 0;
}

static inline int sirius_spin_lock(sirius_spinlock_t *lock) {
  while (__sync_lock_test_and_set(lock, 1)) {
    while (*lock) {
      sirius_cpu_relax();
    }
  }
  return 0;
}

static inline int sirius_spin_unlock(sirius_spinlock_t *lock) {
  __sync_lock_release(lock);
  return 0;
}

#else
#  error \
    "Sirius Spinlock: No atomic implementation available for this compiler/standard"
#endif

#ifdef __cplusplus
}
#endif

#endif // SIRIUS_SPINLOCK_H
