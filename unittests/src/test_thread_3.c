#include "sirius/sirius_thread.h"
#include "sirius/sirius_time.h"
#include "test.h"

#if 0
#define test_root
#endif

static int g_cnt = 0;
static bool g_exit = false;

void thread_func() {
  while (!g_exit) {
    sirius_usleep(1000 * 100);
    t_dprintf(1, "[%s] cnt: %d\n", __FUNCTION__, ++g_cnt);
  }

  sirius_thread_exit(NULL);
}

void *thread_func_wrapper(void *args) {
  if (args)
    ;

  thread_func();
  return NULL;
}

int main() {
#define SLEEP                         \
  for (int i = 0; i < 2000000; i++) { \
  }

  sirius_thread_handle thread_handle;
  sirius_thread_sched_param_t param;

  t_dprintf(1, "============ group1 ============\n");
  t_assert(!sirius_thread_create(
      &thread_handle, NULL, thread_func_wrapper, NULL));
  SLEEP

#ifndef _WIN32
#define GPP                                             \
  do {                                                  \
    t_dprintf(1, "[%s] priority: %d\n", __FUNCTION__,     \
            param.priority);                            \
    t_dprintf(1, "[%s] sched_policy: %d\n", __FUNCTION__, \
            param.sched_policy);                        \
  } while (0)
#else
#define GPP                                         \
  do {                                              \
    t_dprintf(1, "[%s] priority: %d\n", __FUNCTION__, \
            param.priority);                        \
  } while (0)
#endif

#define GP                                               \
  do {                                                   \
    memset(&param, 0,                                    \
           sizeof(sirius_thread_sched_param_t));         \
    t_assert(!sirius_thread_getschedparam(thread_handle, \
                                          &param));      \
    GPP;                                                 \
  } while (0)

  GP;

  memset(&param, 0, sizeof(sirius_thread_sched_param_t));
#ifndef _WIN32
#ifdef test_root
  param.sched_policy = SIRIUS_THREAD_SCHED_FIFO;
  param.priority = 25;
#else
  param.sched_policy = SIRIUS_THREAD_SCHED_OTHER;
  param.priority = 0;
#endif

#else
  param.priority =
      sirius_thread_sched_get_priority_max(thread_handle) -
      2;
#endif
  t_assert(
      !sirius_thread_setschedparam(thread_handle, &param));
  GP;

  g_exit = true;
  t_assert(!sirius_thread_join(thread_handle, NULL));
#undef GP
#undef GPP
  t_dprintf(1, "============ group1 ============\n\n");

  g_exit = false;
  t_dprintf(1, "============ group2 ============\n");
  t_assert(!sirius_thread_create(
      &thread_handle, NULL, thread_func_wrapper, NULL));
  sirius_usleep(450 * 1000);
  t_assert(!sirius_thread_cancel(thread_handle));
  t_assert(!sirius_thread_join(thread_handle, NULL));
  t_dprintf(1, "============ group2 ============\n\n");

  return 0;
#undef SLEEP
}
