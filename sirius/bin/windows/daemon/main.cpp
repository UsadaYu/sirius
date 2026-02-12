// clang-format off
#include "bin/windows/utils.h"
// clang-format on

#ifdef _SIRIUS_WIN_CRTDBG
#  define _CRTDBG_MAP_ALLOC
#  include <crtdbg.h>
#  include <stdlib.h>
#  include <string.h>
#endif

// clang-format off
#undef UTILS_LOG_SHM_HPP_IS_DAEMON_
#define UTILS_LOG_SHM_HPP_IS_DAEMON_ 1
#include "utils/log/shm.hpp"
#include "utils/log/daemon.hpp"
// clang-format on

void thread_log() {
  auto log_manager = std::make_unique<Utils::Log::Daemon::Daemon::LogManager>();

  log_manager->main();
}

static inline void main_dbg() {
#ifdef _SIRIUS_WIN_CRTDBG
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
#endif
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
  main_dbg();

  if (!Utils::Log::Daemon::Daemon::check())
    return EINVAL;

  std::jthread thread_log_(thread_log);

  thread_log_.join();

  return 0;
}
