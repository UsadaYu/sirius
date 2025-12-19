#include <sirius/sirius_thread.h>

#include "internal/utils.h"

void *foo(void *args) {
  (void)args;

  sirius_infosp("[Subt] Thread system id: %" PRIu64 "\n", sirius_thread_id);
#if !defined(_WIN32) && !defined(_WIN64)
  sirius_infosp("[Subt] Thread self id: %" PRId64 "\n",
                (int64_t)sirius_thread_self());
#endif

  return nullptr;
}

int main() {
  sirius_thread_t thread;
  sirius_thread_attr_t attr = {0};

  utils_init();

  attr.detach_state = sirius_thread_joinable;
  attr.inherit_sched = sirius_thread_inherit_sched;

  int ret = sirius_thread_create(&thread, &attr, foo, nullptr);
  if (ret) {
    sirius_error("sirius_thread_create: %d\n", ret);
    return -1;
  }

  sirius_infosp("[Main] Thread system id: %" PRIu64 "\n", sirius_thread_id);
#if !defined(_WIN32) && !defined(_WIN64)
  sirius_infosp("[Main] Thread self id: %" PRId64 "\n",
                (int64_t)sirius_thread_self());
#endif

  sirius_thread_join(thread, nullptr);

  utils_deinit();

  return 0;
}
