// clang-format-off
#include "bin/debug.hpp"
// clang-format-on

#include <print>

#include "bin/log/daemon.hpp"
#include "sirius/version.h"
#include "utils/log/exe.hpp"

namespace sirius {
namespace bin {
namespace log {
namespace u_log = utils::log;

class Parser {
 public:
  Parser(u_log::exe::Args &exe_args) : exe_args_(exe_args) {}

  ~Parser() = default;

  auto arg_daemon() -> std::expected<void, UTrace> {
    auto values = exe_args_.parser.get_all(u_log::exe::Args::kArgDaemon);
    for (auto value : values) {
      if (value == u_log::exe::Args::kArgDaemonSpawn) {
        if (auto ret = main_daemon(); !ret.has_value())
          UTRACE_RETURN(ret);
      }
    }
    return {};
  }

  auto arg_version() -> std::expected<void, UTrace> {
#if (defined(_WIN32) || defined(_WIN64)) && defined(__GNUC__)
    std::cout << std::format("`{0}` version: {1}", _SIRIUS_LOG_MODULE_NAME,
                             SIRIUS_VERSION)
              << std::endl;
#else
    std::println(std::cout, "`{0}` version: {1}", _SIRIUS_LOG_MODULE_NAME,
                 SIRIUS_VERSION);
#endif
    return {};
  }

  auto arg_usage() -> std::expected<void, UTrace> {
    exe_args_.parser.print_usage("`" _SIRIUS_LOG_MODULE_NAME "`");
    return {};
  }

 private:
  u_log::exe::Args &exe_args_;

  auto main_daemon() -> std::expected<void, UTrace> {
    auto log_manager = std::make_unique<Daemon>();
    return log_manager->main();
  }
};

class Main {
 public:
  /**
   * @throw `std::runtime_error`.
   */
  Main(int argc, char **argv)
      : exe_args_(u_log::exe::Args::instance(argc, argv)), parser_(exe_args_) {}

  ~Main() = default;

  auto main_usage() -> std::expected<void, UTrace> {
    return parser_.arg_usage();
  }

  auto main_arg(int argc, char **argv) -> std::expected<void, UTrace> {
    for (size_t i = 1; i < static_cast<size_t>(argc); ++i) {
      if (!exe_args_.parser.is_option_token(argv[i]))
        continue;

      std::string option, value;
      if (auto ret = exe_args_.parser.extract(argv[i], option, value);
          !ret.has_value()) {
        UTRACE_RETURN(ret);
      }

      auto fn_arg = [&]() -> std::expected<void, UTrace> (Parser::*)() {
        if (option == u_log::exe::Args::kArgDaemon) {
          return &Parser::arg_daemon;
        } else if (option == u_log::exe::Args::kArgVersion) {
          return &Parser::arg_version;
        } else {
          return &Parser::arg_usage;
        }
      }();
      if (auto ret = (parser_.*fn_arg)(); !ret.has_value())
        UTRACE_RETURN(ret);
    }

    return {};
  }

 private:
  u_log::exe::Args &exe_args_;
  Parser parser_;
};
} // namespace log
} // namespace bin
} // namespace sirius

int main(int argc, char **argv) {
  auto debug = Debug();
  std::string es;
  std::unique_ptr<sirius::bin::log::Main> main;

  try {
    main = std::make_unique<sirius::bin::log::Main>(argc, argv);
  } catch (const std::runtime_error &e) {
    es = e.what();
    goto label_error;
  } catch (...) {
    es = io_str_error("exception: unknow");
    goto label_error;
  }

  if (argc == 1) {
    if (auto ret = main->main_usage(); !ret.has_value()) {
      es = ret.error().join_self_all();
      goto label_error;
    }
    return 0;
  }

  if (auto ret = main->main_arg(argc, argv); !ret.has_value()) {
    es = ret.error().join_self_all();
    goto label_error;
  }

  return 0;

label_error:
  io_ln_error("{0}", es);
  return -1;
}
