#pragma once

#include <array>
#include <functional>
#include <span>
#include <vector>

#include "utils/args.hpp"
#include "utils/env.h"
#include "utils/log/shm.hpp"
#include "utils/process/sys.hpp"
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

  static std::vector<std::string> &cmds_daemon() {
    static std::vector<std::string> cmd = {std::string("--").append(kArgDaemon),
                                           kArgDaemonSpawn};

    return cmd;
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
        io_outln(IO_ISP("\nThe daemon executable file: {0}", path.string()));
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

class Daemon {
 public:
  Daemon()
      : should_leave_(
          Process::check_init_type() != Process::InitType::kUnreliableInit &&
          Process::check_init_type() != Process::InitType::kUnknown),
        log_shm_(Shm::SharedMem::instance()),
        slots_(nullptr),
        fd_out_(STDOUT_FILENO),
        fd_err_(STDERR_FILENO) {}

  ~Daemon() = default;

  auto main() -> std::expected<void, std::string> {
    bool main_ret = true;
    std::string ret_msg;

    if (auto ret = Shm::GMutex::create(); !ret.has_value())
      return ret;
    if (auto ret = Shm::GMutex::lock(); !ret.has_value()) {
      ret_msg = ret.error();
      goto label_free1;
    }
    if (auto ret = log_shm_.shm_alloc(MasterType::kDaemon); !ret.has_value()) {
      ret_msg = ret.error();
      goto label_free2;
    } else {
      master_ = std::move(ret.value());
    }

    for (int i = 0; i < 2; ++i) {
      auto ret = master_->slots_alloc();
      if (ret.has_value()) {
        slots_ = ret.value();
        break;
      }
      if (i == 1 || master_->daemon_alive_or_reset()) {
        ret_msg = ret.error();
        goto label_free3;
      }
    }

    (void)Shm::GMutex::unlock();

    if (auto ret = daemon_main(); !ret.has_value()) {
      ret_msg = ret.error();
      main_ret = false;
    }

    master_->slots_free();
  label_free3:
    master_.reset();
    log_shm_.shm_free();
  label_free2:
    (void)Shm::GMutex::unlock();
  label_free1:
    Shm::GMutex::destroy();

    return main_ret ? std::expected<void, std::string> {}
                    : std::unexpected(ret_msg);
  }

  void log_write(int level, const void *buffer, size_t size) {
    int fd = level <= SIRIUS_LOG_LEVEL_WARN ? fd_err_ : fd_out_;

    utils_write(fd, buffer, size);
  }

 private:
  bool should_leave_;
  Shm::SharedMem &log_shm_;
  std::unique_ptr<Shm::SharedMem::Master> master_;
  Shm::Slot *slots_;
  int fd_out_;
  int fd_err_;

  std::jthread thread_consumer_;
  std::jthread thread_monitor_;

  /**
   * @note After this function ends, the `Shm::GMutex` will be held.
   */
  auto daemon_main() -> std::expected<void, std::string> {
    auto header = master_->get_shm_header();

    io_outln(IO_ISP("The daemon starts (PID: {0})", Process::pid()));

    thread_consumer_ =
      std::jthread(std::bind_front(&Daemon::thread_consumer, this));
    thread_monitor_ =
      std::jthread(std::bind_front(&Daemon::thread_monitor, this));

    header->is_daemon_ready.store(true, std::memory_order_relaxed);

    uint64_t sleep_ms = 500;
    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

      (void)Shm::GMutex::lock();

      {
        master_->lock_guard();

        if (header->attached_count <= Log::kProcessNbDaemon) {
          if (should_leave_) {
            header->magic = 0;
            header->is_daemon_ready.store(false, std::memory_order_relaxed);
            break;
          } else {
            sleep_ms = 2500;
          }
        } else {
          sleep_ms = 500;
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

    io_outln(IO_ISP("The daemon ended (PID: {0})", Process::pid()));

    return {};
  }

  void thread_consumer(std::stop_token stop_token) {
    int exit_counter = 0;
    auto header = master_->get_shm_header();

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
    auto header = master_->get_shm_header();

    while (!stop_token.stop_requested()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(300));

      uint64_t now = Time::get_monotonic_steady_ms();

      for (size_t i = 0; i < header->capacity; ++i) {
        Shm::SlotState s = slots_[i].state.load(std::memory_order_acquire);

        if (s == Shm::SlotState::kWaiting) {
          uint64_t ts = slots_[i].timestamp_ms.load(std::memory_order_relaxed);
          if (now - ts > Log::kShmSlotResetTimeoutMilliseconds) {
            io_errln(IO_WSP("Recovering stuck slot: {0}", i));

            /**
             * @note Construct a forged log indicating data loss.
             */
            auto es =
              IO_WSP("Slot recovered/skipped due to timeout").append("\n");
            slots_[i].buffer.type = Shm::DataType::kLog;
            slots_[i].buffer.level = SIRIUS_LOG_LEVEL_ERROR;

            auto &log = slots_[i].buffer.data.log;
            log.buf_size = es.size();
            std::memcpy(log.buf, es.c_str(), log.buf_size + 1);

            /**
             * @note Let it be `kReady`, so that consumers can read it, thereby
             * advancing the index.
             */
            slots_[i].state.store(Shm::SlotState::kReady,
                                  std::memory_order_release);
          }
        }
      }
    }
  }

  [[nodiscard]] std::optional<std::reference_wrapper<Shm::Slot>>
  get_slot(const uint64_t read_index, const int retry_times) {
    size_t slot_idx = read_index & (Log::kShmCapacity - 1);
    // Shm::Slot &slot = log_shm_->slots[slot_idx];
    Shm::Slot &slot = slots_[slot_idx];

    int retries = 0;
    while (slot.state.load(std::memory_order_acquire) !=
           Shm::SlotState::kReady) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      if (++retries > retry_times) {
        io_errln(IO_WSP("Skip corrupted slot index: {0}", read_index));
        return std::nullopt;
      }
    }

    return slot;
  }
};
} // namespace Exe
} // namespace Log
} // namespace Utils
