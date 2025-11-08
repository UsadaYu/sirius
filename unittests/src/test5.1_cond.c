#include "sirius/sirius_cond.h"
#include "sirius/sirius_thread.h"
#include "test.h"

#define BROAD_M 1
#define THREAD_CNT (16)

static unsigned int g_idx = 0;
static sirius_cond_t g_cond;
static sirius_mutex_t g_mutex;
static bool g_prod_flag = true;
static bool g_exit = false;

static void *foo_cons(void *args) {
  for (int i = 0; i < 1024; i++) {
    sirius_mutex_lock(&g_mutex);

#if BROAD_M
    sirius_cond_wait(&g_cond, &g_mutex);
#else
    /**
     * @brief The thread awakened by `broadcast` is unsafe to control the
     * `g_prod_flag` flag, so a non-infinite signal wait is used here.
     */
    sirius_cond_timedwait(&g_cond, &g_mutex, 1000);
#endif

    t_dprintf(1, "[thread id: %" PRIu64 "] [var: %u]\n", sirius_thread_id,
              g_idx);

    g_prod_flag = true;
    sirius_mutex_unlock(&g_mutex);
  }

  return nullptr;
}

static void *foo_prod(void *args) {
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

  return nullptr;
}

int main() {
  sirius_thread_t cons_thread[THREAD_CNT];
  sirius_thread_t prod_thread;

  sirius_cond_init(&g_cond, nullptr);
  sirius_mutex_init(&g_mutex, nullptr);

  for (int i = 0; i < THREAD_CNT; i++) {
    sirius_thread_create(&cons_thread[i], nullptr, foo_cons, nullptr);
  }

  sirius_thread_create(&prod_thread, nullptr, foo_prod, nullptr);

  for (int i = 0; i < THREAD_CNT; i++) {
    sirius_thread_join(cons_thread[i], nullptr);
  }

  g_exit = true;
  sirius_thread_join(prod_thread, nullptr);

  sirius_mutex_destroy(&g_mutex);
  sirius_cond_destroy(&g_cond);

  return 0;
}
