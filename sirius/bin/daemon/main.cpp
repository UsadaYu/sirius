// clang-format-off
#include "bin/debug.hpp"
// clang-format-on

#include "utils/log/daemon.hpp"

static inline auto main_log_manager() -> std::expected<void, std::string> {
  auto log_manager = std::make_unique<Utils::Log::Daemon::LogManager>();

  return log_manager->main();
}

int main(int argc, char **argv) {
  auto debug = Debug();
  auto &args_instance = Utils::Log::Daemon::Args::instance(argc, argv);

  switch (args_instance.get_type()) {
  case Utils::Log::Daemon::Args::ArgValue::kSpawn:
    if (auto ret = main_log_manager(); !ret.has_value()) {
      auto es = ret.error().append("\n");
      UTILS_WRITE(STDERR_FILENO, es.c_str(), es.size());
      return -1;
    }
    break;
  default:
    break;
  }

  return 1;
}
