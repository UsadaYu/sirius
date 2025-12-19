#include <sirius/sirius_thread.h>
#include <sirius/sirius_time.h>

#include "internal/utils.h"

/**
 * @note Setting the rotation mode and thread priority often requires certain
 * permissions.
 */
#define PERMISSION (0)

void *foo(void *args) {
  sirius_usleep(100);

  const char *infc_str = (const char *)args;

  std::cout << "[Subt] " << infc_str << std::endl;

  sirius_usleep(100);
  return nullptr;
}

int main() {
  auto init = Utils::Init();

  sirius_thread_t thread;
  sirius_thread_attr_t attr {};

  attr.detach_state = sirius_thread_detached;
  attr.inherit_sched = sirius_thread_explicit_sched;
  attr.scope = sirius_thread_scope_system;
  attr.stacksize = 1024 * 200;
#if PERMISSION
  attr.sched_param.sched_policy = sirius_thread_sched_fifo;
  attr.sched_param.priority = 10;
#endif

  const char *args = "Hello Sub Thread";
  int ret = sirius_thread_create(&thread, &attr, foo, (void *)args);
  if (ret) {
    std::string es = std::format("sirius_thread_create: {}", ret);
    throw std::runtime_error(es);
  }

  int priority;
  sirius_thread_get_priority_max(thread, &priority);
  std::cout << "[Main] " << "Sub thread priority max: " << priority
            << std::endl;
  sirius_thread_get_priority_min(thread, &priority);
  std::cout << "[Main] " << "Sub thread priority min: " << priority
            << std::endl;

  sirius_thread_sched_param_t sched_param {};

#if PERMISSION
  sched_param.sched_policy = sirius_thread_sched_other;
  sched_param.priority = 0;
  if ((ret = sirius_thread_setschedparam(thread, &sched_param)) != 0) {
    std::string es = std::format("sirius_thread_setschedparam: {}", ret);
    std::cerr << es << std::endl;
  }
#endif

  if ((ret = sirius_thread_getschedparam(thread, &sched_param)) != 0) {
    std::string es = std::format("sirius_thread_getschedparam: {}", ret);
    std::cerr << es << std::endl;
  }
  std::cout << "[Main] "
            << "Sub thread sched policy: " << sched_param.sched_policy
            << std::endl;
  std::cout << "[Main] " << "Sub thread priority: " << sched_param.priority
            << std::endl;

  sirius_usleep(1000 * 10);
  std::cout << std::endl;
  return 0;
}
