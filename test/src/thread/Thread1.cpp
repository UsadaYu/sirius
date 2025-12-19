#include <sirius/sirius_thread.h>

#include <filesystem>
#include <iomanip>
#include <mutex>

#include "internal/utils.h"

constexpr int NB_THREADS = 512;

static int g_index = 1;
static std::mutex g_mutex;

void *foo(void *args) {
  (void)args;

  std::string nspace;

  {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_index < 10) {
      nspace = "   ";
    } else if (g_index < 100) {
      nspace = "  ";
    } else if (g_index < 1000) {
      nspace = " ";
    } else {
      std::cerr << "Too many threads, index: " << g_index << std::endl;
      sirius_thread_exit(nullptr);
    }

    std::cout << "Foo: " << g_index << nspace << "| ";
    if (g_index % 8 == 0)
      std::cout << std::endl;

    g_index++;
  }

  return nullptr;
}

int main() {
  auto init = Utils::Init();

  sirius_thread_t thread[NB_THREADS];

  for (size_t i = 0; i < NB_THREADS; ++i) {
    sirius_thread_create(thread + i, nullptr, foo, nullptr);
  }

  for (size_t i = 0; i < NB_THREADS; ++i) {
    sirius_thread_join(thread[i], nullptr);
  }

  return 0;
}
