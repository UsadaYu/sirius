#include <sirius/foundation/thread.h>

#include <thread>

#include "inner/utils.h"

#define NB_THREADS (4)
#ifndef GEN_FILE_NAME
#  define GEN_FILE_NAME "./_gen_log.txt"
#endif

static std::atomic<bool> g_exit_flag = false;

static void thread_foo() {
  while (!g_exit_flag.load(std::memory_order_relaxed)) {
    sirius_error("--------------------------------\n");
    sirius_debug("[TID: %llu]\n", sirius_thread_id());
    sirius_info("[TID: %llu]\n", sirius_thread_id());
    sirius_warn("[TID: %llu]\n", sirius_thread_id());

    sirius_debugsp("[TID: %llu]\n", sirius_thread_id());
    sirius_infosp("[TID: %llu]\n", sirius_thread_id());
    sirius_warnsp("[TID: %llu]\n", sirius_thread_id());
    sirius_error("--------------------------------\n\n");

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
  }
}

int main() {
  auto init = utils::Init();

  sirius_log_config_t cfg = {};

  cfg.out.log_path = GEN_FILE_NAME;
  cfg.out.shared = SiriusThreadProcess::kSiriusThreadProcessShared;
  cfg.err.log_path = GEN_FILE_NAME;
  cfg.err.shared = SiriusThreadProcess::kSiriusThreadProcessShared;
  sirius_log_configure(&cfg);

  std::jthread threads[NB_THREADS];
  for (auto &t : threads) {
    t = std::jthread(thread_foo);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  cfg.out.log_path = nullptr;
  cfg.out.shared = SiriusThreadProcess::kSiriusThreadProcessPrivate;
  sirius_log_configure(&cfg);

  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  cfg.err.log_path = nullptr;
  cfg.err.shared = SiriusThreadProcess::kSiriusThreadProcessPrivate;
  sirius_log_configure(&cfg);

  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  cfg.out.log_path = GEN_FILE_NAME;
  cfg.err.log_path = GEN_FILE_NAME;
  sirius_log_configure(&cfg);

  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  g_exit_flag.store(true, std::memory_order_relaxed);

  for (auto &t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }

  /**
   * @note
   * Ultra-long log printing test.
   */
  cfg.out.log_path = nullptr;
  cfg.out.shared = SiriusThreadProcess::kSiriusThreadProcessShared;
  cfg.err.log_path = nullptr;
  cfg.err.shared = SiriusThreadProcess::kSiriusThreadProcessShared;
  sirius_log_configure(&cfg);
  sirius_warnsp("--------------------------------\n");
  sirius_warnsp("- Ultra-long log printing test\n");
  char ultra_long_string[40960];
  std::memset(ultra_long_string, 'Q', sizeof(ultra_long_string));
  ultra_long_string[sizeof(ultra_long_string) - 1] = '\0';
  sirius_warnsp("- The length of the string to be printed: %zu\n",
                std::strlen(ultra_long_string));
  sirius_logsp_impl(SIRIUS_LOG_LEVEL_INFO, _SIRIUS_LOG_MODULE_NAME, "- %s\n",
                    ultra_long_string);
  sirius_warnsp("--------------------------------\n");

  return 0;
}
