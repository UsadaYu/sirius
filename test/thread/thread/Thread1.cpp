#include <sirius/thread/thread.h>

#include <filesystem>
#include <iomanip>
#include <mutex>
#include <thread>

#include "internal/utils.h"

static constexpr int kNbThreads = 128;

static std::atomic<int> g_index = 1;
static std::mutex g_mutex;

void *foo(void *arg) {
  (void)arg;

  std::string nspace;

  {
    std::lock_guard lock(g_mutex);

    if (g_index.load() < 10) {
      nspace = "   ";
    } else if (g_index.load() < 100) {
      nspace = "  ";
    } else if (g_index.load() < 1000) {
      nspace = " ";
    } else {
      sirius_error("Please reduce the index\n");
      nspace = "\n";
    }

    utils_dprintf(1, "Foo: %d%s| ", g_index.load(), nspace.c_str());
    if (g_index.load() % 8 == 0) {
      utils_dprintf(1, "\n");
    }

    g_index++;
  }

  sirius_thread_exit(nullptr);
  return nullptr;
}

int main() {
  auto init = Utils::Init();

  sirius_thread_t threads[kNbThreads];

  // --- Join ---
  for (size_t i = 0; i < kNbThreads; ++i) {
    UTILS_ASSERT(!sirius_thread_create(threads + i, nullptr, foo, nullptr));
  }
  for (size_t i = 0; i < kNbThreads; ++i) {
    UTILS_ASSERT(!sirius_thread_join(threads[i], nullptr));
  }

  // --- Detach ---
  g_index = 1;
  utils_dprintf(1, "\n");
  memset(threads, 0, kNbThreads * sizeof(sirius_thread_t));
  sirius_thread_attr_t attr {};
  attr.detach_state = kSiriusThreadCreateDetached;
  for (size_t i = 0; i < kNbThreads; ++i) {
    UTILS_ASSERT(!sirius_thread_create(threads + i, &attr, foo, nullptr));
  }

  while (g_index.load() <= kNbThreads) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  return 0;
}
