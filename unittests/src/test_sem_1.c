#include "sirius/sirius_sem.h"
#include "sirius/sirius_thread.h"
#include "test.h"

static bool g_exit = false;
static char g_str[128];
static sirius_sem_handle g_sem_sub;
static sirius_sem_handle g_sem_main;

void thread_func() {
  while (!g_exit) {
    t_assert(!sirius_sem_wait(&g_sem_sub));
    if (unlikely(g_exit))
      break;

    t_dprintf(1, "%s", g_str);

    t_assert(!sirius_sem_post(&g_sem_main));
  }
}

void *thread_func_wrapper(void *args) {
  if (args)
    ;

  thread_func();
  return NULL;
}

int main() {
  t_assert(!sirius_sem_init(&g_sem_main, 0, 0));
  t_assert(!sirius_sem_init(&g_sem_sub, 0, 0));

  sirius_thread_handle thread;
  t_assert(!sirius_thread_create(&thread, NULL, thread_func_wrapper, NULL));

  for (int i = 0; i < 2048; i++) {
    memset(g_str, 0, sizeof(g_str));
    snprintf(g_str, sizeof(g_str), "[thread id: %" PRIu64 "] [index: %d]\n",
             sirius_thread_id, i);

    t_assert(!sirius_sem_post(&g_sem_sub));
    t_assert(!sirius_sem_wait(&g_sem_main));
  }

  g_exit = true;
  t_assert(!sirius_sem_post(&g_sem_sub));

  t_assert(!sirius_thread_join(thread, NULL));
  t_assert(!sirius_sem_destroy(&g_sem_sub));
  t_assert(!sirius_sem_destroy(&g_sem_main));

  return 0;
}
