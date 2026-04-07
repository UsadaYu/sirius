#include <sirius/thread/thread.h>

#include <iomanip>
#include <mutex>
#include <thread>

#include "inner/utils.h"

namespace {
inline constexpr int kNbThreads = 128;

inline std::atomic<int> g_index = 1;
inline std::mutex g_mutex;

inline void *foo(void *arg) {
  (void)arg;

  std::string nspace;

  {
    auto lock = std::lock_guard(g_mutex);

    if (g_index.load() < 10) {
      nspace = "   ";
    } else if (g_index.load() < 100) {
      nspace = "  ";
    } else if (g_index.load() < 1000) {
      nspace = " ";
    } else {
      ss_log_error("Please reduce the index\n");
      nspace = "\n";
    }

    utils_dprintf(1, "Foo: %d%s| ", g_index.load(), nspace.c_str());
    if (g_index.load() % 8 == 0) {
      utils_dprintf(1, "\n");
    }

    ++g_index;
  }

  ss_thread_exit(nullptr);
  return nullptr;
}

inline int main_impl() {
  ss_thread_t threads[kNbThreads];

  // --- Join ---
  for (auto &t : threads) {
    UTILS_ASSERT(!ss_thread_create(&t, nullptr, foo, nullptr));
  }
  for (auto &t : threads) {
    UTILS_ASSERT(!ss_thread_join(t, nullptr));
  }

  // --- Detach ---
  g_index = 1;
  utils_dprintf(1, "\n");
  std::memset(threads, 0, kNbThreads * sizeof(ss_thread_t));
  ss_thread_attr_t attr {};
  attr.detach_state = kSsThreadCreateDetached;
  for (auto &t : threads) {
    UTILS_ASSERT(!ss_thread_create(&t, &attr, foo, nullptr));
  }

  while (g_index.load() <= kNbThreads) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

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
