#include "sirius/sirius_thread.h"
#include "test.h"

#if 0
#  define test_root
#endif

static int idx = 0;
#ifndef _WIN32
static char g_retval[64][256] = {0};
#endif

void thread_func() {
  for (int i = 0; i < 200000; i++) {
  }

  char r[64] = {0};
  t_dprintf(1, "[%s] sub thread id: %" PRIu64 "\n", __func__, sirius_thread_id);
#ifndef _WIN32
  t_dprintf(1, "[%s] sub thread self-id: %lu\n", __func__,
            sirius_thread_self());
#endif
  t_dprintf(1, "%s: idx=%d\n", __func__, ++idx);
  snprintf(r, sizeof(r), "%s-%d", __func__, idx);
#ifndef _WIN32
  strncpy(g_retval[idx], r, sizeof(g_retval[idx]) - 1);
  sirius_thread_exit((void *)g_retval[idx]);
#else
  DWORD e_code = 0;
  sirius_thread_exit(&e_code);
#endif
}

void *thread_func_wrapper(void *args) {
  if (args)
    ;

  thread_func();
  return NULL;
}

int main() {
#define SLEEP \
  for (int i = 0; i < 2000000; i++) { \
  }

  int priority_min, priority_max;
  t_dprintf(1, "============ group1 ============\n");
  priority_min = sirius_thread_sched_get_priority_min(sirius_thread_self());
  priority_max = sirius_thread_sched_get_priority_max(sirius_thread_self());
  t_dprintf(1, "current thread priority: [min: %d][max: %d]\n", priority_min,
            priority_max);
  SLEEP
  t_dprintf(1, "============ group1 ============\n\n");

  t_dprintf(1, "============ group2 ============\n");
  sirius_thread_handle thread_handle;
  t_assert(
    !sirius_thread_create(&thread_handle, NULL, thread_func_wrapper, NULL));
  t_dprintf(1, "[%s] main thread id: %" PRIu64 "\n", __func__,
            sirius_thread_id);
#ifndef _WIN32
  t_dprintf(1, "[%s] sub thread self-id: %lu\n", __func__, thread_handle);
#endif
  void *retval = NULL;
  t_assert(!sirius_thread_join(thread_handle, &retval));
#ifndef _WIN32
  t_dprintf(1, "[%s] retval: %s\n", __func__, (char *)retval);
#endif
  SLEEP
  t_dprintf(1, "============ group2 ============\n\n");

  return 0;
#undef SLEEP
}
