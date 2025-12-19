#include <sirius/sirius_thread.h>

#include "internal/utils.h"

void *foo(void *arg) {
  (void)arg;

  sirius_infosp("[Subt] Thread system id: %" PRIu64 "\n", sirius_thread_id);
#if !defined(_WIN32) && !defined(_WIN64)
  sirius_infosp("[Subt] Thread self id: %" PRId64 "\n",
                (int64_t)sirius_thread_self());
#endif

  return nullptr;
}

int main() {
  int ret;
  sirius_thread_t thread;

  utils_init();

  ret = sirius_thread_create(&thread, nullptr, foo, nullptr);
  if (ret) {
    sirius_error("sirius_thread_create: %d\n", ret);
    return -1;
  }

  sirius_thread_detach(thread);

  sirius_infosp("[Main] Thread system id: %" PRIu64 "\n", sirius_thread_id);
#if !defined(_WIN32) && !defined(_WIN64)
  sirius_infosp("[Main] Thread self id: %" PRId64 "\n",
                (int64_t)sirius_thread_self());
#endif

  sirius_thread_join(thread, nullptr);

  sirius_warnsp("--------------------------------\n");
  sirius_warnsp("- Try to join the detached thread\n");
  ret = sirius_thread_join(thread, nullptr);
  sirius_warnsp("- sirius_thread_join: %d\n", ret);
  sirius_warnsp("--------------------------------\n");

  utils_deinit();

  return 0;
}
