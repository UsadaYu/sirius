#pragma once

#include <array>
#include <functional>
#include <span>
#include <vector>

#include "utils/args.hpp"
#include "utils/env.h"
#include "utils/log/shm.hpp"
#include "utils/time.hpp"

namespace Utils {
namespace Log {
namespace Exe {
/**
 * @implements Singleton pattern.
 */
class Args {
 public:
  static constexpr const char *kArgHelp = "help";
  static constexpr const char *kArgVersion = "version";

  static constexpr const char *kArgDaemon = "daemon";
  static constexpr const char *kArgDaemonSpawn = "spawn";

  static inline ::Utils::Args::Parser parser;

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

    std::call_once(once_flag, [&]() {
      /**
       * @note Even if `once_flag_` will not be marked after an exception is
       * thrown, it is no longer desired here for the process to survive after
       * this error.
       */
      instance.init(argc, argv);
    });

    return instance;
  }

  static std::vector<std::string> &cmds_daemon() {
    static std::vector<std::string> cmd = {std::string("--").append(kArgDaemon),
                                           kArgDaemonSpawn};

    return cmd;
  }

 private:
  /**
   * @throw `std::runtime_error`.
   */
  void init(int argc, char **argv) {
    auto check = [](const std::expected<void, std::string> &ret) {
      if (!ret.has_value())
        throw std::runtime_error(ret.error());
    };

    // clang-format off
    check(parser.add_option(kArgHelp, false, {}, false, false,
                            "Helper"));
    check(parser.add_option(kArgVersion, false, {}, false, false,
                            "Print version"));
    check(parser.add_option(kArgDaemon, true, {kArgDaemonSpawn}, false, false,
                            "Spawn the daemon"));
    // clang-format on

    check(parser.parse(argc, argv));
  }
};

/**
 * @implements Singleton pattern.
 */
class Exe {
 private:
  Exe() : path_("") {}

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
    std::filesystem::path exe_set;
    std::filesystem::path exe_env = path_env();
    std::filesystem::path exe_dll;
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
        auto es = IO_INFOSP("\nThe daemon executable file: {0}", path.string())
                    .append("\n");
        utils_write(STDOUT_FILENO, es.c_str(), es.size());
        return path;
      }
    }

    return std::unexpected(ENOENT);
  }

 private:
  std::filesystem::path path_;
  std::mutex mutex_;

  std::filesystem::path path_env() {
    std::string env = Env::get_env(SIRIUS_ENV_LOG_EXE_PATH);

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
      es = Io::win_last_error(dw_err, "GetModuleHandleExW")
             .append(Io::row_gs("\n{0}", utils_pretty_func))
             .append("\n");
      utils_write(STDERR_FILENO, es.c_str(), es.size());
      return {};
    }

    DWORD length = GetModuleFileNameW(modele, path, MAX_PATH);
    if (length == 0) {
      const DWORD dw_err = GetLastError();
      es = Io::win_last_error(dw_err, "GetModuleFileNameW")
             .append(Io::row_gs("\n{0}", utils_pretty_func))
             .append("\n");
      utils_write(STDERR_FILENO, es.c_str(), es.size());
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

class Daemon {
 public:
  Daemon()
      : log_shm_(std::make_unique<Shm::Shm>(Shm::MasterType::kDaemon)),
        fd_out_(STDOUT_FILENO),
        fd_err_(STDERR_FILENO) {}

  ~Daemon() = default;

  auto main() -> std::expected<void, std::string> {
    std::expected<void, std::string> ret;

    if (ret = Shm::GMutex::create(); !ret.has_value())
      return std::unexpected(ret.error());
    if (ret = Shm::GMutex::lock(); !ret.has_value())
      goto label_free1;
    if (ret = log_shm_->init(); !ret.has_value())
      goto label_free2;

    (void)Shm::GMutex::unlock();

    ret = daemon_main();

    log_shm_->deinit();
  label_free2:
    (void)Shm::GMutex::unlock();
  label_free1:
    Shm::GMutex::destroy();

    return ret;
  }

  void log_write(int level, const void *buffer, size_t size) {
    int fd = level <= SIRIUS_LOG_LEVEL_WARN ? fd_err_ : fd_out_;

    utils_write(fd, buffer, size);
  }

 private:
  std::unique_ptr<Shm::Shm> log_shm_;

  std::jthread thread_consumer_;
  std::jthread thread_monitor_;

  int fd_out_;
  int fd_err_;

  /**
   * @note After this function ends, the `Shm::GMutex` will be held.
   */
  auto daemon_main() -> std::expected<void, std::string> {
    std::string es;
    auto header = log_shm_->header;

    es = IO_INFOSP("The daemon starts (PID: {0})", Process::pid()).append("\n");
    utils_write(STDOUT_FILENO, es.c_str(), es.size());

    thread_consumer_ =
      std::jthread(std::bind_front(&Daemon::thread_consumer, this));
    thread_monitor_ =
      std::jthread(std::bind_front(&Daemon::thread_monitor, this));

    header->is_daemon_ready.store(true, std::memory_order_relaxed);

    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(250));

      (void)Shm::GMutex::lock();

      {
        log_shm_->lock_guard();

        if (header->attached_count == Log::kProcessNbDaemon) [[unlikely]] {
          header->magic = 0;
          header->is_daemon_ready.store(false, std::memory_order_relaxed);
          break;
        }
      }

      (void)Shm::GMutex::unlock();
    }

    thread_monitor_.request_stop();
    thread_consumer_.request_stop();

    if (thread_monitor_.joinable()) {
      thread_monitor_.join();
    }
    if (thread_consumer_.joinable()) {
      thread_consumer_.join();
    }

    es = IO_INFOSP("The daemon ended (PID: {0})", Process::pid()).append("\n");
    utils_write(STDOUT_FILENO, es.c_str(), es.size());

    return {};
  }

  void thread_consumer(std::stop_token stop_token) {
    int exit_counter = 0;
    auto header = log_shm_->header;

    while (true) {
      uint64_t read_index = header->read_index.load(std::memory_order_acquire);
      uint64_t write_index =
        header->write_index.load(std::memory_order_acquire);

      if (read_index >= write_index) {
        if (stop_token.stop_requested())
          break;
        std::this_thread::sleep_for(std::chrono::microseconds(150));
        continue;
      }

      uint64_t retry_times = header->capacity - (write_index - read_index) + 10;
      retry_times = likely(retry_times < 2000) ? retry_times : 2000;

      auto opt_slot = get_slot(read_index, 100);
      if (opt_slot) [[likely]] {
        Shm::Slot &slot = opt_slot->get();
        Shm::Buffer &buffer = slot.buffer;
        if (buffer.type == Shm::DataType::kLog) [[likely]] {
          auto &data = buffer.data.log;
          log_write(buffer.level, (void *)data.buf, data.buf_size);
        } else {
          /* ----------------------------------------- */
          /* ---------------- Not Yet ---------------- */
          /* ----------------------------------------- */
        }
        slot.state.store(Shm::SlotState::kFree, std::memory_order_release);
      } else {
        if (stop_token.stop_requested()) {
          if (++exit_counter >= 5)
            break;
        } else {
          exit_counter = 0;
        }
      }

      header->read_index.fetch_add(1, std::memory_order_release);
    }
  }

  void thread_monitor(std::stop_token stop_token) {
    auto header = log_shm_->header;
    auto slots = log_shm_->slots;

    while (!stop_token.stop_requested()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));

      uint64_t now = Time::get_monotonic_steady_ms();

      for (size_t i = 0; i < header->capacity; ++i) {
        Shm::SlotState s = slots[i].state.load(std::memory_order_acquire);

        if (s == Shm::SlotState::kWaiting) {
          uint64_t ts = slots[i].timestamp_ms.load(std::memory_order_relaxed);
          if (now - ts > Log::kShmSlotResetTimeoutMilliseconds) {
            std::string es;

            es = IO_WARNSP("Recovering stuck slot: {0}", i).append("\n");
            utils_write(STDERR_FILENO, es.c_str(), es.size());

            /**
             * @note Construct a forged log indicating data loss.
             */
            es =
              IO_WARNSP("Slot recovered/skipped due to timeout").append("\n");
            slots[i].buffer.type = Shm::DataType::kLog;
            slots[i].buffer.level = SIRIUS_LOG_LEVEL_ERROR;

            auto &log = slots[i].buffer.data.log;
            log.buf_size = es.size();
            std::memcpy(log.buf, es.c_str(), log.buf_size + 1);

            /**
             * @note Let it be `kReady`, so that consumers can read it, thereby
             * advancing the index.
             */
            slots[i].state.store(Shm::SlotState::kReady,
                                 std::memory_order_release);
          }
        }
      }
    }
  }

  [[nodiscard]] std::optional<std::reference_wrapper<Shm::Slot>>
  get_slot(const uint64_t read_index, const int retry_times) {
    size_t slot_idx = read_index & (Log::kShmCapacity - 1);
    Shm::Slot &slot = log_shm_->slots[slot_idx];

    int retries = 0;
    while (slot.state.load(std::memory_order_acquire) !=
           Shm::SlotState::kReady) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      if (++retries > retry_times) {
        auto es =
          IO_WARNSP("Skip corrupted slot index: {0}", read_index).append("\n");
        utils_write(STDERR_FILENO, es.c_str(), es.size());
        return std::nullopt;
      }
    }

    return slot;
  }
};
} // namespace Exe
} // namespace Log
} // namespace Utils
