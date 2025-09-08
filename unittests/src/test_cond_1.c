#include "sirius/sirius_cond.h"
#include "sirius/sirius_thread.h"
#include "test.h"

#define BROAD_M 1
#define THREAD_CNT (16)

static unsigned int g_idx = 0;
sirius_cond_handle g_cond;
sirius_mutex_handle g_mutex;
bool g_prod_flag = true;
bool g_exit = false;

static void cons_thread(void *args) {
  for (int i = 0; i < 1024; i++) {
    sirius_mutex_lock(&g_mutex);

#if BROAD_M
    sirius_cond_wait(&g_cond, &g_mutex);
#else
    /**
     * The thread awakened by `broadcast` is unsafe to control the
     * `g_prod_flag` flag, so a non-infinite signal wait is used here.
     */
    sirius_cond_timedwait(&g_cond, &g_mutex, 1000);
#endif

    t_dprintf(1, "[thread id: %" PRIu64 "] [var: %u]\n", sirius_thread_id,
              g_idx);

    g_prod_flag = true;
    sirius_mutex_unlock(&g_mutex);
  }
}

static void *cons_thread_wrapper(void *args) {
  cons_thread(args);
  return NULL;
}

static void prod_thread(void *args) {
  while (!g_exit) {
    sirius_mutex_lock(&g_mutex);

    if (g_prod_flag) {
      g_idx++;
#if BROAD_M
      sirius_cond_signal(&g_cond);
#else
      sirius_cond_broadcast(&g_cond);
#endif
      g_prod_flag = false;
    }

    sirius_mutex_unlock(&g_mutex);
  }
}

static void *prod_thread_wrapper(void *args) {
  prod_thread(args);
  return NULL;
}

int main() {
  sirius_thread_handle cons_thread[THREAD_CNT];
  sirius_thread_handle prod_thread;

  sirius_cond_init(&g_cond, NULL);
  sirius_mutex_init(&g_mutex, NULL);

  for (int i = 0; i < THREAD_CNT; i++) {
    sirius_thread_create(&cons_thread[i], NULL, cons_thread_wrapper, NULL);
  }

  sirius_thread_create(&prod_thread, NULL, prod_thread_wrapper, NULL);

  for (int i = 0; i < THREAD_CNT; i++) {
    sirius_thread_join(cons_thread[i], NULL);
  }

  g_exit = true;
  sirius_thread_join(prod_thread, NULL);

  sirius_mutex_destroy(&g_mutex);
  sirius_cond_destroy(&g_cond);

  return 0;
}
