/**
 * @note Since the process is terminated before the detached threads, this test
 * case will cause a large amount of memory leaks.
 */
#include <sirius/thread/thread.h>

#include <random>
#include <thread>

#include "internal/utils.h"

static constexpr int kNbThreads = 48;

static std::random_device rd;
static std::mt19937 gen(rd());
static std::uniform_int_distribution<> dis(0, 1);

static void *foo(void *arg) {
  (void)arg;

  char var[512];
  memset(var, 'q', sizeof(var));

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  if (dis(gen) == 1)
    return nullptr;

  sirius_thread_exit(nullptr);
  return nullptr;
}

int main() {
  auto init = utils::Init();

  // clang-format off
  sirius_warnsp(
    "\nSince the process is terminated before the detached threads,"
    "\nthis test case will cause a large amount of memory leaks\n");
  // clang-format on

  sirius_thread_t threads[kNbThreads];
  sirius_thread_attr_t attr {};

  attr.detach_state = kSiriusThreadCreateDetached;

  for (auto &t : threads) {
    if (int ret = sirius_thread_create(&t, &attr, foo, nullptr); ret) {
      sirius_error("sirius_thread_create: %d\n", ret);
      return -1;
    }
  }

  // std::this_thread::sleep_for(std::chrono::milliseconds(500));

  return 0;
}
