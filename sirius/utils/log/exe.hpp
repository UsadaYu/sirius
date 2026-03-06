#pragma once

#include "utils/args.hpp"
#include "utils/env.h"
#include "utils/file.hpp"

namespace sirius {
namespace utils {
namespace log {
namespace exe {
/**
 * @implements Singleton pattern.
 */
class Args {
 public:
  static constexpr const char *kArgHelp = "help";
  static constexpr const char *kArgVersion = "version";

  static constexpr const char *kArgDaemon = "daemon";
  static constexpr const char *kArgDaemonSpawn = "spawn";

  static inline args::Parser parser;

 private:
  Args() = default;

  ~Args() = default;

 public:
  Args(const Args &) = delete;
  Args &operator=(const Args &) = delete;

  /**
   * @throw `std::runtime_error`.
   */
  static Args &instance(int argc, char **argv) {
    static Args instance;
    static std::once_flag once_flag;
    static bool initialized = false;

    std::string ret_msg;
    std::call_once(once_flag, [&argc, &argv, &ret_msg]() {
      if (auto ret = instance.init(argc, argv); !ret.has_value()) {
        ret_msg = ret.error();
        return;
      }
      initialized = true;
    });

    if (!initialized)
      throw std::runtime_error(ret_msg);

    return instance;
  }

  static std::vector<std::string> cmds_daemon() {
    static std::vector<std::string> cmds = {
      std::string("--").append(kArgDaemon), kArgDaemonSpawn};

    return cmds;
  }

 private:
  auto init(int argc, char **argv) -> std::expected<void, std::string> {
    std::expected<void, std::string> ret;
#define E(e) \
  do { \
    ret = e; \
    if (!ret.has_value()) \
      return std::unexpected(ret.error()); \
  } while (0)

    // clang-format off
    E(parser.add_option(kArgHelp, false, {}, false, false, "Helper"));
    E(parser.add_option(kArgVersion, false, {}, false, false, "Print version"));
    E(parser.add_option(kArgDaemon, true, {kArgDaemonSpawn}, false, false, "Daemon"));
    E(parser.parse(argc, argv));
    // clang-format on

    return {};
#undef E
  }
};

/**
 * @implements Singleton pattern.
 */
class Exe {
 private:
  Exe() = default;

  ~Exe() = default;

 public:
  Exe(const Exe &) = delete;
  Exe &operator=(const Exe &) = delete;

  static Exe &instance() {
    static Exe instance;

    return instance;
  }

 public:
  /**
   * @return 0 on success, or an `errno` value on failure.
   */
  int set_path(std::filesystem::path path) {
    if (path.empty())
      return EINVAL;

    if (!std::filesystem::exists(path))
      return ENOENT;

    {
      std::lock_guard lock(mutex_);

      path_ = path;
    }

    return 0;
  }

  /**
   * @note Priority (from top to bottom).
   *
   * - (1) Api configuration.
   *
   * - (2) Environment.
   *
   * - (3) Search from shared library (dll only).
   *
   * - (4) Install.
   */
  auto get_path() -> std::expected<std::filesystem::path, int /* errno */> {
    std::filesystem::path exe_set {};
    std::filesystem::path exe_env = path_env();
    std::filesystem::path exe_dll {};
    std::filesystem::path exe_install = path_install();

    {
      std::lock_guard lock(mutex_);

      exe_set = path_;
    }

    if (auto ret = File::get_exe_path_matrix(std::string(_SIRIUS_EXE_LOG_NAME),
                                             current_shared_dir())) {
      exe_dll = ret.has_value() ? ret.value() : "";
    }

    std::vector paths = {
      exe_set,
      exe_env,
      exe_dll,
      exe_install,
    };

    for (auto path : paths) {
      if (!path.empty() && std::filesystem::exists(path)) {
        io_outln(IO_ISP("\nThe daemon executable file: {0}", path.string()));
        return path;
      }
    }

    return std::unexpected(ENOENT);
  }

 private:
  std::filesystem::path path_ {};
  std::mutex mutex_ {};

  std::filesystem::path path_env() {
    std::string env = env::get_env(SIRIUS_ENV_LOG_EXE_PATH);

    auto path = std::filesystem::path(env);

    return (!path.empty() && std::filesystem::exists(path))
      ? path
      : std::filesystem::path {};
  }

  std::filesystem::path path_install() {
    auto ret =
      File::get_exe_path_matrix(std::string(_SIRIUS_EXE_LOG_NAME),
                                std::filesystem::path(_SIRIUS_EXE_DIR));
    if (!ret.has_value())
      return std::filesystem::path {};

    auto path = ret.value();

    return (!path.empty() && std::filesystem::exists(path))
      ? path
      : std::filesystem::path {};
  }

#ifdef _SIRIUS_WIN_DLL
  static std::filesystem::path current_shared_dir() {
    std::string es;
    HMODULE modele = nullptr;
    wchar_t path[MAX_PATH];

    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)&current_shared_dir, &modele)) {
      const DWORD dw_err = GetLastError();
      io_errln(Io::win_err(dw_err, "GetModuleHandleExW", utils_pretty_func));
      return {};
    }

    DWORD length = GetModuleFileNameW(modele, path, MAX_PATH);
    if (length == 0) {
      const DWORD dw_err = GetLastError();
      io_errln(Io::win_err(dw_err, "GetModuleFileNameW", utils_pretty_func));
      return {};
    }

    auto bin_dir = std::filesystem::path(path).parent_path();

    return bin_dir;
  }
#else
  static std::filesystem::path current_shared_dir() {
    return {};
  }
#endif
};
} // namespace exe
} // namespace log
} // namespace utils
} // namespace sirius
