/**
 * @note Since the process is terminated before the detached threads, this test
 * case will cause a large amount of memory leaks.
 */
#include <sirius/thread/thread.h>

#include <random>
#include <thread>

#include "inner/utils.h"

namespace {
inline constexpr int kNbThreads = 48;

inline std::random_device rd;
inline std::mt19937 gen(rd());
inline std::uniform_int_distribution<> dis(0, 1);

inline void *foo(void *arg) {
  (void)arg;

  char var[512];
  std::memset(var, 'q', sizeof(var));

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  if (dis(gen) == 1)
    return nullptr;

  ss_thread_exit(nullptr);
  return nullptr;
}

inline int main_impl() {
  ss_thread_t threads[kNbThreads];
  ss_thread_attr_t attr {};

  attr.detach_state = kSsThreadCreateDetached;

  for (auto &t : threads) {
    if (int ret = ss_thread_create(&t, &attr, foo, nullptr); ret) {
      ss_log_error("ss_thread_create: %d\n", ret);
      return -1;
    }
  }

  // std::this_thread::sleep_for(std::chrono::milliseconds(500));

  return 0;
}
} // namespace

int main() {
  auto init = utils::Init();

  // clang-format off
  ss_log_warnsp(
    "\nSince the process is terminated before the detached threads,"
    "\nthis test case will cause a large amount of memory leaks\n");
  // clang-format on

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
