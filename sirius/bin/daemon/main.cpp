// clang-format-off
#include "bin/debug.hpp"
// clang-format-on

#include "utils/log/daemon.hpp"

static inline bool main_log_manager() {
  auto log_manager = std::make_unique<Utils::Log::Daemon::Daemon::LogManager>();

  return log_manager->main();
}

int main(int argc, char **argv) {
  auto debug = Debug();
  auto &args_instance = Utils::Log::Daemon::args_instance;

  if (!args_instance.parse(argc, argv))
    return EINVAL;

  switch (args_instance.get_type()) {
  case Utils::Log::Daemon::Args::ArgValue::kSpawn:
    return (int)!main_log_manager();
  default:
    break;
  }

  return 1;
}
