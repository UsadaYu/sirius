/**
 * @note Since the process is terminated before the detached threads, this test
 * case will cause a large amount of memory leaks.
 */
#include <sirius/sirius_thread.h>
#include <sirius/sirius_time.h>

#include <random>

#include "internal/utils.h"

static constexpr int NB_THREADS = 24;

static std::random_device rd;
static std::mt19937 gen(rd());
static std::uniform_int_distribution<> dis(0, 1);

static void *foo(void *arg) {
  (void)arg;

  char var[512];
  memset(var, 'q', sizeof(var));

  sirius_usleep(20 * 1000);

  if (dis(gen) == 1)
    return nullptr;

  sirius_thread_exit(nullptr);
  return nullptr;
}

int main() {
  auto init = Utils::Init();

  // clang-format off
  sirius_warnsp(
    log_purple
    "\n"
    "  Since the process is terminated before the detached threads,\n"
    "  this test case will cause a large amount of memory leaks\n"
    log_color_none);
  // clang-format on

  sirius_thread_t threads[NB_THREADS];
  sirius_thread_attr_t attr {};

  attr.detach_state = sirius_thread_detached;

  for (auto thread : threads) {
    int ret = sirius_thread_create(&thread, &attr, foo, nullptr);
    if (ret) {
      sirius_error("sirius_thread_create: %d\n", ret);
      return -1;
    }
  }

  // sirius_usleep(1000 * 1000);

  return 0;
}
