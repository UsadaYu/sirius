#include "sirius/sirius_mutex.h"
#include "sirius/sirius_queue.h"
#include "sirius/sirius_thread.h"
#include "test.h"

#define THREAD_CNT (32)
#define QUE_SIZE (128)

static sirius_queue_t g_que_result;
static sirius_queue_t g_que_free;
static sirius_thread_t g_thd[THREAD_CNT];
static bool g_thd_exit_arr[THREAD_CNT] = {0};
static bool g_thd_exit = false;
static sirius_mutex_t g_mutex;
static char g_que_ctx[QUE_SIZE][256] = {0};

void thread_exit() {
  int idx = 0;

  sirius_mutex_lock(&g_mutex);

  while (g_thd_exit_arr[idx++])
    ;
  g_thd_exit_arr[idx - 1] = true;

  if (unlikely(idx == THREAD_CNT)) {
    /**
     * @brief Wait for the consumer thread to finish fetching the result in the
     * queue.
     */
    sirius_usleep(500 * 1000);

    g_thd_exit = true;
    size_t num = 0;
    t_assert(!sirius_queue_cache_num(g_que_result, &num));
    if (0 == num) {
      char *q_str;
      t_assert(!sirius_queue_get(g_que_free, (size_t *)&q_str, 0));
      t_assert(!sirius_queue_put(g_que_result, (size_t)q_str, 0));
    }
  }

  sirius_mutex_unlock(&g_mutex);
}

void thread_func() {
  char str[256];

  for (int i = 0; i < 2048; i++) {
    memset(str, 0, sizeof(str));

    snprintf(str, sizeof(str), "[index: %d] [thread id: %" PRIu64 "]\n", i,
             sirius_thread_id);

    char *q_str = nullptr;
    t_assert(!sirius_queue_get(g_que_free, (size_t *)&q_str,
                               sirius_timeout_infinite) ||
             q_str);

    memcpy(q_str, str, strlen(str));

    t_assert(
      !sirius_queue_put(g_que_result, (size_t)q_str, sirius_timeout_infinite));
  }

  thread_exit();
}

void *thread_func_wrapper(void *args) {
  if (args)
    ;

  thread_func();
  return nullptr;
}

int main() {
  sirius_queue_args_t qargs = {0};
  qargs.elem_count = QUE_SIZE;
  qargs.que_type = sirius_queue_type_mtx;
  t_assert(!sirius_queue_alloc(&g_que_result, &qargs));
  t_assert(!sirius_queue_alloc(&g_que_free, &qargs));
  t_assert(!sirius_mutex_init(&g_mutex, nullptr));

  for (int i = 0; i < QUE_SIZE; i++) {
    t_assert(
      !sirius_queue_put(g_que_free, (size_t)g_que_ctx[i], sirius_timeout_none));
  }

  for (int i = 0; i < THREAD_CNT; i++) {
    t_assert(
      !sirius_thread_create(&g_thd[i], nullptr, thread_func_wrapper, nullptr));
  }

  char *str = nullptr;
  while (!g_thd_exit) {
    t_assert(
      !sirius_queue_get(g_que_result, (size_t *)&str, sirius_timeout_infinite));
    if (unlikely(g_thd_exit)) {
      break;
    }

    t_dprintf(1, "%s", str);
    memset(str, 0, strlen(str));

    t_assert(
      !sirius_queue_put(g_que_free, (size_t)str, sirius_timeout_infinite));
  }

  for (int i = 0; i < THREAD_CNT; i++) {
    t_assert(!sirius_thread_join(g_thd[i], nullptr));
  }

  t_assert(!sirius_mutex_destroy(&g_mutex));
  t_assert(!sirius_queue_free(g_que_free));
  t_assert(!sirius_queue_free(g_que_result));

  return 0;
}
