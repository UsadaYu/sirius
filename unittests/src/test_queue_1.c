#include "sirius/sirius_errno.h"
#include "sirius/sirius_mutex.h"
#include "sirius/sirius_queue.h"
#include "sirius/sirius_thread.h"
#include "test.h"

#define THREAD_CNT (32)
#define QUE_SIZE (128)

static sirius_que_handle g_que_result;
static sirius_que_handle g_que_free;
static sirius_thread_handle g_thd[THREAD_CNT];
static bool g_thd_exit_arr[THREAD_CNT] = {0};
static bool g_thd_exit = false;
static sirius_mutex_handle g_mutex;
static char g_que_ctx[QUE_SIZE][256] = {0};

void thread_exit() {
  int idx = 0;

  sirius_mutex_lock(&g_mutex);

  while (g_thd_exit_arr[idx++]);
  g_thd_exit_arr[idx - 1] = true;

  if (unlikely(idx == THREAD_CNT)) {
    /**
     * Wait for the consumer thread to finish fetching the
     * result in the queue.
     */
    sirius_usleep(500 * 1000);

    g_thd_exit = true;
    size_t num = 0;
    t_assert(!sirius_que_cache_num(g_que_result, &num));
    if (0 == num) {
      char *q_str;
      t_assert(!sirius_que_get(g_que_free,
                               (size_t *)&q_str, 0));
      t_assert(
          !sirius_que_put(g_que_result, (size_t)q_str, 0));
    }
  }

  sirius_mutex_unlock(&g_mutex);
}

void thread_func() {
  char str[256];

  for (int i = 0; i < 2048; i++) {
    memset(str, 0, sizeof(str));

    snprintf(str, sizeof(str),
             "[index: %d] [thread id: %llu]\n", i,
             sirius_thread_id);

    char *q_str = NULL;
    t_assert(!sirius_que_get(g_que_free, (size_t *)&q_str,
                             sirius_timeout_infinite) ||
             q_str);

    memcpy(q_str, str, strlen(str));

    t_assert(!sirius_que_put(g_que_result, (size_t)q_str,
                             sirius_timeout_infinite));
  }

  thread_exit();
}

void *thread_func_wrapper(void *args) {
  if (args)
    ;

  thread_func();
  return NULL;
}

int main() {
  sirius_que_t cr = {0};
  cr.elem_nr = QUE_SIZE;
  cr.que_type = sirius_que_type_mtx;
  t_assert(!sirius_que_alloc(&cr, &g_que_result));
  t_assert(!sirius_que_alloc(&cr, &g_que_free));
  t_assert(!sirius_mutex_init(&g_mutex, NULL));

  for (int i = 0; i < QUE_SIZE; i++) {
    t_assert(!sirius_que_put(g_que_free,
                             (size_t)g_que_ctx[i],
                             sirius_timeout_none));
  }

  for (int i = 0; i < THREAD_CNT; i++) {
    t_assert(!sirius_thread_create(
        &g_thd[i], NULL, thread_func_wrapper, NULL));
  }

  char *str = NULL;
  while (!g_thd_exit) {
    t_assert(!sirius_que_get(g_que_result, (size_t *)&str,
                             sirius_timeout_infinite));
    if (unlikely(g_thd_exit)) {
      break;
    }

    t_dprintf(1, "%s", str);
    memset(str, 0, strlen(str));

    t_assert(!sirius_que_put(g_que_free, (size_t)str,
                             sirius_timeout_infinite));
  }

  for (int i = 0; i < THREAD_CNT; i++) {
    t_assert(!sirius_thread_join(g_thd[i], NULL));
  }

  t_assert(!sirius_mutex_destroy(&g_mutex));
  t_assert(!sirius_que_free(g_que_free));
  t_assert(!sirius_que_free(g_que_result));

  return 0;
}
