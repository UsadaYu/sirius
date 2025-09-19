#include "sirius/sirius_thread.h"
#include "test.h"

#define N (32)
#if 0
#  define test_root
#endif

static int g_idx = 0;
static int g_argc = 1;
static int g_args_arr[N * 64] = {0};

void thread_func(void *args) {
  sirius_usleep(1000);
  t_dprintf(1, "%s: args=%d, idx=%d\n", __func__, *((int *)args), ++g_idx);
}

void *thread_func_wrapper(void *args) {
  thread_func(args);
  return NULL;
}

int main() {
#define THD_C(cnt) \
  do { \
    int idx = cnt > N ? N : cnt; \
    for (int i = 0; i < idx; i++) { \
      g_args_arr[g_argc] = g_argc; \
      t_assert(!sirius_thread_create(&thread_handle[i], &attr, \
                                     thread_func_wrapper, \
                                     (void *)(g_args_arr + g_argc))); \
      g_argc++; \
    } \
  } while (0)

#define THD_J(cnt) \
  do { \
    int idx = cnt > N ? N : cnt; \
    for (int i = 0; i < idx; i++) { \
      t_assert(!sirius_thread_join(thread_handle[i], NULL)); \
    } \
  } while (0)

  sirius_thread_handle thread_handle[N];
  sirius_thread_attr_t attr = {0};

  attr.detach_state = sirius_thread_detached;
  attr.inherit_sched = sirius_thread_explicit_sched;
#ifdef test_root
  attr.sched_param.sched_policy = sirius_thread_sched_rr;
#endif
  attr.scope = sirius_thread_scope_system;
  attr.sched_param.priority =
    sirius_thread_sched_get_priority_max(sirius_thread_self());
  attr.stacksize = 1024 * 200;
  t_dprintf(1, "============ group1 ============\n");
  THD_C(N);
  sirius_usleep(1000 * 500);
  t_dprintf(1, "============ group1 ============\n\n");

  attr.detach_state = sirius_thread_joinable;
#ifdef test_root
  attr.sched_param.sched_policy = sirius_thread_sched_fifo;
  attr.scope = sirius_thread_scope_process;
#endif
  attr.sched_param.priority =
    sirius_thread_sched_get_priority_min(sirius_thread_self());
  attr.guardsize = 128;
  t_dprintf(1, "============ group2 ============\n");
  THD_C(N);
  THD_J(N);
  sirius_usleep(1000 * 500);
  t_dprintf(1, "============ group2 ============\n\n");

  attr.detach_state = sirius_thread_detached;
  attr.inherit_sched = sirius_thread_inherit_sched;
  attr.stacksize = 1024 * 250;
  t_dprintf(1, "============ group3 ============\n");
  THD_C(N);
  sirius_usleep(1000 * 500);
  t_dprintf(1, "============ group3 ============\n\n");

  attr.detach_state = sirius_thread_joinable;
  char arr[1024 * 500];
  attr.stackaddr = (void *)arr;
  attr.stacksize = 1024 * 500;
  t_dprintf(1, "============ group4 ============\n");
  sirius_usleep(1000 * 500);
  THD_C(1);
  THD_J(1);
  sirius_usleep(1000 * 500);
  t_dprintf(1, "============ group4 ============\n\n");

  return 0;
#undef THD_J
#undef THD_C
}
