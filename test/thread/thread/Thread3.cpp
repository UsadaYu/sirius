#include <sirius/thread/thread.h>

#include <thread>

#include "inner/utils.h"

/**
 * @note Setting the rotation mode and thread priority often requires certain
 * permissions.
 */
#define PERMISSION (0)

namespace {
inline std::atomic<bool> g_thread_flag1 = false;
inline std::atomic<bool> g_thread_flag2 = false;

inline void *foo(void *arg) {
  (void)arg;

  while (!g_thread_flag1.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  g_thread_flag2.store(true);

  return nullptr;
}

inline void print_sched(ss_thread_t thread) {
  int ret;
  std::string es;
  ss_thread_sched_args_t sched_param {};

  ret = ss_thread_getschedparam(thread, &sched_param);
  if (ret) {
    es = std::format(
      "\n"
      "  Main-Detach\n"
      "  `ss_thread_getschedparam` error: {}",
      ret);
    throw std::runtime_error(std::move(es));
  }
  ss_log_infosp(
    "\n"
    "Main-Detach. Successfully get the sub thread scheduling policy\n"
    "Scheduling policy: %d; Priority: %d\n",
    sched_param.sched_policy, sched_param.priority);
}

inline int main_impl() {
  int ret;
  std::string es;
  ss_thread_attr_t attr {};
  ss_thread_t thread;

  attr.detach_state = kSsThreadCreateDetached;

  // POSIX
#if PERMISSION
  attr.sched_param.sched_policy = kSsThreadSchedFifo;
#endif

#if defined(_WIN32) || defined(_WIN64) || PERMISSION
  attr.sched_param.priority = 2;
  ss_log_infosp("Main-Detach. Thread initial priority: %d\n",
                attr.sched_param.priority);
#endif

  ret = ss_thread_create(&thread, &attr, foo, nullptr);
  if (ret) {
    es = std::format(
      "\n"
      "  Main-Detach\n"
      "  `ss_thread_create` error: {}",
      ret);
    throw std::runtime_error(std::move(es));
  }

  print_sched(thread);

  int priority;
  ss_thread_get_priority_max(thread, &priority);
  ss_log_infosp("Main-Detach. Gets the max priority of the sub thread: %d\n",
                priority);
  ss_thread_get_priority_min(thread, &priority);
  ss_log_infosp("Main-Detach. Gets the min priority of the sub thread: %d\n",
                priority);

#if defined(_WIN32) || defined(_WIN64) || PERMISSION
  ss_thread_sched_args_t sched_param {};
  sched_param.sched_policy = kSsThreadSchedRR;
  sched_param.priority = 36;
  ret = ss_thread_setschedparam(thread, &sched_param);
  if (ret) {
    es = std::format(
      "\n"
      "  Main-Detach\n"
      "  `ss_thread_setschedparam` error: {}",
      ret);
    throw std::runtime_error(std::move(es));
  } else {
    ss_log_infosp(
      "\n"
      "Main-Detach. Successfully set the sub thread scheduling policy\n"
      "Scheduling policy: %d; Priority: %d\n",
      sched_param.sched_policy, sched_param.priority);
  }
#endif

  print_sched(thread);

  g_thread_flag1.store(true);

  while (!g_thread_flag2.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  return 0;
}
} // namespace

int main() {
  auto init = utils::Init();

  try {
    return main_impl();
  } catch (const std::exception &e) {
    ss_log_error("%s\n", e.what());
    return -1;
  } catch (...) {
    ss_log_error("`exception`: unknow\n");
    return -1;
  }
}
