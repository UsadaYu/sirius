#ifndef SIRIUS_SPINLOCK_H
#define SIRIUS_SPINLOCK_H

#include "sirius/sirius_common.h"

#if defined(_MSC_VER)
#  include <intrin.h>
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && \
  !defined(__STDC_NO_ATOMICS__)
#  include <stdatomic.h>
#  include <stdbool.h>
#else
/**
 * @note If c11 is not supported and it is not MSVC, try using the built-in
 * function gcc/clang, which allows compilation under `std=c99`.
 */
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @note Define shared attribute constants (for compatibility with API
 * signatures, but usually ignored in atomic lock implementations)
 */
#define SIRIUS_THREAD_PROCESS_PRIVATE 0
#define SIRIUS_THREAD_PROCESS_SHARED 1

/**
 * @implements
 */

#if defined(_MSC_VER)
#  define _SIRIUS_CPU_RELAX() YieldProcessor()
#elif defined(__SSE2__) || defined(__x86_64__) || defined(__i386__)
#  include <emmintrin.h>
#  define _SIRIUS_CPU_RELAX() _mm_pause()
#elif defined(__aarch64__)
#  define _SIRIUS_CPU_RELAX() __asm__ volatile("yield" ::: "memory")
#else
#  define _SIRIUS_CPU_RELAX() ((void)0)
#endif

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
      _SIRIUS_CPU_RELAX();
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
      _SIRIUS_CPU_RELAX();
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
      _SIRIUS_CPU_RELAX();
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
