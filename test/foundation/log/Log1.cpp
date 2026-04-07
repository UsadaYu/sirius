#include <sirius/foundation/thread.h>

#include <thread>

#include "inner/utils.h"

#ifndef _GEN_FILE_NAME
#  define _GEN_FILE_NAME "./_gen_log.txt"
#endif

namespace {
inline constexpr size_t kNbThreads = 4;
inline constexpr const char *kGenFileName = _GEN_FILE_NAME;

inline std::atomic<bool> g_exit_flag = false;

inline void thread_foo() {
  while (!g_exit_flag.load(std::memory_order_relaxed)) {
    ss_log_error("--------------------------------\n");
    ss_log_debug("[TID: %llu]\n", ss_thread_id());
    ss_log_info("[TID: %llu]\n", ss_thread_id());
    ss_log_warn("[TID: %llu]\n", ss_thread_id());

    ss_log_debugsp("[TID: %llu]\n", ss_thread_id());
    ss_log_infosp("[TID: %llu]\n", ss_thread_id());
    ss_log_warnsp("[TID: %llu]\n", ss_thread_id());
    ss_log_error("--------------------------------\n\n");

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
  }
}

inline int main_impl() {
  ss_log_config_t cfg {};
  cfg.out.flags = kSS_O_RDWR | kSS_O_CREAT | kSS_O_APPEND;
  cfg.out.mode = SS_FS_PERM_RW;

  std::jthread threads[kNbThreads];
  for (auto &t : threads) {
    t = std::jthread(thread_foo);
  }

  // --- Shared ---
  cfg.out.shared = SsThreadProcess::kSsThreadProcessShared;
  cfg.out.ansi_disable = 0;
  cfg.err = cfg.out;
  cfg.out.log_path = kGenFileName;
  cfg.err.log_path = kGenFileName;
  ss_log_configure(&cfg);

  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  cfg.out.log_path = nullptr;
  ss_log_configure(&cfg);

  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  cfg.err.log_path = nullptr;
  ss_log_configure(&cfg);

  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  cfg.err.ansi_disable = 1;
  ss_log_configure(&cfg);

  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  cfg.out.ansi_disable = 1;
  ss_log_configure(&cfg);

  // --- Private ---
  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  cfg.out.shared = SsThreadProcess::kSsThreadProcessPrivate;
  cfg.out.ansi_disable = 0;
  cfg.err = cfg.out;
  cfg.out.log_path = kGenFileName;
  cfg.err.log_path = kGenFileName;
  ss_log_configure(&cfg);

  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  cfg.out.log_path = nullptr;
  ss_log_configure(&cfg);

  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  cfg.err.log_path = nullptr;
  ss_log_configure(&cfg);

  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  cfg.err.ansi_disable = 1;
  ss_log_configure(&cfg);

  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  cfg.out.ansi_disable = 1;
  ss_log_configure(&cfg);

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
  cfg.out.shared = SsThreadProcess::kSsThreadProcessShared;
  cfg.err.log_path = nullptr;
  cfg.err.shared = SsThreadProcess::kSsThreadProcessShared;
  ss_log_configure(&cfg);
  ss_log_warnsp("--------------------------------\n");
  ss_log_warnsp("- Ultra-long log printing test\n");
  char ultra_long_string[40960];
  std::memset(ultra_long_string, 'Q', sizeof(ultra_long_string));
  ultra_long_string[sizeof(ultra_long_string) - 1] = '\0';
  ss_log_warnsp("- The length of the string to be printed: %zu\n",
                std::strlen(ultra_long_string));
  ss_logsp_impl(SS_LOG_LEVEL_INFO, _SIRIUS_LOG_MODULE_NAME, "- %s\n",
                ultra_long_string);
  ss_log_warnsp("--------------------------------\n");

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
