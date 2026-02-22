#pragma once

#include <functional>
#include <vector>

#include "utils/args.hpp"
#include "utils/env.h"
#include "utils/log/shm.hpp"
#include "utils/time.hpp"

namespace Utils {
namespace Log {
namespace Daemon {
class Args {
 public:
  enum class ArgValue : int {
    kNone = 0,
    kSpawn = 10,
  };

  static inline const std::string kArgSpawn = "spawn";

 private:
  Args() = default;

  ~Args() = default;

 public:
  Args(const Args &) = delete;
  Args &operator=(const Args &) = delete;

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

  ArgValue get_type() const {
    if (parser_.has(kArgSpawn) && !parser_.get(kArgSpawn).empty()) {
      return ArgValue::kSpawn;
    } else {
      return ArgValue::kNone;
    }
  }

  static std::string &arg_spawn_value() {
    static std::string value = std::string(_SIRIUS_NAMESPACE) + "_" +
      Ns::generate_namespace_prefix(Log::kDaemonArgExeSpawn);

    return value;
  }

  static std::vector<std::string> &arg_spawn_cmds() {
    static std::vector<std::string> cmd = {"--" + kArgSpawn, arg_spawn_value()};

    return cmd;
  }

 private:
  ::Utils::Args::Parser parser_;
  std::unordered_multimap<std::string, std::string> list_;

  void init(int argc, char **argv) {
    parser_.add_option(kArgSpawn, {arg_spawn_value()}, true, false,
                       "Run the executable");
    list_.emplace(kArgSpawn, arg_spawn_value());

    auto parse_ret = parser_.parse(argc, argv);
    if (!parse_ret.has_value()) {
      throw std::runtime_error(parse_ret.error());
    }
  }
};

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
   * - (4) Dafault.
   */
  auto get_path() -> std::expected<std::filesystem::path, int /* errno */> {
    std::filesystem::path exe_set;
    std::filesystem::path exe_dll;
    std::filesystem::path exe_default;

    {
      std::lock_guard lock(mutex_);

      exe_set = path_;
    }

    std::string const kExeDaemonName = std::string(_SIRIUS_EXE_DAEMON_NAME);
    auto exe_dll_ret =
      File::get_exe_path_matrix(kExeDaemonName, current_shared_dir());
    exe_dll = exe_dll_ret.has_value() ? exe_dll_ret.value() : "";

    auto exe_default_ret = default_path();
    exe_default = exe_default_ret.has_value() ? exe_default_ret.value() : "";

    std::vector paths = {
      exe_set,
      env_path(),
      exe_dll,
      exe_default,
    };

    for (auto path : paths) {
      if (!path.empty() && std::filesystem::exists(path)) {
        auto es = std::format(
          "{0}{1}", Io::io().s_info(""),
          Io::row("The daemon executable file: {0}\n", path.string()));
        UTILS_WRITE(STDOUT_FILENO, es.c_str(), es.size());
        return path;
      }
    }

    return std::unexpected(ENOENT);
  }

 private:
  std::filesystem::path path_;
  std::mutex mutex_;

  static std::filesystem::path env_path() {
    std::string env = Env::get_env(SIRIUS_ENV_EXE_DAEMON_PATH);

    auto path = std::filesystem::path(env);

    return (!path.empty() && std::filesystem::exists(path)) ? path : "";
  }

  auto default_path() -> std::expected<std::filesystem::path, int /* errno */> {
    return File::get_exe_path_matrix(std::string(_SIRIUS_EXE_DAEMON_NAME),
                                     std::filesystem::path(_SIRIUS_EXE_DIR));
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
      es = Io::win_last_error(dw_err, "GetModuleHandleExW") +
        Io::row("{0}\n", utils_pretty_function);
      UTILS_WRITE(STDERR_FILENO, es.c_str(), es.size());
      return "";
    }

    DWORD length = GetModuleFileNameW(modele, path, MAX_PATH);
    if (length == 0) {
      const DWORD dw_err = GetLastError();
      es = Io::win_last_error(dw_err, "GetModuleFileNameW") +
        Io::row("{0}\n", utils_pretty_function);
      UTILS_WRITE(STDERR_FILENO, es.c_str(), es.size());
      return "";
    }

    auto bin_dir = std::filesystem::path(path).parent_path();

    return bin_dir;
  }
#else
  static std::filesystem::path current_shared_dir() {
    return "";
  }
#endif
};

class LogManager {
 public:
  LogManager()
      : log_shm_(std::make_unique<Shm::Shm>(Shm::MasterType::kDaemon)),
        fd_out_(STDOUT_FILENO),
        fd_err_(STDERR_FILENO) {}

  ~LogManager() = default;

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

    UTILS_WRITE(fd, buffer, size);
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

    es = std::format("{0}The daemon starts (PID: {1})\n", Io::io().s_info(""),
                     Process::pid());
    UTILS_WRITE(STDOUT_FILENO, es.c_str(), es.size());

    thread_consumer_ =
      std::jthread(std::bind_front(&LogManager::thread_consumer, this));
    thread_monitor_ =
      std::jthread(std::bind_front(&LogManager::thread_monitor, this));

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

    es = std::format("{0}The daemon ended (PID: {1})\n", Io::io().s_info(""),
                     Process::pid());
    UTILS_WRITE(STDOUT_FILENO, es.c_str(), es.size());

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

            es = std::format("{0}Recovering stuck slot: {1}\n",
                             Io::io().s_warn(""), i);
            UTILS_WRITE(STDERR_FILENO, es.c_str(), es.size());

            /**
             * @note Construct a forged log indicating data loss.
             */
            es = std::format("{0}Slot recovered/skipped due to timeout\n",
                             Io::io().s_error(""));
            const char *err_msg = es.c_str();
            slots[i].buffer.type = Shm::DataType::kLog;
            slots[i].buffer.level = SIRIUS_LOG_LEVEL_ERROR;

            auto &log = slots[i].buffer.data.log;
            log.buf_size = std::strlen(err_msg);
            std::memcpy(log.buf, err_msg, log.buf_size + 1);

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
        auto es = std::format("{0}Skip corrupted slot index: {1}\n",
                              Io::io().s_warn(""), read_index);
        UTILS_WRITE(STDERR_FILENO, es.c_str(), es.size());
        return std::nullopt;
      }
    }

    return slot;
  }
};
} // namespace Daemon
} // namespace Log
} // namespace Utils
