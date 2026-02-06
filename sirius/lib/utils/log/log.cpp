/**
 * @note Exception handling is required when the daemon is killed or crashed,
 * which will be added later.
 */

#include "lib/utils/log/log.h"

#include <mutex>
#include <thread>
#include <vector>

#include "lib/utils/initializer.h"
#include "sirius/utils/thread.h"
#include "utils/time.hpp"

// clang-format off
#ifdef UTILS_LOG_SHM_HPP_IS_DAEMON_
#  undef UTILS_LOG_SHM_HPP_IS_DAEMON_
#endif
#define UTILS_LOG_SHM_HPP_IS_DAEMON_ 0
#include "utils/log/shm.hpp"
#include "utils/log/daemon.hpp"
// clang-format on

namespace StdType {
enum class Put : int {
  In = 0,
  Out = 1,
  Err = 2,
};

static inline bool check(Utils::Utils::IntegralOrEnum auto type) {
  int index = static_cast<int>(type);

  if (index < static_cast<int>(Put::In) || index > static_cast<int>(Put::Err))
    [[unlikely]] {
    return false;
  }

  return true;
}
} // namespace StdType

namespace UL = Utils::Log;

class LogManager {
 public:
  LogManager()
      : is_activated_(false),
        is_creator_(false),
        daemon_arg_(UL::Daemon::arg()),
        log_shm_(std::make_unique<UL::Shm::Shm>()) {
    fd_out_ = STDOUT_FILENO;
    fd_err_ = STDERR_FILENO;
  }

  ~LogManager() {}

  bool init() {
    if (!UL::Shm::ProcessMutex::create())
      return false;
    if (!UL::Shm::ProcessMutex::lock())
      goto label_free1;
    if (!log_shm_->open_shm())
      goto label_free2;
    is_creator_ = log_shm_->shm_creator;
    if (!log_shm_->memory_map())
      goto label_free3;
    if (!log_shm_->slots_init())
      goto label_free4;

    UL::Shm::ProcessMutex::unlock();

    if (is_creator_ && !start_daemon())
      goto label_free5;
    if (!wait_for_daemon())
      goto label_free5;

    is_activated_ = true;

    return true;

  label_free5:
    UL::Shm::ProcessMutex::lock();
    log_shm_->slots_deinit();
  label_free4:
    log_shm_->memory_unmap();
  label_free3:
    log_shm_->close_shm(is_creator_);
  label_free2:
    UL::Shm::ProcessMutex::unlock();
  label_free1:
    UL::Shm::ProcessMutex::destory();

    return false;
  }

  void deinit() {
    if (!is_activated_)
      return;

    is_activated_ = false;

    UL::Shm::ProcessMutex::lock();

    log_shm_->slots_deinit();
    log_shm_->memory_unmap();
    log_shm_->close_shm(false);

    UL::Shm::ProcessMutex::unlock();

    UL::Shm::ProcessMutex::destory();
  }

  void fs_configure(const sirius_log_fs_t &config, StdType::Put put_type) {
    if (!StdType::check(put_type))
      return;

    size_t path_size = 0;

    if (config.log_path) {
      path_size =
        Utils::File::path_length_check(config.log_path, UL::LOG_PATH_MAX - 1);
      if (path_size == 0)
        return;
    }

    if (config.shared == SiriusThreadProcess::kSiriusThreadProcessPrivate) {
      fs_configure_private(config.log_path, path_size, put_type);
    } else {
      fs_configure_shared(config.log_path, path_size, put_type);
    }
  }

  void produce(void *src, size_t size) {
    if (produce_fallback(src)) [[unlikely]]
      return;

    uint64_t cur_write_idx =
      log_shm_->header->write_index.fetch_add(1, std::memory_order_acq_rel);
    size_t slot_idx = cur_write_idx & (UL::SHM_CAPACITY - 1);
    UL::Shm::Slot &slot = log_shm_->slots[slot_idx];

    int retries = 0;
    while (slot.state.load(std::memory_order_acquire) !=
           UL::Shm::SlotState::FREE) {
      ++retries;
      if (retries % 10 == 0) {
        std::this_thread::yield();
      } else if (retries > 50) {
        retries = 0;
        std::this_thread::sleep_for(std::chrono::nanoseconds(100));
      }
    }

    slot.timestamp_ms.store(Utils::Time::get_monotonic_steady_ms(),
                            std::memory_order_relaxed);
    slot.state.store(UL::Shm::SlotState::WRITING, std::memory_order_release);

    std::memcpy(&slot.buffer, src, size);

    slot.state.store(UL::Shm::SlotState::READY, std::memory_order_release);
  }

  void log_write(int level, const void *buffer, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);

    int fd = level <= SIRIUS_LOG_LEVEL_WARN ? fd_err_ : fd_out_;

    UTILS_WRITE(fd, buffer, size);
  }

 private:
  bool is_activated_;
  bool is_creator_;
  std::string daemon_arg_;
  std::unique_ptr<UL::Shm::Shm> log_shm_;

  std::mutex mutex_;
  int fd_out_;
  int fd_err_;

#if defined(_WIN32) || defined(_WIN64)
  bool start_daemon() {
    auto daemon_exe = UL::Daemon::Exe::path();
    if (daemon_exe.empty())
      return false;

    std::string cmd_line = UL::Daemon::arg();

    STARTUPINFOA si {};
    si.cb = sizeof(STARTUPINFOA);
    PROCESS_INFORMATION pi {};

    BOOL ret =
      CreateProcessA(daemon_exe.string().c_str(), cmd_line.data(), nullptr,
                     nullptr, TRUE, 0, nullptr, nullptr, &si, &pi);
    if (!ret) {
      UL::win_last_error("CreateProcessA");
      return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    UTILS_DPRINTF(STDOUT_FILENO,
                  LOG_LEVEL_STR_INFO "%sTry to start the daemon\n", UL::NATIVE);

    return true;
  }
#else
#endif

  bool wait_for_daemon() {
    auto header = log_shm_->header;

    int retries = 0;
    const int retry_times = 600;
    while (!header->is_daemon_ready.load(std::memory_order_relaxed)) {
      if (retries++ > retry_times) {
        UTILS_DPRINTF(STDERR_FILENO,
                      LOG_LEVEL_STR_ERROR "%sNo daemon was found\n",
                      UL::NATIVE);
        return false;
      }
      if (retries % 100 == 0) {
        UTILS_DPRINTF(STDOUT_FILENO,
                      LOG_LEVEL_STR_INFO
                      "%sTrying to acquire daemon. Total attempts: %d; "
                      "Attempted times: %d\n",
                      UL::NATIVE, retry_times, retries);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return true;
  }

  void fs_configure_shared(const char *log_path, size_t path_size,
                           StdType::Put put_type) {
    UL::Shm::Buffer buffer {};
    auto &data = buffer.data.fs;

    buffer.type = UL::Shm::DataType::Config;
    buffer.level = put_type == StdType::Put::Out ? SIRIUS_LOG_LEVEL_DEBUG
                                                 : SIRIUS_LOG_LEVEL_ERROR;

    if (path_size) {
      std::memcpy(data.path, log_path, path_size);
      data.path[path_size] = '\0';
      path_size += 1;
      data.state = UL::Shm::FsState::File;
    } else {
      data.state = UL::Shm::FsState::Std;
    }

    produce(&buffer, sizeof(UL::Shm::Buffer) - UL::LOG_PATH_MAX + path_size);
  }

  void fs_configure_private(const char *log_path, size_t path_size,
                            StdType::Put put_type) {
    if (path_size) {
      (void)log_path;
      /* ----------------------------------------- */
      /* ---------------- Not Yet ---------------- */
      /* ----------------------------------------- */
    } else {
      std::lock_guard<std::mutex> lock(mutex_);
      if (put_type == StdType::Put::Out) {
        fd_out_ = STDOUT_FILENO;
      } else {
        fd_err_ = STDERR_FILENO;
      }
    }
  }

  bool produce_fallback(void *src) {
    if (log_shm_->header->is_daemon_ready.load(std::memory_order_relaxed))
      [[likely]] {
      return false;
    }

    auto buffer = (UL::Shm::Buffer *)src;

    if (buffer->type == UL::Shm::DataType::Log) [[likely]] {
      log_write(buffer->level, (void *)buffer->data.log.buf,
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

extern "C" bool constructor_utils_log() {
#if defined(_WIN32) || defined(_WIN64)
  win_enable_ansi();
#endif

  return true;
}

extern "C" void destructor_utils_log() {}

// --- API ---
static bool g_api_initialization = false;
static std::mutex g_api_mutex;
static std::unique_ptr<LogManager> g_log_manager;

/**
 * @note About Windows' DLL, if there are still unfinished threads in the
 * process, there is still a risk of resource recovery failure, problems such as
 * deadlocks may occur. When considering all aspects, using `std::atexit` can
 * minimize this risk to the greatest extent.
 */
static void g_log_manager_deinit(void) {
  g_log_manager->deinit();
}

static force_inline bool initialization_check() {
  if (g_api_initialization) [[likely]]
    return true;

  std::lock_guard<std::mutex> lock(g_api_mutex);

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

  return UL::Daemon::Exe::set_path(path);
}
#else
extern "C" sirius_api int sirius_log_set_daemon_path(const char *path) {
  return 0;
}
#endif

extern "C" sirius_api void
sirius_log_configure(const sirius_log_config_t *config) {
  if (!initialization_check() || !config) [[unlikely]]
    return;

  g_log_manager->fs_configure(config->out, StdType::Put::Out);
  g_log_manager->fs_configure(config->err, StdType::Put::Out);
}

#define ISSUE_ERROR_STANDARD_FUNCTION(function) \
  do { \
    const char *e = \
      "\n" \
      "  The LOG module issues an ERROR\n" \
      "  " function " error\n"; \
    g_log_manager->log_write(SIRIUS_LOG_LEVEL_ERROR, e, strlen(e)); \
  } while (0)

#define ISSUE_ERROR_LOG_LOST() \
  do { \
    const char *e = \
      "\n" \
      "  The LOG module issues an ERROR\n" \
      "  Log length exceeds the buffer, log will be lost\n"; \
    g_log_manager->log_write(SIRIUS_LOG_LEVEL_ERROR, e, strlen(e)); \
  } while (0)

#define ISSUE_WARNING_LOG_TRUNCATED() \
  do { \
    const char *e = \
      "\n" \
      "  The LOG module issues a WARNING\n" \
      "  Log length exceeds the buffer, log will be truncated\n"; \
    g_log_manager->log_write(SIRIUS_LOG_LEVEL_ERROR, e, strlen(e)); \
  } while (0)

extern "C" sirius_api void sirius_log_impl(int level, const char *level_str,
                                           const char *color,
                                           const char *module, const char *file,
                                           const char *func, int line,
                                           const char *fmt, ...) {
  if (!initialization_check()) [[unlikely]]
    return;

  UL::Shm::Buffer buffer {};
  auto &data = buffer.data.log;
  size_t len = 0;

  buffer.type = UL::Shm::DataType::Log;
  buffer.level = level;

  time_t raw_time;
  struct tm tm_info;
  time(&raw_time);
  UTILS_LOCALTIME_R(&raw_time, &tm_info);

  /**
   * @ref https://linux.die.net/man/3/snprintf
   */
  int written =
    snprintf(data.buf, UL::LOG_BUF_SIZE,
             "%s%s [%02d:%02d:%02d %s %" PRIu64 " %s (%s|%d)]%s ", color,
             level_str, tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, module,
             sirius_thread_id(), file, func, line, LOG_COLOR_NONE);

  if (written < 0) {
    ISSUE_ERROR_STANDARD_FUNCTION("snprintf");
    return;
  }

  len += written;
  if (len >= UL::LOG_BUF_SIZE) [[unlikely]] {
    ISSUE_ERROR_LOG_LOST();
    return;
  }

  /**
   * @ref https://linux.die.net/man/3/vsnprintf
   */
  va_list args;
  va_start(args, fmt);
  written = vsnprintf(data.buf + len, UL::LOG_BUF_SIZE - len, fmt, args);
  va_end(args);

  if (written < 0) {
    ISSUE_ERROR_STANDARD_FUNCTION("vsnprintf");
    return;
  }

  len += written;
  if (len >= UL::LOG_BUF_SIZE) [[unlikely]] {
    len = UL::LOG_BUF_SIZE - 1;
    ISSUE_WARNING_LOG_TRUNCATED();
  }

  data.buf_size = len;

  g_log_manager->produce(&buffer,
                         sizeof(UL::Shm::Buffer) - UL::LOG_BUF_SIZE + len);
}

extern "C" sirius_api void sirius_logsp_impl(int level, const char *level_str,
                                             const char *color,
                                             const char *module,
                                             const char *fmt, ...) {
  if (!initialization_check()) [[unlikely]]
    return;

  UL::Shm::Buffer buffer {};
  auto &data = buffer.data.log;
  size_t len = 0;

  buffer.type = UL::Shm::DataType::Log;
  buffer.level = level;

  time_t raw_time;
  struct tm tm_info;
  time(&raw_time);
  UTILS_LOCALTIME_R(&raw_time, &tm_info);

  int written = snprintf(
    data.buf, UL::LOG_BUF_SIZE, "%s%s [%02d:%02d:%02d %s]%s ", color, level_str,
    tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, module, LOG_COLOR_NONE);

  if (written < 0) {
    ISSUE_ERROR_STANDARD_FUNCTION("snprintf");
    return;
  }

  len += written;
  if (len >= UL::LOG_BUF_SIZE) [[unlikely]] {
    ISSUE_ERROR_LOG_LOST();
    return;
  }

  va_list args;
  va_start(args, fmt);
  written = vsnprintf(data.buf + len, UL::LOG_BUF_SIZE - len, fmt, args);
  va_end(args);

  if (written < 0) {
    ISSUE_ERROR_STANDARD_FUNCTION("vsnprintf");
    return;
  }

  len += written;
  if (len >= UL::LOG_BUF_SIZE) [[unlikely]] {
    len = UL::LOG_BUF_SIZE - 1;
    ISSUE_WARNING_LOG_TRUNCATED();
  }

  data.buf_size = len;

  g_log_manager->produce(&buffer,
                         sizeof(UL::Shm::Buffer) - UL::LOG_BUF_SIZE + len);
}
