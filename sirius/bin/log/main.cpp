// clang-format-off
#include "bin/debug.hpp"
// clang-format-on

#include <print>

#include "sirius/version.h"
#include "utils/log/exe.hpp"

using ExeArgs = ::Utils::Log::Exe::Args;

class Parser {
 public:
  Parser(ExeArgs &exe_args) : exe_args_(exe_args) {}

  ~Parser() = default;

  auto arg_daemon() -> std::expected<void, std::string> {
    std::string es;
    auto values = exe_args_.parser.get_all(ExeArgs::kArgDaemon);

    for (auto value : values) {
      if (value == ExeArgs::kArgDaemonSpawn) {
        if (auto ret = main_daemon(); !ret.has_value())
          return std::unexpected(ret.error());
      }
    }

    return {};
  }

  auto arg_version() -> std::expected<void, std::string> {
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

  auto arg_default() -> std::expected<void, std::string> {
    exe_args_.parser.print_usage("`" _SIRIUS_LOG_MODULE_NAME "`");

    return {};
  }

 private:
  ExeArgs &exe_args_;

  auto main_daemon() -> std::expected<void, std::string> {
    auto log_manager = std::make_unique<Utils::Log::Exe::Daemon>();

    return log_manager->main();
  }
};

class Main {
 public:
  /**
   * @throw `std::runtime_error`.
   */
  Main(int argc, char **argv)
      : exe_args_(ExeArgs::instance(argc, argv)), parser_(exe_args_) {}

  ~Main() = default;

  auto main_default() -> std::expected<void, std::string> {
    return parser_.arg_default();
  }

  auto main_arg(int argc, char **argv) -> std::expected<void, std::string> {
    for (size_t i = 1; i < static_cast<size_t>(argc); ++i) {
      std::string option, value;

      if (!exe_args_.parser.is_option_token(argv[i]))
        continue;

      if (auto ret = exe_args_.parser.extract(argv[i], option, value);
          !ret.has_value()) {
        return std::unexpected(ret.error());
      }

      auto fn_arg = [&]() -> std::expected<void, std::string> (Parser::*)() {
        if (option == ExeArgs::kArgDaemon) {
          return &Parser::arg_daemon;
        } else if (option == ExeArgs::kArgVersion) {
          return &Parser::arg_version;
        } else {
          return &Parser::arg_default;
        }
      }();

      if (auto ret = (parser_.*fn_arg)(); !ret.has_value())
        return std::unexpected(ret.error());
    }

    return {};
  }

 private:
  ExeArgs &exe_args_;
  ::Parser parser_;
};

int main(int argc, char **argv) {
  auto debug = Debug();
  std::string es;
  std::unique_ptr<Main> main;

  try {
    main = std::make_unique<Main>(argc, argv);
  } catch (const std::runtime_error &e) {
    es = e.what();
    goto label_error;
  } catch (...) {
    es = IO_E("exception: unknow");
    goto label_error;
  }

  if (argc == 1) {
    if (auto ret = main->main_default(); !ret.has_value()) {
      es = ret.error();
      goto label_error;
    }
    return 0;
  }

  if (auto ret = main->main_arg(argc, argv); !ret.has_value()) {
    es = ret.error();
    goto label_error;
  }

  return 0;

label_error:
#if (defined(_WIN32) || defined(_WIN64)) && defined(__GNUC__)
  std::cerr << es << std::endl;
#else
  std::println(std::cerr, "{0}", es);
#endif

  return -1;
}
