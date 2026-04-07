#include <sirius/c/time.h>
#include <sirius/thread/thread.h>

#include "inner/utils.h"

/**
 * @brief LOCK_TYPE: 0 - spinlock; 1 - mutex
 */
#ifndef LOCK_TYPE
#  define LOCK_TYPE 0
#endif

#if LOCK_TYPE == 0

#  include <sirius/thread/spinlock.h>

#  define lock_t ss_spinlock_t
#  define str_lock_init "ss_spin_init"
#  define str_lock_lock "ss_spin_lock"
#  define str_lock_unlock "ss_spin_unlock"
#  define str_lock_destroy "ss_spin_destroy"
#  define lock_init(lock) ss_spin_init(&(lock), 0)
#  define lock_lock(lock) ss_spin_lock(&(lock))
#  define lock_unlock(lock) ss_spin_unlock(&(lock))
#  define lock_destroy(lock) ss_spin_destroy(&(lock))

#else

#  include <sirius/thread/mutex.h>

#  define lock_t ss_mutex_t
#  define str_lock_init "ss_mutex_init"
#  define str_lock_lock "ss_mutex_lock"
#  define str_lock_unlock "ss_mutex_unlock"
#  define str_lock_destroy "ss_mutex_destroy"
#  define lock_init(lock) ss_mutex_init(&(lock), nullptr)
#  define lock_lock(lock) ss_mutex_lock(&(lock))
#  define lock_unlock(lock) ss_mutex_unlock(&(lock))
#  define lock_destroy(lock) ss_mutex_destroy(&(lock))

#endif

/**
 * @brief Basic function testing.
 */

static inline bool test_basic_functionality() {
  ss_log_infosp("--- Basic function test begins ---\n");

  lock_t lock;

  if (lock_init(lock) != 0) {
    ss_log_error("%s\n", str_lock_init);
    return false;
  }
  ss_log_infosp("Successfully executed function `%s`\n", str_lock_init);

  if (lock_lock(lock) != 0) {
    ss_log_error("%s\n", str_lock_lock);
    return false;
  }
  ss_log_infosp("Successfully executed function `%s`\n", str_lock_lock);

  if (lock_unlock(lock) != 0) {
    ss_log_error("%s\n", str_lock_unlock);
    return false;
  }
  ss_log_infosp("Successfully executed function `%s`\n", str_lock_unlock);

  if (lock_destroy(lock) != 0) {
    ss_log_error("%s\n", str_lock_destroy);
    return false;
  }
  ss_log_infosp("Successfully executed function `%s`\n", str_lock_destroy);

  ss_log_infosp("--- Basic function test ended ---\n\n");

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

    ++g_counter;
    volatile int temp = g_counter;
    (void)temp;

    lock_unlock(g_counter_lock);

    if (i % 1000 == 0) {
      ss_os_yield();
    }
  }

  ss_log_infosp("The thread ended. Index: %d; TID: %" PRIu64 "\n", thread_index,
                SS_THREAD_ID);
  return nullptr;
}

static inline bool test_multi_thread_contention() {
  ss_log_infosp("--- Multithreaded competitive test begins ---\n");
  ss_log_infosp(
    "Start %d threads, "
    "and each thread performs %d auto-increment operations\n",
    NB_THREADS, ITERATIONS);

  int ret = 0;
  ss_thread_t threads[NB_THREADS];
  int thread_indexs[NB_THREADS];

  if (lock_init(g_counter_lock) != 0) {
    ss_log_error("%s\n", str_lock_init);
    return false;
  }

  g_counter = 0;

  uint64_t start = ss_get_clock_monotonic_us();
  for (int i = 0; i < NB_THREADS; ++i) {
    thread_indexs[i] = i;
    ret =
      ss_thread_create(threads + i, nullptr, thread_func, &thread_indexs[i]);
    if (ret) {
      ss_log_error("ss_thread_create: %d\n", ret);
      return false;
    }
  }

  for (int i = 0; i < NB_THREADS; ++i) {
    ss_thread_join(threads[i], nullptr);
  }
  uint64_t end = ss_get_clock_monotonic_us();

  // Verify the result
  long expected = NB_THREADS * ITERATIONS;
  if (g_counter == expected) {
    ss_log_infosp("Test passed. Counter value: %ld (expected value: %ld)\n",
                  g_counter, expected);
  } else {
    ss_log_error("Test failed. Counter value: %ld (expected value: %ld)\n",
                 g_counter, expected);
  }

  double elapsed = (double)(end - start) / 1000 / 1000;
  ss_log_infosp("Total time consumption: %.3f s\n", elapsed);
  ss_log_infosp(
    "Operation "
    "\033[0;35m"
    "%.0f"
    "\033[m"
    " times per second\n",
    expected / elapsed);

  lock_destroy(g_counter_lock);

  ss_log_infosp("--- Multithreaded competitive test ended ---\n\n");

  return g_counter == expected;
}

/**
 * @brief Reentrant testing.
 *
 * @note Reentrant tests can get stuck, which are commented.
 */

static inline bool test_reentrancy() {
  ss_log_infosp("--- Reentrant test begins ---\n");

#if LOCK_TYPE == 1

  lock_t lock;

#  if LOCK_TYPE == 0
  if (ss_spin_init(&lock, 0) != 0) {
    ss_log_error("ss_spin_init\n");
    return false;
  }
#  else
  enum SsMutexType type = kSsMutexTypeRecursive;
  if (ss_mutex_init(&lock, &type) != 0) {
    ss_log_error("ss_mutex_init\n");
    return false;
  }
#  endif

  ss_log_infosp("The first lock...\n");
  lock_lock(lock);
  ss_log_infosp("Successfully lock for the first time\n");

  ss_log_infosp("Try the second lock (on the same thread)...\n");
  ss_log_warnsp(
    "If it is a non-reentrant lock, the program may get stuck here\n");

#  if LOCK_TYPE == 0 && (defined(__unix__) || defined(__linux__))
  alarm(2);
#  endif

  lock_lock(lock);

  ss_logsp_impl(SS_LOG_LEVEL_WARN, _SIRIUS_LOG_MODULE_NAME,
                "Successfully lock for the second time\n");
  ss_logsp_impl(SS_LOG_LEVEL_WARN, _SIRIUS_LOG_MODULE_NAME,
                "This should be a reentrable lock\n");

  lock_unlock(lock);
  lock_unlock(lock);

  lock_destroy(lock);

#  if LOCK_TYPE == 0 && (defined(__unix__) || defined(__linux__))
  alarm(0);
#  endif

#else
  ss_log_infosp("skipped\n");

#endif

  ss_log_infosp("--- Reentrant test ended ---\n\n");

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
    if (ss_unlikely(g_fairness_exit_flag)) {
      lock_unlock(g_counter_lock);
      break;
    } else {
      ++count;
      lock_unlock(g_counter_lock);
    }

    if (i % 100 == 0) {
      ss_os_yield();
    }
  }

  ss_log_infosp(
    "The thread executed "
    "\033[0;35m"
    "%d"
    "\033[m"
    " times. "
    "Thread index: %d; TID: %" PRIu64 "\n",
    count, thread_index, SS_THREAD_ID);
  return nullptr;
}

static inline bool test_fairness() {
  ss_log_infosp("--- Fairness test begins ---\n");
  ss_log_infosp("Run it for a relatively long time\n");
  ss_log_infosp(
    "Observe whether the execution times of each thread are balanced\n");

  int ret = 0;
  ss_thread_t threads[NB_THREADS];
  int thread_indexs[NB_THREADS];

  lock_init(g_counter_lock);

  for (int i = 0; i < NB_THREADS; ++i) {
    thread_indexs[i] = i;
    ret = ss_thread_create(threads + i, nullptr, thread_func_fairness,
                           &thread_indexs[i]);
    if (ret) {
      ss_log_error("ss_thread_create: %d\n", ret);
      return false;
    }
  }

  const int kGap = 3, kTotal = 12;
  for (int i = 0; i < kTotal; i += kGap) {
    int remain = kTotal - i;
    const char *sp = remain >= 10 ? " " : "  ";
    ss_log_infosp("%d%sseconds remain...\n", remain, sp);
    ss_usleep(kGap * 1000 * 1000);
  }

  lock_lock(g_counter_lock);
  g_fairness_exit_flag = true;
  lock_unlock(g_counter_lock);

  for (int i = 0; i < NB_THREADS; ++i) {
    ss_thread_join(threads[i], nullptr);
  }

  lock_destroy(g_counter_lock);

  ss_log_infosp("--- Fairness test ended ---\n\n");

  return true;
}

int main() {
  utils_init();

  bool ret = true;

  ret &= test_basic_functionality();
  ret &= test_multi_thread_contention();
  ret &= test_reentrancy();
  ret &= test_fairness();

  utils_deinit();

  return (int)!ret;
}
