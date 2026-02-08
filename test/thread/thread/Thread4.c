/**
 * @note This test case will try to join the detached thread.
 * This is unsafe. Tools like asan may report `abort`.
 */

#include <sirius/thread/spinlock.h>
#include <sirius/thread/thread.h>

#include "internal/utils.h"

#define NB_GROUPS 4
#define INDEX_INIT (-1)

static int g_index = INDEX_INIT;
static sirius_spinlock_t g_spin;

static void *foo(void *arg) {
  const char *string = (const char *)arg;

  sirius_infosp("[%s] Thread id: %" PRIu64 "\n", string, SIRIUS_THREAD_ID);

  sirius_spin_lock(&g_spin);
  g_index++;
  sirius_spin_unlock(&g_spin);

  return nullptr;
}

int main() {
  utils_init();

  int ret;
  sirius_thread_attr_t attr;
  int index = INDEX_INIT;
  sirius_thread_t threads[NB_GROUPS];
  const char *args[NB_GROUPS];

  sirius_spin_init(&g_spin, kSiriusThreadProcessPrivate);

  // --- 01 ---
  sirius_infosp("[Main-thread 01] Thread id: %" PRIu64 "\n", SIRIUS_THREAD_ID);
  index++;
  memset(&attr, 0, sizeof(sirius_thread_attr_t));

  attr.detach_state = kSiriusThreadCreateDetached;

  args[index] = "Sub-thread  01";

  ret = sirius_thread_create(threads + index, &attr, foo, (void *)args[index]);
  if (ret) {
    sirius_error("sirius_thread_create: %d\n", ret);
    return -1;
  }

  sirius_warnsp("--------------------------------\n");
  sirius_warnsp("- Try to detached the already detached thread\n");
  sirius_warnsp("- sirius_thread_detach: %d\n",
                sirius_thread_detach(threads[index]));
  sirius_warnsp("- sirius_thread_detach: %d\n",
                sirius_thread_detach(threads[index]));
  sirius_warnsp("--------------------------------\n");

  utils_dprintf(1, "\n");
  // --- 02 ---
  sirius_infosp("[Main-thread 02] Thread id: %" PRIu64 "\n", SIRIUS_THREAD_ID);
  index++;
  memset(&attr, 0, sizeof(sirius_thread_attr_t));

  attr.detach_state = kSiriusThreadCreateDetached;

  args[index] = "Sub-thread  02";

  ret = sirius_thread_create(threads + index, &attr, foo, (void *)args[index]);
  if (ret) {
    sirius_error("sirius_thread_create: %d\n", ret);
    return -1;
  }

  sirius_warnsp("--------------------------------\n");
  sirius_warnsp("- Try to join the thread detached during initialization\n");
  sirius_logsp_impl(
    SIRIUS_LOG_LEVEL_WARN, LOG_LEVEL_STR_WARN, LOG_RED, _SIRIUS_LOG_PRINT_NAME,
    LOG_PURPLE
    "- This is unsafe. Tools like asan may abort here\n" LOG_COLOR_NONE);
  sirius_warnsp("- sirius_thread_join: %d\n",
                sirius_thread_join(threads[index], nullptr));
  sirius_warnsp("- sirius_thread_join: %d\n",
                sirius_thread_join(threads[index], nullptr));
  sirius_warnsp("--------------------------------\n");

  utils_dprintf(1, "\n");
  // --- 03 ---
  utils_dprintf(1, "\n");
  sirius_infosp("[Main-thread 03] Thread id: %" PRIu64 "\n", SIRIUS_THREAD_ID);
  index++;
  memset(&attr, 0, sizeof(sirius_thread_attr_t));

  attr.detach_state = kSiriusThreadCreateJoinable;

  args[index] = "Sub-thread  03";

  ret = sirius_thread_create(threads + index, &attr, foo, (void *)args[index]);
  if (ret) {
    sirius_error("sirius_thread_create: %d\n", ret);
    return -1;
  }

  sirius_thread_detach(threads[index]);

  sirius_warnsp("--------------------------------\n");
  sirius_warnsp("- Try to join the thread manually detached\n");
  sirius_logsp_impl(
    SIRIUS_LOG_LEVEL_WARN, LOG_LEVEL_STR_WARN, LOG_RED, _SIRIUS_LOG_PRINT_NAME,
    LOG_PURPLE
    "- This is unsafe. Tools like asan may abort here\n" LOG_COLOR_NONE);
  sirius_warnsp("- sirius_thread_join: %d\n",
                sirius_thread_join(threads[index], nullptr));
  sirius_warnsp("- sirius_thread_join: %d\n",
                sirius_thread_join(threads[index], nullptr));
  sirius_warnsp("--------------------------------\n");

  // ---
  while (true) {
    sirius_spin_lock(&g_spin);
    if (g_index < index) {
      sirius_spin_unlock(&g_spin);
      continue;
    }
    sirius_spin_unlock(&g_spin);
    break;
  }

  sirius_spin_destroy(&g_spin);

  utils_deinit();

  return 0;
}
