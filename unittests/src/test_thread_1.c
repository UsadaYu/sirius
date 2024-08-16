#include "sirius_thread.h"
#include "test.h"

#define N (32)
#if 0
#define test_root
#endif

static int g_idx = 0;
static int g_argc = 1;
static int g_args_arr[N * 64] = {0};

void thread_func(void *args) {
  for (int i = 0; i < 200000; i++) {
  }
  t_dprintf(1, "%s: args=%d, idx=%d\n", __FUNCTION__,
          *((int *)args), ++g_idx);
}

void *thread_func_wrapper(void *args) {
  thread_func(args);
  return NULL;
}

int main() {
#define THD_C(cnt)                                       \
  do {                                                   \
    int idx = cnt > N ? N : cnt;                         \
    for (int i = 0; i < idx; i++) {                      \
      g_args_arr[g_argc] = g_argc;                       \
      t_assert(!sirius_thread_create(                    \
          &thread_handle[i], &attr, thread_func_wrapper, \
          (void *)(g_args_arr + g_argc)));               \
      g_argc++;                                          \
    }                                                    \
  } while (0)

#define THD_J(cnt)                                      \
  do {                                                  \
    int idx = cnt > N ? N : cnt;                        \
    for (int i = 0; i < idx; i++) {                     \
      t_assert(                                         \
          !sirius_thread_join(thread_handle[i], NULL)); \
    }                                                   \
  } while (0)

#define SLEEP                         \
  for (int i = 0; i < 2000000; i++) { \
  }

  sirius_thread_handle thread_handle[N];
  sirius_thread_attr_t attr = {0};

#ifndef _WIN32
  attr.detach_state = sirius_thread_detached;
  attr.inherit_sched = SIRIUS_THREAD_EXPLICIT_SCHED;
#ifdef test_root
  attr.sched_param.sched_policy = SIRIUS_THREAD_SCHED_RR;
#endif
  attr.scope = SIRIUS_THREAD_SCOPE_SYSTEM;
  attr.sched_param.priority =
      sirius_thread_sched_get_priority_max(
          sirius_thread_self());
  attr.stacksize = 1024 * 200;
  t_dprintf(1, "============ group1 ============\n");
  THD_C(N);
  SLEEP
  t_dprintf(1, "============ group1 ============\n\n");

  attr.detach_state = sirius_thread_joinable;
#ifdef test_root
  attr.sched_param.sched_policy = SIRIUS_THREAD_SCHED_FIFO;
  attr.scope = SIRIUS_THREAD_SCOPE_PROCESS;
#endif
  attr.sched_param.priority =
      sirius_thread_sched_get_priority_min(
          sirius_thread_self());
  attr.guardsize = 128;
  t_dprintf(1, "============ group2 ============\n");
  THD_C(N);
  THD_J(N);
  SLEEP
  t_dprintf(1, "============ group2 ============\n\n");

  attr.detach_state = sirius_thread_detached;
  attr.inherit_sched = SIRIUS_THREAD_INHERIT_SCHED;
  attr.stacksize = 1024 * 250;
  t_dprintf(1, "============ group3 ============\n");
  THD_C(N);
  SLEEP
  t_dprintf(1, "============ group3 ============\n\n");

  attr.detach_state = sirius_thread_joinable;
  char arr[1024 * 500];
  attr.stackaddr = (void *)arr;
  attr.stacksize = 1024 * 500;
  t_dprintf(1, "============ group4 ============\n");
  SLEEP SLEEP SLEEP THD_C(1);
  THD_J(1);
  SLEEP
  t_dprintf(1, "============ group4 ============\n\n");

#else
  attr.sched_param.priority =
      sirius_thread_sched_get_priority_max(
          sirius_thread_self());
  attr.stacksize = 1024 * 200;
  t_dprintf(1, "============ group1 ============\n");
  THD_C(N);
  THD_J(N);
  SLEEP
  t_dprintf(1, "============ group1 ============\n\n");

  attr.sched_param.priority =
      sirius_thread_sched_get_priority_min(
          sirius_thread_self());
  attr.stacksize = 1024 * 300;
  t_dprintf(1, "============ group2 ============\n");
  THD_C(N);
  THD_J(N);
  SLEEP
  t_dprintf(1, "============ group2 ============\n\n");

#endif

  return 0;
#undef SLEEP
#undef THD_J
#undef THD_C
}
