#include <sirius/c/time.h>
#include <sirius/cpu.h>
#include <sirius/thread/thread.h>

#include "internal/utils.h"

/**
 * @brief LOCK_TYPE: 0 - spinlock; 1 - mutex
 */
#ifndef LOCK_TYPE
#  define LOCK_TYPE 0
#endif

#if LOCK_TYPE == 0

#  include <sirius/thread/spinlock.h>

#  define lock_t sirius_spinlock_t
#  define str_lock_init "sirius_spin_init"
#  define str_lock_lock "sirius_spin_lock"
#  define str_lock_unlock "sirius_spin_unlock"
#  define str_lock_destory "sirius_spin_destroy"
#  define lock_init(lock) sirius_spin_init(&(lock), 0)
#  define lock_lock(lock) sirius_spin_lock(&(lock))
#  define lock_unlock(lock) sirius_spin_unlock(&(lock))
#  define lock_destory(lock) sirius_spin_destroy(&(lock))

#else

#  include <sirius/thread/mutex.h>

#  define lock_t sirius_mutex_t
#  define str_lock_init "sirius_mutex_init"
#  define str_lock_lock "sirius_mutex_lock"
#  define str_lock_unlock "sirius_mutex_unlock"
#  define str_lock_destory "sirius_mutex_destroy"
#  define lock_init(lock) sirius_mutex_init(&(lock), nullptr)
#  define lock_lock(lock) sirius_mutex_lock(&(lock))
#  define lock_unlock(lock) sirius_mutex_unlock(&(lock))
#  define lock_destory(lock) sirius_mutex_destroy(&(lock))

#endif

/**
 * @brief Basic function testing.
 */

static inline bool test_basic_functionality() {
  sirius_infosp("--- Basic function test begins ---\n");

  lock_t lock;

  if (lock_init(lock) != 0) {
    sirius_error("%s\n", str_lock_init);
    return false;
  }
  sirius_infosp("Successfully executed function `%s`\n", str_lock_init);

  if (lock_lock(lock) != 0) {
    sirius_error("%s\n", str_lock_lock);
    return false;
  }
  sirius_infosp("Successfully executed function `%s`\n", str_lock_lock);

  if (lock_unlock(lock) != 0) {
    sirius_error("%s\n", str_lock_unlock);
    return false;
  }
  sirius_infosp("Successfully executed function `%s`\n", str_lock_unlock);

  if (lock_destory(lock) != 0) {
    sirius_error("%s\n", str_lock_destory);
    return false;
  }
  sirius_infosp("Successfully executed function `%s`\n", str_lock_destory);

  sirius_infosp("--- Basic function test ended ---\n\n");

  return true;
}

/**
 * @brief Multithreaded competitive testing.
 */

#define NB_THREADS 8
#define ITERATIONS 100000

static lock_t g_counter_lock;
static volatile long g_counter = 0;

static void *thread_func(void *arg) {
  int thread_index = *(int *)arg;

  for (int i = 0; i < ITERATIONS; ++i) {
    lock_lock(g_counter_lock);

    g_counter++;
    volatile int temp = g_counter;
    (void)temp;

    lock_unlock(g_counter_lock);

    if (i % 1000 == 0)
      sirius_cpu_relax();
  }

  sirius_infosp("The thread ended. Index: %d; TID: %" PRIu64 "\n", thread_index,
                sirius_thread_id);
  return nullptr;
}

static inline bool test_multi_thread_contention() {
  sirius_infosp("--- Multithreaded competitive test begins ---\n");
  sirius_infosp(
    "Start %d threads, and each thread performs %d auto-increment operations\n",
    NB_THREADS, ITERATIONS);

  int ret = 0;
  sirius_thread_t threads[NB_THREADS];
  int thread_indexs[NB_THREADS];

  if (lock_init(g_counter_lock) != 0) {
    sirius_error("%s\n", str_lock_init);
    return false;
  }

  g_counter = 0;

  uint64_t start = sirius_get_time_us();
  for (int i = 0; i < NB_THREADS; ++i) {
    thread_indexs[i] = i;
    ret = sirius_thread_create(threads + i, nullptr, thread_func,
                               &thread_indexs[i]);
    if (ret) {
      sirius_error("sirius_thread_create: %d\n", ret);
      return false;
    }
  }

  for (int i = 0; i < NB_THREADS; ++i) {
    sirius_thread_join(threads[i], nullptr);
  }
  uint64_t end = sirius_get_time_us();

  // Verify the result
  long expected = NB_THREADS * ITERATIONS;
  if (g_counter == expected) {
    sirius_infosp("Test passed. Counter value: %ld (expected value: %ld)\n",
                  g_counter, expected);
  } else {
    sirius_error("Test failed. Counter value: %ld (expected value: %ld)\n",
                 g_counter, expected);
  }

  double elapsed = (double)(end - start) / 1000 / 1000;
  sirius_infosp("Total time consumption: %.3f s\n", elapsed);
  sirius_infosp("Operation " log_purple "%.0f" log_color_none
                " times per second\n",
                expected / elapsed);

  lock_destory(g_counter_lock);

  sirius_infosp("--- Multithreaded competitive test ended ---\n\n");

  return g_counter == expected;
}

/**
 * @brief Reentrant testing.
 *
 * @note Reentrant tests can get stuck, which are commented.
 */

static inline bool test_reentrancy() {
  sirius_infosp("--- Reentrant test begins ---\n");

#if LOCK_TYPE == 1

  lock_t lock;

#  if LOCK_TYPE == 0
  if (sirius_spin_init(&lock, 0) != 0) {
    sirius_error("sirius_spin_init\n");
    return false;
  }
#  else
  sirius_mutex_type_t type = sirius_mutex_recursive;
  if (sirius_mutex_init(&lock, &type) != 0) {
    sirius_error("sirius_mutex_init\n");
    return false;
  }
#  endif

  sirius_infosp("The first lock...\n");
  lock_lock(lock);
  sirius_infosp("Successfully lock for the first time\n");

  sirius_infosp("Try the second lock (on the same thread)...\n");
  sirius_warnsp(
    "If it is a non-reentrant lock, the program may get stuck here\n");

#  if LOCK_TYPE == 0 && (defined(__unix__) || defined(__linux__))
  alarm(2);
#  endif

  lock_lock(lock);

  sirius_logsp_impl(sirius_log_level_warn, log_level_str_info, log_purple,
                    sirius_log_module_name,
                    "Successfully lock for the second time\n");
  sirius_logsp_impl(sirius_log_level_warn, log_level_str_info, log_purple,
                    sirius_log_module_name,
                    "This should be a reentrable lock\n");

  lock_unlock(lock);
  lock_unlock(lock);

  lock_destory(lock);

#  if LOCK_TYPE == 0 && (defined(__unix__) || defined(__linux__))
  alarm(0);
#  endif

#else
  sirius_infosp("skipped\n");

#endif

  sirius_infosp("--- Reentrant test ended ---\n\n");

  return true;
}

/**
 * @brief Fairness testing.
 */

static bool g_fairness_exit_flag = false;

static void *thread_func_fairness(void *arg) {
  int thread_index = *(int *)arg;
  int count = 0;

  /**
   * @note Limited cycles prevent accidents.
   */
  for (int i = 0; i < ITERATIONS * 6400; ++i) {
    lock_lock(g_counter_lock);
    if (unlikely(g_fairness_exit_flag)) {
      lock_unlock(g_counter_lock);
      break;
    } else {
      count++;
      lock_unlock(g_counter_lock);
    }

    if (i % 100 == 0) {
      sirius_cpu_relax();
    }
  }

  sirius_infosp("The thread executed " log_purple "%d" log_color_none
                " times. Thread index: %d; TID: %" PRIu64 "\n",
                count, thread_index, sirius_thread_id);
  return nullptr;
}

static inline bool test_fairness() {
  sirius_infosp("--- Fairness test begins ---\n");
  sirius_infosp("Run it for a relatively long time\n");
  sirius_infosp(
    "Observe whether the execution times of each thread are balanced\n");

  int ret = 0;
  sirius_thread_t threads[NB_THREADS];
  int thread_indexs[NB_THREADS];

  lock_init(g_counter_lock);

  for (int i = 0; i < NB_THREADS; ++i) {
    thread_indexs[i] = i;
    ret = sirius_thread_create(threads + i, nullptr, thread_func_fairness,
                               &thread_indexs[i]);
    if (ret) {
      sirius_error("sirius_thread_create: %d\n", ret);
      return false;
    }
  }

  const int gap = 3, total = 12;
  for (int i = 0; i < total; i += gap) {
    int remain = total - i;
    const char *sp = remain >= 10 ? " " : "  ";
    sirius_infosp("%d%sseconds remain...\n", remain, sp);
    sirius_usleep(gap * 1000 * 1000);
  }

  lock_lock(g_counter_lock);
  g_fairness_exit_flag = true;
  lock_unlock(g_counter_lock);

  for (int i = 0; i < NB_THREADS; ++i) {
    sirius_thread_join(threads[i], nullptr);
  }

  lock_destory(g_counter_lock);

  sirius_infosp("--- Fairness test ended ---\n\n");

  return true;
}

int main() {
  bool ret = true;

  utils_init();

  ret &= test_basic_functionality();
  ret &= test_multi_thread_contention();
  ret &= test_reentrancy();
  ret &= test_fairness();

  utils_deinit();

  return (int)!ret;
}
