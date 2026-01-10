#include <sirius/sirius_thread.h>
#include <sirius/sirius_time.h>

#include "internal/utils.h"

/**
 * @note Setting the rotation mode and thread priority often requires certain
 * permissions.
 */
#define PERMISSION (0)

static std::atomic<bool> g_thread_flag = false;

static void *foo(void *arg) {
  (void)arg;

  while (!g_thread_flag.load()) {
    sirius_usleep(200 * 1000);
  }

  sirius_usleep(200 * 1000);

  return nullptr;
}

static void print_sched(sirius_thread_t thread) {
  int ret;
  std::string es;
  sirius_thread_sched_param_t sched_param {};

  ret = sirius_thread_getschedparam(thread, &sched_param);
  if (ret) {
    es =
      std::format(log_red
                  "\n"
                  "  Main-Detach\n"
                  "  `sirius_thread_getschedparam` error: {}\n" log_color_none,
                  ret);
    throw std::runtime_error(es);
  }
  sirius_infosp("--------------------------------\n");
  sirius_infosp(
    "- Main-Detach. Successfully get the sub thread scheduling policy\n");
  sirius_infosp("- Scheduling policy: %d; Priority: %d\n",
                sched_param.sched_policy, sched_param.priority);
  sirius_infosp("--------------------------------\n");
}

int main() {
  auto init = Utils::Init();

  int ret;
  std::string es;
  sirius_thread_attr_t attr {};
  sirius_thread_t thread;

  attr.detach_state = sirius_thread_detached;

  // POSIX
#if PERMISSION
  attr.sched_param.sched_policy = sirius_thread_sched_fifo;
#endif

#if defined(_WIN32) || defined(_WIN64) || PERMISSION
  attr.sched_param.priority = 2;
  sirius_infosp("Main-Detach. Thread initial priority: %d\n",
                attr.sched_param.priority);
#endif

  ret = sirius_thread_create(&thread, &attr, foo, nullptr);
  if (ret) {
    es = std::format(log_red
                     "\n"
                     "  Main-Detach\n"
                     "  `sirius_thread_create` error: {}\n" log_color_none,
                     ret);
    throw std::runtime_error(es);
  }

  print_sched(thread);

  int priority;
  sirius_thread_get_priority_max(thread, &priority);
  sirius_infosp("Main-Detach. Gets the max priority of the sub thread: %d\n",
                priority);
  sirius_thread_get_priority_min(thread, &priority);
  sirius_infosp("Main-Detach. Gets the min priority of the sub thread: %d\n",
                priority);

#if defined(_WIN32) || defined(_WIN64) || PERMISSION
  sirius_thread_sched_param_t sched_param {};
  sched_param.sched_policy = sirius_thread_sched_rr;
  sched_param.priority = 36;
  ret = sirius_thread_setschedparam(thread, &sched_param);
  if (ret) {
    es =
      std::format(log_red
                  "\n"
                  "  Main-Detach\n"
                  "  `sirius_thread_setschedparam` error: {}\n" log_color_none,
                  ret);
    throw std::runtime_error(es);
  } else {
    sirius_infosp("--------------------------------\n");
    sirius_infosp(
      "- Main-Detach. Successfully set the sub thread scheduling policy\n");
    sirius_infosp("- Scheduling policy: %d; Priority: %d\n",
                  sched_param.sched_policy, sched_param.priority);
    sirius_infosp("--------------------------------\n");
  }
#endif

  print_sched(thread);

  g_thread_flag.store(true);

  return 0;
}
