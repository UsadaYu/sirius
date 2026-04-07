/**
 * @note This test case will try to join the detached thread.
 * This is unsafe. Tools like asan may report `abort`.
 */

#include <sirius/thread/spinlock.h>
#include <sirius/thread/thread.h>

#include "inner/utils.h"

#define NB_GROUPS 4
#define INDEX_INIT (-1)

static int g_index = INDEX_INIT;
static ss_spinlock_t g_spin;

static void *foo(void *arg) {
  const char *string = (const char *)arg;

  ss_log_infosp("[%s] Thread id: %" PRIu64 "\n", string, SS_THREAD_ID);

  ss_spin_lock(&g_spin);
  ++g_index;
  ss_spin_unlock(&g_spin);

  return nullptr;
}

int main() {
  utils_init();

  int ret;
  ss_thread_attr_t attr;
  int index = INDEX_INIT;
  ss_thread_t threads[NB_GROUPS];
  const char *args[NB_GROUPS];

  ss_spin_init(&g_spin, kSsThreadProcessPrivate);

  // --- 01 ---
  ss_log_infosp("[Main-thread 01] Thread id: %" PRIu64 "\n", SS_THREAD_ID);
  ++index;
  memset(&attr, 0, sizeof(ss_thread_attr_t));

  attr.detach_state = kSsThreadCreateDetached;

  args[index] = "Sub-thread  01";

  ret = ss_thread_create(threads + index, &attr, foo, (void *)args[index]);
  if (ret) {
    ss_log_error("ss_thread_create: %d\n", ret);
    goto label_error;
  }

  ss_log_warnsp("--------------------------------\n");
  ss_log_warnsp("- Try to detached the already detached thread\n");
  ss_log_warnsp("- ss_thread_detach: %d\n", ss_thread_detach(threads[index]));
  ss_log_warnsp("- ss_thread_detach: %d\n", ss_thread_detach(threads[index]));
  ss_log_warnsp("--------------------------------\n");

  utils_dprintf(1, "\n");
  // --- 02 ---
  ss_log_infosp("[Main-thread 02] Thread id: %" PRIu64 "\n", SS_THREAD_ID);
  ++index;
  memset(&attr, 0, sizeof(ss_thread_attr_t));

  attr.detach_state = kSsThreadCreateDetached;

  args[index] = "Sub-thread  02";

  ret = ss_thread_create(threads + index, &attr, foo, (void *)args[index]);
  if (ret) {
    ss_log_error("ss_thread_create: %d\n", ret);
    goto label_error;
  }

  ss_log_warnsp("--------------------------------\n");
  ss_log_warnsp("- Try to join the thread detached during initialization\n");
  ss_logsp_impl(SS_LOG_LEVEL_ERROR, _SIRIUS_LOG_MODULE_NAME,
                "- This is unsafe. Tools like asan may abort here\n");
  ss_log_warnsp("- ss_thread_join: %d\n",
                ss_thread_join(threads[index], nullptr));
  ss_log_warnsp("- ss_thread_join: %d\n",
                ss_thread_join(threads[index], nullptr));
  ss_log_warnsp("--------------------------------\n");

  utils_dprintf(1, "\n");
  // --- 03 ---
  utils_dprintf(1, "\n");
  ss_log_infosp("[Main-thread 03] Thread id: %" PRIu64 "\n", SS_THREAD_ID);
  ++index;
  memset(&attr, 0, sizeof(ss_thread_attr_t));

  attr.detach_state = kSsThreadCreateJoinable;

  args[index] = "Sub-thread  03";

  ret = ss_thread_create(threads + index, &attr, foo, (void *)args[index]);
  if (ret) {
    ss_log_error("ss_thread_create: %d\n", ret);
    goto label_error;
  }

  ss_thread_detach(threads[index]);

  ss_log_warnsp("--------------------------------\n");
  ss_log_warnsp("- Try to join the thread manually detached\n");
  ss_logsp_impl(SS_LOG_LEVEL_ERROR, _SIRIUS_LOG_MODULE_NAME,
                "- This is unsafe. Tools like asan may abort here\n");
  ss_log_warnsp("- ss_thread_join: %d\n",
                ss_thread_join(threads[index], nullptr));
  ss_log_warnsp("- ss_thread_join: %d\n",
                ss_thread_join(threads[index], nullptr));
  ss_log_warnsp("--------------------------------\n");

  // ---
  while (true) {
    ss_spin_lock(&g_spin);
    if (g_index < index) {
      ss_spin_unlock(&g_spin);
      continue;
    }
    ss_spin_unlock(&g_spin);
    break;
  }

  ss_spin_destroy(&g_spin);

  utils_deinit();

  return 0;

label_error:
  utils_deinit();
  return -1;
}
