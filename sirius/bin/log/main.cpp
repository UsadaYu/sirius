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
          utrace_return(ret);
      }
    }
    return {};
  }

  auto arg_version() -> std::expected<void, UTrace> {
    auto msg = std::format("`{0}` version: {1}", _SIRIUS_LOG_MODULE_NAME,
                           SIRIUS_VERSION);
    utils::io_msg_outln(msg);
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
    return log_manager->main().utrace_transform_error_default();
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
    return parser_.arg_usage().utrace_transform_error_default();
  }

  auto main_arg(int argc, char **argv) -> std::expected<void, UTrace> {
    for (size_t i = 1; i < static_cast<size_t>(argc); ++i) {
      if (!exe_args_.parser.is_option_token(argv[i]))
        continue;

      std::string option, value;
      auto ret =
        exe_args_.parser.extract(argv[i], option, value).and_then([&]() {
          auto fn_arg = [&]() -> std::expected<void, UTrace> (Parser::*)() {
            if (option == u_log::exe::Args::kArgDaemon) {
              return &Parser::arg_daemon;
            } else if (option == u_log::exe::Args::kArgVersion) {
              return &Parser::arg_version;
            } else {
              return &Parser::arg_usage;
            }
          }();
          return (parser_.*fn_arg)();
        }).utrace_transform_error_default();
      if (!ret.has_value())
        ret;
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
  std::unique_ptr<sirius::bin::log::Main> main;

  try {
    main = std::make_unique<sirius::bin::log::Main>(argc, argv);
  } catch (const std::exception &e) {
    io_ln_error("{0}", e.what());
    return -1;
  } catch (...) {
    io_ln_error("exception: unknow");
    return -1;
  }

  if (argc == 1) {
    if (auto ret = main->main_usage(); !ret.has_value()) {
      io_ln_error("{0}", ret.error().join_self_all());
      return -1;
    }
    return 0;
  } else {
    if (auto ret = main->main_arg(argc, argv); !ret.has_value()) {
      io_ln_error("{0}", ret.error().join_self_all());
      return -1;
    }
    return 0;
  }
}
