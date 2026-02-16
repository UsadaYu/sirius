#include <sirius/foundation/thread.h>

#include <thread>

#include "internal/utils.h"

#define NB_THREADS (4)
#ifndef GEN_FILE_NAME
#  define GEN_FILE_NAME "./_gen_log.txt"
#endif

static bool g_exit_flag = false;

static void thread_foo() {
  while (!g_exit_flag) {
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
  auto init = Utils::Init();

  sirius_log_config_t cfg = {};

  cfg.out.log_path = GEN_FILE_NAME;
  cfg.out.shared = SiriusThreadProcess::kSiriusThreadProcessShared;
  cfg.err.log_path = GEN_FILE_NAME;
  cfg.err.shared = SiriusThreadProcess::kSiriusThreadProcessShared;
  sirius_log_configure(&cfg);

  std::thread threads[NB_THREADS];
  for (int i = 0; i < NB_THREADS; ++i) {
    threads[i] = std::thread(thread_foo);
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
  g_exit_flag = true;

  for (int i = 0; i < NB_THREADS; ++i) {
    threads[i].join();
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
  char ultra_long_string[40960] = {0};
  for (size_t i = 0; i < sizeof(ultra_long_string) - 1; ++i) {
    ultra_long_string[i] = 'Q';
  }
  sirius_warnsp("- The length of the string to be printed: %zu\n",
                strlen(ultra_long_string));
  sirius_logsp_impl(SIRIUS_LOG_LEVEL_NONE, "Ultra", LOG_PURPLE,
                    _SIRIUS_LOG_PRINT_NAME, "- %s\n", ultra_long_string);
  sirius_warnsp("--------------------------------\n");

  return 0;
}
