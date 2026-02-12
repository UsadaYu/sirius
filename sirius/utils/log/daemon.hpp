#pragma once

#include <functional>
#include <vector>

#include "utils/env.h"
#include "utils/time.hpp"

namespace Utils {
namespace Log {
namespace Daemon {
static std::string &arg() {
  static std::string arg = std::string(_SIRIUS_NAMESPACE) + "_" +
    Ns::generate_namespace_prefix(Log::kDaemonArgKey);

  return arg;
}

static inline bool is_daemon() {
  LPSTR sz_cmd_line = GetCommandLineA();
  std::string cmd_line(sz_cmd_line);
  size_t found = cmd_line.find(arg());

  return found != std::string::npos;
}

namespace Daemon {
#include "utils/log/shm.hpp"

static inline bool check() {
  if (!is_daemon()) {
    utils_dprintf(STDERR_FILENO,
                  LOG_LEVEL_STR_ERROR
                  "%sThe startup parameters for the daemon are invalid\n",
                  Log::kDaemon);
    return false;
  }

  return true;
}

class LogManager {
 public:
  LogManager() : log_shm_(std::make_unique<Shm::Shm>()) {
    fd_out_ = STDOUT_FILENO;
    fd_err_ = STDERR_FILENO;
  }

  ~LogManager() {}

  bool main() {
    bool ret = false;

    if (!Shm::GMutex::create())
      return false;
    if (!Shm::GMutex::lock())
      goto label_free1;
    if (!log_shm_->open_shm())
      goto label_free2;
    if (!log_shm_->memory_map())
      goto label_free3;
    if (!log_shm_->slots_init())
      goto label_free4;

    Shm::GMutex::unlock();

    ret = daemon_main();

    log_shm_->slots_deinit();
  label_free4:
    log_shm_->memory_unmap();
  label_free3:
    log_shm_->close_shm(ret);
  label_free2:
    Shm::GMutex::unlock();
  label_free1:
    Shm::GMutex::destroy();

    return ret;
  }

  void log_write(int level, const void *buffer, size_t size) {
    int fd = level <= SIRIUS_LOG_LEVEL_WARN ? fd_err_ : fd_err_;

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
  bool daemon_main() {
    auto header = log_shm_->header;

    utils_dprintf(STDOUT_FILENO, LOG_LEVEL_STR_INFO "%sThe daemon starts\n",
                  Log::kDaemon);

    thread_consumer_ =
      std::jthread(std::bind_front(&LogManager::thread_consumer, this));
    thread_monitor_ =
      std::jthread(std::bind_front(&LogManager::thread_monitor, this));

    header->is_daemon_ready.store(true, std::memory_order_relaxed);

    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(250));

      Shm::GMutex::lock();

      {
        log_shm_->lock_guard();

        if (header->attached_count == 0) [[unlikely]] {
          header->magic = 0;
          header->is_daemon_ready.store(false, std::memory_order_relaxed);
          break;
        }
      }

      Shm::GMutex::unlock();
    }

    thread_monitor_.request_stop();
    thread_consumer_.request_stop();

    if (thread_monitor_.joinable()) {
      thread_monitor_.join();
    }
    if (thread_consumer_.joinable()) {
      thread_consumer_.join();
    }

    utils_dprintf(STDOUT_FILENO, LOG_LEVEL_STR_INFO "%sThe daemon ended\n",
                  Log::kDaemon);

    return true;
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
        if (buffer.type == Shm::DataType::Log) [[likely]] {
          auto &data = buffer.data.log;
          log_write(buffer.level, (void *)data.buf, data.buf_size);
        } else {
          /* ----------------------------------------- */
          /* ---------------- Not Yet ---------------- */
          /* ----------------------------------------- */
        }
        slot.state.store(Shm::SlotState::FREE, std::memory_order_release);
      } else {
        if (stop_token.stop_requested()) {
          if (++exit_counter >= 5)
            break;
          utils_dprintf(STDOUT_FILENO,
                        LOG_LEVEL_STR_WARN "%s`exit_counter`: %d\n",
                        Log::kDaemon, exit_counter);
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

        if (s == Shm::SlotState::WRITING) {
          uint64_t ts = slots[i].timestamp_ms.load(std::memory_order_relaxed);
          if (now - ts > Log::kShmSlotResetTimeoutMilliseconds) {
            utils_dprintf(STDERR_FILENO,
                          LOG_LEVEL_STR_WARN "%sRecovering stuck slot: %zu\n",
                          Log::kDaemon, i);

            /**
             * @note Construct a forged log indicating data loss.
             */
            std::string es =
              std::format(LOG_RED LOG_LEVEL_STR_ERROR
                          "{}Slot recovered/skipped due to timeout\n",
                          Log::kDaemon);
            const char *err_msg = es.c_str();
            slots[i].buffer.type = Shm::DataType::Log;
            slots[i].buffer.level = SIRIUS_LOG_LEVEL_ERROR;

            auto &log = slots[i].buffer.data.log;
            log.buf_size = std::strlen(err_msg);
            std::memcpy(log.buf, err_msg, log.buf_size + 1);

            /**
             * @note Let it be `READY`, so that consumers can read it, thereby
             * advancing the index.
             */
            slots[i].state.store(Shm::SlotState::READY,
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
           Shm::SlotState::READY) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      if (++retries > retry_times) {
        utils_dprintf(STDERR_FILENO,
                      LOG_LEVEL_STR_WARN "%sSkip corrupted slot index: %" PRIu64
                                         "\n",
                      Log::kDaemon, read_index);
        return std::nullopt;
      }
    }

    return slot;
  }
};
} // namespace Daemon

namespace Exe {
static inline std::filesystem::path env_path() {
  std::string env = Env::get_env(SIRIUS_ENV_EXE_DAEMON_PATH);

  auto path = std::filesystem::path(env);

  return (!path.empty() && std::filesystem::exists(path)) ? path : "";
}

static inline std::filesystem::path default_path() {
  return File::get_exe_path_matrix(_SIRIUS_EXE_DAEMON_NAME, _SIRIUS_EXE_DIR);
}

#ifdef _SIRIUS_WIN_DLL
static inline std::filesystem::path current_dll_dir() {
  std::string es;
  HMODULE modele = nullptr;
  wchar_t path[MAX_PATH];

  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          (LPCWSTR)&current_dll_dir, &modele)) {
    const DWORD dw_err = GetLastError();
    es = std::format("{} -> `GetModuleHandleExW`", utils_pretty_function);
    utils_win_last_error(dw_err, es.c_str());
    return "";
  }

  DWORD length = GetModuleFileNameW(modele, path, MAX_PATH);
  if (length == 0) {
    const DWORD dw_err = GetLastError();
    es = std::format("{} -> `GetModuleFileNameW`", utils_pretty_function);
    utils_win_last_error(dw_err, es.c_str());
    return "";
  }

  auto bin_dir = std::filesystem::path(path).parent_path();

  return bin_dir;
}
#endif

inline std::filesystem::path daemon_exe_path = "";
static inline int set_path(std::filesystem::path path) {
  if (path.empty()) {
    utils_errno_error(EINVAL, utils_pretty_function);
    errno = EINVAL;
    return errno;
  }

  if (!std::filesystem::exists(path)) {
    utils_errno_error(ENOENT, utils_pretty_function);
    errno = ENOENT;
    return errno;
  }

  daemon_exe_path = path;

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
static inline std::filesystem::path path() {
  std::vector<std::filesystem::path> paths = {
    daemon_exe_path,
    env_path(),
#ifdef _SIRIUS_WIN_DLL
    File::get_exe_path_matrix(_SIRIUS_EXE_DAEMON_NAME, current_dll_dir()),
#endif
    default_path(),
  };

  for (auto path : paths) {
    if (!path.empty()) {
      utils_dprintf(STDOUT_FILENO,
                    LOG_LEVEL_STR_INFO "%sThe daemon executable file: `%s`\n",
                    Log::kCommon, path.string().c_str());
      return path;
    }
  }

  utils_dprintf(STDERR_FILENO,
                LOG_LEVEL_STR_ERROR
                "%sFail to find the daemon executable file\n",
                Log::kCommon);

  return "";
}
} // namespace Exe
} // namespace Daemon
} // namespace Log
} // namespace Utils
