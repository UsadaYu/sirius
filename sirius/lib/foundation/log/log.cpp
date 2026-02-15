/**
 * @note Exception handling is required when the daemon is killed or crashed,
 * which will be added later.
 */

#include "lib/foundation/log/log.h"

#include <mutex>
#include <thread>
#include <vector>

#include "lib/foundation/initializer.h"
#include "sirius/foundation/thread.h"
#include "utils/log/daemon.hpp"
#include "utils/time.hpp"

namespace StdType {
enum class Put : int {
  kIn = 0,
  kOut = 1,
  kErr = 2,
};

static inline bool check(Utils::Utils::IntegralOrEnum auto type) {
  int index = static_cast<int>(type);

  if (index < static_cast<int>(Put::kIn) || index > static_cast<int>(Put::kErr))
    [[unlikely]] {
    return false;
  }

  return true;
}
} // namespace StdType

class FsManager {
 public:
  FsManager() : fd_out_(STDOUT_FILENO), fd_err_(STDERR_FILENO) {}

  ~FsManager() {}

  void log_write(int level, const void *buffer, size_t size) {
    std::lock_guard lock(mutex_);

    int fd = level <= SIRIUS_LOG_LEVEL_WARN ? fd_err_ : fd_out_;

    UTILS_WRITE(fd, buffer, size);
  }

  void fs_configure(const char *log_path, size_t path_size,
                    StdType::Put put_type) {
    if (path_size != 0) {
      (void)log_path;
      /* ----------------------------------------- */
      /* ---------------- Not Yet ---------------- */
      /* ----------------------------------------- */
    } else {
      std::lock_guard lock(mutex_);

      if (put_type == StdType::Put::kOut) {
        fd_out_ = STDOUT_FILENO;
      } else {
        fd_err_ = STDERR_FILENO;
      }
    }
  }

 private:
  std::mutex mutex_;
  int fd_out_;
  int fd_err_;
};

static FsManager g_fs_manager;

class LogManager {
 public:
  LogManager()
      : is_activated_(false),
        daemon_arg_(Utils::Log::Daemon::arg()),
        log_shm_(std::make_unique<Utils::Log::Shm::Shm>(
          Utils::Log::Shm::MasterType::kNative)) {}

  ~LogManager() {
    deinit();
  }

  bool init() {
    if (!Utils::Log::Shm::GMutex::create())
      return false;
    if (!Utils::Log::Shm::GMutex::lock())
      goto label_free1;
    if (!log_shm_->open_shm())
      goto label_free2;
    if (!log_shm_->memory_map())
      goto label_free3;
    if (!log_shm_->slots_init())
      goto label_free4;

    Utils::Log::Shm::GMutex::unlock();

    if (log_shm_->is_shm_creator() && !start_daemon())
      goto label_free5;
    if (!wait_for_daemon())
      goto label_free5;

    is_activated_.store(true, std::memory_order_relaxed);

    return true;

  label_free5:
    Utils::Log::Shm::GMutex::lock();
    log_shm_->slots_deinit();
  label_free4:
    log_shm_->memory_unmap();
  label_free3:
    log_shm_->close_shm(log_shm_->is_shm_creator());
  label_free2:
    Utils::Log::Shm::GMutex::unlock();
  label_free1:
    Utils::Log::Shm::GMutex::destroy();

    return false;
  }

  void deinit() {
    if (!is_activated_.exchange(false, std::memory_order_relaxed))
      return;

    Utils::Log::Shm::GMutex::lock();

    log_shm_->slots_deinit();
    log_shm_->memory_unmap();
    log_shm_->close_shm(false);

    Utils::Log::Shm::GMutex::unlock();

    Utils::Log::Shm::GMutex::destroy();
  }

  /**
   * @note In case of an emergency and when it is not possible to write data to
   * the shared memory, the operation may revert to writing data "natively".
   * At this point, it is necessary to ensure that the "native" file descriptor
   * is valid.
   */
  void fs_configure(const sirius_log_fs_t &config, StdType::Put put_type) {
    if (!StdType::check(put_type)) [[unlikely]]
      return;

    size_t path_size = 0;

    if (config.log_path) {
      path_size =
        utils_string_length_check(config.log_path, Utils::Log::kLogPathMax);
      if (path_size == 0 || path_size == Utils::Log::kLogPathMax) {
        utils_dprintf(STDERR_FILENO,
                      LOG_LEVEL_STR_WARN "%sInvalid argument. `log_path`\n",
                      Utils::Log::kNative);
        return;
      }
    }

    if (config.shared == SiriusThreadProcess::kSiriusThreadProcessPrivate) {
      g_fs_manager.fs_configure(config.log_path, path_size, put_type);
    } else {
      fs_configure_shared(config.log_path, path_size, put_type);
    }
  }

  void produce_shared(void *src, size_t size) {
    if (produce_fallback(src)) [[unlikely]]
      return;

    uint64_t cur_write_idx =
      log_shm_->header->write_index.fetch_add(1, std::memory_order_acq_rel);
    size_t slot_idx = cur_write_idx & (Utils::Log::kShmCapacity - 1);
    Utils::Log::Shm::Slot &slot = log_shm_->slots[slot_idx];

    int retries = 0;
    while (slot.state.load(std::memory_order_acquire) !=
           Utils::Log::Shm::SlotState::kFree) {
      ++retries;
      if (retries % 10 == 0) {
        std::this_thread::yield();
      } else if (retries > 50) {
        retries = 0;
        std::this_thread::sleep_for(std::chrono::nanoseconds(100));
      }
    }

    slot.state.store(Utils::Log::Shm::SlotState::kWaiting,
                     std::memory_order_release);

    slot.timestamp_ms.store(Utils::Time::get_monotonic_steady_ms(),
                            std::memory_order_relaxed);

    std::memcpy(&slot.buffer, src, size);

    slot.state.store(Utils::Log::Shm::SlotState::kReady,
                     std::memory_order_release);
  }

  bool shared_valid() const {
    return log_shm_->header->is_daemon_ready.load(std::memory_order_relaxed) &&
      is_activated_.load(std::memory_order_relaxed);
  }

 private:
  std::atomic<bool> is_activated_;
  std::string daemon_arg_;
  std::unique_ptr<Utils::Log::Shm::Shm> log_shm_;

#if defined(_WIN32) || defined(_WIN64)
  bool start_daemon() {
    std::string es;

    auto daemon_exe = Utils::Log::Daemon::Exe::path();
    if (daemon_exe.empty())
      return false;

    std::string cmd_line = Utils::Log::Daemon::arg();

    STARTUPINFOA si {};
    si.cb = sizeof(STARTUPINFOA);
    PROCESS_INFORMATION pi {};

    BOOL ret =
      CreateProcessA(daemon_exe.string().c_str(), cmd_line.data(), nullptr,
                     nullptr, TRUE, 0, nullptr, nullptr, &si, &pi);
    if (!ret) {
      const DWORD dw_err = GetLastError();
      es = std::format("{}{} -> `CreateProcessA`", Utils::Log::kNative,
                       utils_pretty_function);
      utils_win_last_error(dw_err, es.c_str());
      return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    utils_dprintf(STDOUT_FILENO,
                  LOG_LEVEL_STR_INFO "%sTry to start the daemon\n",
                  Utils::Log::kNative);

    return true;
  }
#else
  bool start_daemon() {
    // --- fork ---
    utils_dprintf(STDERR_FILENO, "Not Yet\n");

    return false;

    return true;
  }
#endif

  bool wait_for_daemon() {
    auto header = log_shm_->header;

    int retries = 0;
    constexpr int kRetryTimes = 400;
    while (!header->is_daemon_ready.load(std::memory_order_relaxed)) {
      if (retries++ > kRetryTimes) {
        utils_dprintf(STDERR_FILENO,
                      LOG_LEVEL_STR_ERROR "%sNo daemon was found\n",
                      Utils::Log::kNative);
        return false;
      }
      if (retries % 100 == 0) {
        utils_dprintf(STDOUT_FILENO,
                      LOG_LEVEL_STR_INFO
                      "%sTrying to acquire daemon. Total attempts: %d; "
                      "Attempted times: %d\n",
                      Utils::Log::kNative, kRetryTimes, retries);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return true;
  }

  void fs_configure_shared(const char *log_path, size_t path_size,
                           StdType::Put put_type) {
    Utils::Log::Shm::Buffer buffer {};
    auto &data = buffer.data.fs;

    buffer.type = Utils::Log::Shm::DataType::kConfig;
    buffer.level = put_type == StdType::Put::kOut ? SIRIUS_LOG_LEVEL_DEBUG
                                                  : SIRIUS_LOG_LEVEL_ERROR;

    if (path_size) {
      std::memcpy(data.path, log_path, path_size);
      data.path[path_size] = '\0';
      path_size += 1;
      data.state = Utils::Log::Shm::FsState::kFile;
    } else {
      data.state = Utils::Log::Shm::FsState::kStd;
    }

    produce_shared(
      &buffer,
      sizeof(Utils::Log::Shm::Buffer) - Utils::Log::kLogPathMax + path_size);
  }

  bool produce_fallback(void *src) {
    if (log_shm_->header->is_daemon_ready.load(std::memory_order_relaxed) &&
        is_activated_.load(std::memory_order_relaxed)) [[likely]] {
      return false;
    }

    auto buffer = (Utils::Log::Shm::Buffer *)src;

    if (buffer->type == Utils::Log::Shm::DataType::kLog) [[likely]] {
      g_fs_manager.log_write(buffer->level, (void *)buffer->data.log.buf,
                             buffer->data.log.buf_size);
    }

    return true;
  }
};

// --- Global Instance & Hooks ---

#if defined(_WIN32) || defined(_WIN64)
static inline void win_enable_ansi() {
#  if defined(NTDDI_VERSION) && (NTDDI_VERSION >= 0x0A000002)
  HANDLE h_out = GetStdHandle(STD_OUTPUT_HANDLE);
  HANDLE h_err = GetStdHandle(STD_ERROR_HANDLE);
  if (!h_out || h_out == INVALID_HANDLE_VALUE || !h_err ||
      h_err == INVALID_HANDLE_VALUE) {
    return;
  }

  DWORD mode = 0;
  if (GetConsoleMode(h_out, &mode)) {
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(h_out, mode);
  }
  if (GetConsoleMode(h_err, &mode)) {
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(h_err, mode);
  }
#  endif
}
#endif

extern "C" bool constructor_foundation_log() {
#if defined(_WIN32) || defined(_WIN64)
  win_enable_ansi();
#endif

  return true;
}

extern "C" void destructor_foundation_log() {}

// --- API ---
static bool g_api_initialization = false;
static std::mutex g_api_mutex;
static std::unique_ptr<LogManager> g_log_manager;
static uint64_t g_try_times = 0;
static uint64_t g_try_timestamp_ms = 0;
static constexpr uint64_t kRetryTimeIntervalMilliseconds = 15 * 1000;

static void g_log_manager_deinit(void) {
  g_log_manager->deinit();
}

static force_inline bool initialization_check() {
  if (g_api_initialization)
    return true;

  std::lock_guard lock(g_api_mutex);

  if (g_try_times > 1) {
    if (Utils::Time::get_monotonic_steady_ms() - g_try_timestamp_ms <
        kRetryTimeIntervalMilliseconds) {
      return false;
    }
    g_try_times = 0;
  }

  g_try_times += 1;
  g_try_timestamp_ms = Utils::Time::get_monotonic_steady_ms();

  if (!g_api_initialization) {
    auto log_manager = std::make_unique<LogManager>();

    g_api_initialization = log_manager->init();

    if (g_api_initialization) {
      g_log_manager = std::move(log_manager);
      std::atexit(g_log_manager_deinit);
    }
  }

  return g_api_initialization;
}

#if defined(_WIN32) || defined(_WIN64)
extern "C" sirius_api int sirius_log_set_daemon_path(const char *path) {
  if (g_api_initialization) {
    sirius_error("The Daemon has started, it is too late to configure\n");
    return EBUSY;
  }

  return Utils::Log::Daemon::Exe::set_path(path);
}
#else
extern "C" sirius_api int sirius_log_set_daemon_path(const char *path) {
  return 0;
}
#endif

extern "C" sirius_api void
sirius_log_configure(const sirius_log_config_t *config) {
  if (!initialization_check() || !config)
    return;

  g_log_manager->fs_configure(config->out, StdType::Put::kOut);
  g_log_manager->fs_configure(config->err, StdType::Put::kOut);
}

#define ISSUE_ERROR_STANDARD_FUNCTION(function) \
  do { \
    const char *e = \
      "\n" \
      "  The LOG module issues an ERROR\n" \
      "  " function " error\n"; \
    g_fs_manager.log_write(SIRIUS_LOG_LEVEL_ERROR, e, strlen(e)); \
  } while (0)

#define ISSUE_ERROR_LOG_LOST() \
  do { \
    const char *e = \
      "\n" \
      "  The LOG module issues an ERROR\n" \
      "  Log length exceeds the buffer, log will be lost\n"; \
    g_fs_manager.log_write(SIRIUS_LOG_LEVEL_ERROR, e, strlen(e)); \
  } while (0)

#define ISSUE_WARNING_LOG_TRUNCATED() \
  do { \
    const char *e = \
      "\n" \
      "  The LOG module issues a WARNING\n" \
      "  Log length exceeds the buffer, log will be truncated\n"; \
    g_fs_manager.log_write(SIRIUS_LOG_LEVEL_ERROR, e, strlen(e)); \
  } while (0)

static force_inline void log_write(Utils::Log::Shm::Buffer &buffer,
                                   size_t data_len) {
  if (initialization_check()) {
    g_log_manager->produce_shared(
      &buffer,
      sizeof(Utils::Log::Shm::Buffer) - Utils::Log::kLogBufferSize + data_len);
  } else {
    g_fs_manager.log_write(buffer.level, (void *)buffer.data.log.buf,
                           buffer.data.log.buf_size);
  }
}

extern "C" sirius_api void sirius_log_impl(int level, const char *level_str,
                                           const char *color,
                                           const char *module, const char *file,
                                           const char *func, int line,
                                           const char *fmt, ...) {
  Utils::Log::Shm::Buffer buffer {};
  auto &data = buffer.data.log;
  size_t len = 0;

  buffer.type = Utils::Log::Shm::DataType::kLog;
  buffer.level = level;

  time_t raw_time;
  struct tm tm_info;
  time(&raw_time);
  UTILS_LOCALTIME_R(&raw_time, &tm_info);

  /**
   * @ref https://linux.die.net/man/3/snprintf
   */
  int written =
    snprintf(data.buf, Utils::Log::kLogBufferSize,
             "%s%s [%02d:%02d:%02d %s %" PRIu64 " %s (%s|%d)]%s ", color,
             level_str, tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, module,
             sirius_thread_id(), file, func, line, LOG_COLOR_NONE);

  if (written < 0) {
    ISSUE_ERROR_STANDARD_FUNCTION("snprintf");
    return;
  }

  len += written;
  if (len >= Utils::Log::kLogBufferSize) [[unlikely]] {
    ISSUE_ERROR_LOG_LOST();
    return;
  }

  /**
   * @ref https://linux.die.net/man/3/vsnprintf
   */
  va_list args;
  va_start(args, fmt);
  written =
    vsnprintf(data.buf + len, Utils::Log::kLogBufferSize - len, fmt, args);
  va_end(args);

  if (written < 0) {
    ISSUE_ERROR_STANDARD_FUNCTION("vsnprintf");
    return;
  }

  len += written;
  if (len >= Utils::Log::kLogBufferSize) [[unlikely]] {
    len = Utils::Log::kLogBufferSize - 1;
    ISSUE_WARNING_LOG_TRUNCATED();
  }

  data.buf_size = len;

  log_write(buffer, len);
}

extern "C" sirius_api void sirius_logsp_impl(int level, const char *level_str,
                                             const char *color,
                                             const char *module,
                                             const char *fmt, ...) {
  Utils::Log::Shm::Buffer buffer {};
  auto &data = buffer.data.log;
  size_t len = 0;

  buffer.type = Utils::Log::Shm::DataType::kLog;
  buffer.level = level;

  time_t raw_time;
  struct tm tm_info;
  time(&raw_time);
  UTILS_LOCALTIME_R(&raw_time, &tm_info);

  int written =
    snprintf(data.buf, Utils::Log::kLogBufferSize,
             "%s%s [%02d:%02d:%02d %s]%s ", color, level_str, tm_info.tm_hour,
             tm_info.tm_min, tm_info.tm_sec, module, LOG_COLOR_NONE);

  if (written < 0) {
    ISSUE_ERROR_STANDARD_FUNCTION("snprintf");
    return;
  }

  len += written;
  if (len >= Utils::Log::kLogBufferSize) [[unlikely]] {
    ISSUE_ERROR_LOG_LOST();
    return;
  }

  va_list args;
  va_start(args, fmt);
  written =
    vsnprintf(data.buf + len, Utils::Log::kLogBufferSize - len, fmt, args);
  va_end(args);

  if (written < 0) {
    ISSUE_ERROR_STANDARD_FUNCTION("vsnprintf");
    return;
  }

  len += written;
  if (len >= Utils::Log::kLogBufferSize) [[unlikely]] {
    len = Utils::Log::kLogBufferSize - 1;
    ISSUE_WARNING_LOG_TRUNCATED();
  }

  data.buf_size = len;

  log_write(buffer, len);
}
