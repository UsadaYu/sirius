#pragma once

#include <vector>

#include "utils/env.h"
#include "utils/time.hpp"

namespace Daemon {
static std::string &arg() {
  static std::string arg = std::string(sirius_namespace) + "_" +
    Utils::Ns::generate_namespace_prefix(Utils::Log::DAEMON_ARG_KEY);

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
    UTILS_DPRINTF(STDERR_FILENO,
                  log_level_str_error
                  "%sThe startup parameters for the daemon are invalid\n",
                  Utils::Log::DAEMON);
    return false;
  }

  return true;
}

class LogManager {
 public:
  LogManager() : log_shm_(std::make_unique<LogShm>()) {}

  ~LogManager() {}

  bool main() {
    bool ret = false;

    if (!ProcessMutex::create())
      return false;
    if (!ProcessMutex::lock())
      goto label_free1;
    if (!log_shm_->open_shm())
      goto label_free2;
    if (!log_shm_->memory_map())
      goto label_free3;
    if (!log_shm_->slots_init())
      goto label_free4;

    ProcessMutex::unlock();

    daemon_main();
    ret = true;

    log_shm_->slots_deinit();
  label_free4:
    log_shm_->memory_unmap();
  label_free3:
    log_shm_->close_shm(ret);
  label_free2:
    ProcessMutex::unlock();
  label_free1:
    ProcessMutex::destory();

    return ret;
  }

  void log_write(int level, const void *buffer, size_t size) {
    auto header = log_shm_->header;

    int fd = level <= sirius_log_level_warn
      ? header->fd_stderr.load(std::memory_order_relaxed)
      : header->fd_stdout.load(std::memory_order_relaxed);

    UTILS_WRITE(fd, buffer, size);
  }

 private:
  std::unique_ptr<LogShm> log_shm_;

  std::thread thread_consumer_;
  std::thread thread_monitor_;
  std::atomic<bool> thread_running_;

  /**
   * @note After this function ends, the process lock will be held.
   */
  bool daemon_main() {
    auto header = log_shm_->header;

    UTILS_DPRINTF(STDOUT_FILENO, log_level_str_info "%sThe daemon starts\n",
                  Utils::Log::DAEMON);

    thread_running_.store(true, std::memory_order_relaxed);
    thread_consumer_ = std::thread([this]() {
      this->thread_consumer();
    });
    thread_monitor_ = std::thread([this]() {
      this->thread_monitor();
    });

    header->is_daemon_ready.store(true, std::memory_order_relaxed);

    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      ProcessMutex::lock();
      log_shm_->shm_lock_lock();

      if (header->attached_count == 0) [[unlikely]] {
        header->magic = 0;
        header->is_daemon_ready.store(false, std::memory_order_relaxed);
        log_shm_->shm_lock_unlock();
        break;
      }

      log_shm_->shm_lock_unlock();
      ProcessMutex::unlock();
    }

    thread_running_.store(false, std::memory_order_relaxed);
    if (thread_monitor_.joinable()) {
      thread_monitor_.join();
    }
    if (thread_consumer_.joinable()) {
      thread_consumer_.join();
    }

    UTILS_DPRINTF(STDOUT_FILENO, log_level_str_info "%sThe daemon ended\n",
                  Utils::Log::DAEMON);

    return true;
  }

  void thread_consumer() {
    auto header = log_shm_->header;

    while (true) {
      uint64_t read_index = header->read_index.load(std::memory_order_acquire);

      if (read_index >= header->write_index.load(std::memory_order_acquire)) {
        if (!thread_running_.load(std::memory_order_relaxed))
          break;
        std::this_thread::sleep_for(std::chrono::microseconds(200));
        continue;
      }

      auto opt_slot = get_slot(read_index, 50);
      if (opt_slot) {
        ShmSlot &slot = opt_slot->get();
        LogBuffer &buffer = slot.buffer;
        log_write(buffer.level, (void *)buffer.data, buffer.data_size);
        slot.state.store(SlotState::FREE, std::memory_order_release);
      }

      header->read_index.fetch_add(1, std::memory_order_release);
    }
  }

  void thread_monitor() {
    auto header = log_shm_->header;
    auto slots = log_shm_->slots;

    while (thread_running_.load(std::memory_order_relaxed)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      uint64_t now = Utils::Time::get_monotonic_steady_ms();

      for (size_t i = 0; i < header->capacity; ++i) {
        SlotState s = slots[i].state.load(std::memory_order_acquire);

        if (s == SlotState::WRITING) {
          uint64_t ts = slots[i].timestamp_ms.load(std::memory_order_relaxed);
          if (now - ts > Utils::Log::SHM_SLOT_RESET_TIMEOUT_MS) {
            UTILS_DPRINTF(STDERR_FILENO,
                          log_level_str_warn "%sRecovering stuck slot: %zu\n",
                          Utils::Log::DAEMON, i);

            /**
             * @note Construct a forged log indicating data loss.
             */
            std::string es =
              std::format(log_red log_level_str_error
                          "{}Slot recovered/skipped due to timeout\n",
                          Utils::Log::DAEMON);
            const char *err_msg = es.c_str();
            std::memcpy(slots[i].buffer.data, err_msg, strlen(err_msg));
            slots[i].buffer.data_size = std::strlen(err_msg);
            slots[i].buffer.level = sirius_log_level_error;

            /**
             * @note Let it be `READY`, so that consumers can read it, thereby
             * advancing the index.
             */
            slots[i].state.store(SlotState::READY, std::memory_order_release);
          }
        }
      }
    }
  }

  [[nodiscard]] std::optional<std::reference_wrapper<ShmSlot>>
  get_slot(const uint64_t read_index, const int retry_times) {
    size_t slot_idx = read_index & (Utils::Log::SHM_CAPACITY - 1);
    ShmSlot &slot = log_shm_->slots[slot_idx];

    int retries = 0;
    while (slot.state.load(std::memory_order_acquire) != SlotState::READY) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      if (++retries > retry_times) {
        UTILS_DPRINTF(STDERR_FILENO,
                      log_level_str_warn "%sSkip corrupted slot index: %" PRIu64
                                         "\n",
                      Utils::Log::DAEMON, read_index);
        return std::nullopt;
      }
    }

    return slot;
  }
};
} // namespace Daemon

namespace Exe {
static inline std::filesystem::path env_path() {
  std::string env = Utils::Env::get_env(SIRIUS_ENV_EXE_DAEMON_PATH);

  auto path = std::filesystem::path(env);

  return (!path.empty() && std::filesystem::exists(path)) ? path : "";
}

static inline std::filesystem::path default_path() {
  return Utils::File::get_exe_path_matrix(sirius_exe_daemon_name,
                                          sirius_exe_dir);
}

#ifdef _SIRIUS_WIN_DLL
static inline std::filesystem::path current_dll_dir() {
  HMODULE modele = nullptr;
  wchar_t path[MAX_PATH];

  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          (LPCWSTR)&current_dll_dir, &modele)) {
    Utils::Log::win_last_error("GetModuleHandleExW");
    return "";
  }

  DWORD length = GetModuleFileNameW(modele, path, MAX_PATH);
  if (length == 0) {
    Utils::Log::win_last_error("GetModuleFileNameW");
    return "";
  }

  auto bin_dir = std::filesystem::path(path).parent_path();

  return bin_dir;
}
#endif

inline std::filesystem::path daemon_exe_path = "";
static inline int set_path(std::filesystem::path path) {
  if (path.empty()) {
    errno = EINVAL;
    Utils::Log::errno_error(internal_pretty_function);
    return errno;
  }

  if (!std::filesystem::exists(path)) {
    errno = ENOENT;
    Utils::Log::errno_error(internal_pretty_function);
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
    Utils::File::get_exe_path_matrix(sirius_exe_daemon_name, current_dll_dir()),
#endif
    default_path(),
  };

  for (auto path : paths) {
    if (!path.empty()) {
      UTILS_DPRINTF(STDOUT_FILENO,
                    log_level_str_info
                    "%sThe path of the daemon executable file: `%s`\n",
                    Utils::Log::COMMON, path.string().c_str());
      return path;
    }
  }

  UTILS_DPRINTF(STDERR_FILENO,
                log_level_str_error
                "%sFail to find the daemon executable file\n",
                Utils::Log::COMMON);

  return "";
}
} // namespace Exe
} // namespace Daemon
