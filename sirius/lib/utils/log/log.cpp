#include "lib/utils/log/log.h"

#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "lib/utils/initializer.h"
#include "sirius/utils/fs.h"
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

class LogManager {
 public:
  std::mutex mutex;
  int fd_out = STDOUT_FILENO;
  int fd_err = STDERR_FILENO;

  LogManager()
      : is_activated_(false),
        is_creator_(false),
        daemon_arg_(Daemon::arg()),
        log_shm_(std::make_unique<LogShm>()) {}

  ~LogManager() {
    if (!is_activated_)
      return;

    sirius_log_config_t args {};
    args.out.fd_enable = true;
    args.out.fd = STDOUT_FILENO;
    args.err.fd_enable = true;
    args.err.fd = STDERR_FILENO;
    fd_configure(&args);

    is_activated_ = false;
    deinit();
  }

  bool init() {
    if (!ProcessMutex::create())
      return false;
    if (!ProcessMutex::lock())
      goto label_free1;
    if (!log_shm_->open_shm())
      goto label_free2;
    is_creator_ = log_shm_->shm_creator;
    if (!log_shm_->memory_map())
      goto label_free3;
    if (!log_shm_->slots_init())
      goto label_free4;

    ProcessMutex::unlock();

    if (is_creator_ && !start_daemon())
      goto label_free5;
    if (!wait_for_daemon())
      goto label_free5;

    is_activated_ = true;

    return true;

  label_free5:
    ProcessMutex::lock();
    log_shm_->slots_deinit();
  label_free4:
    log_shm_->memory_unmap();
  label_free3:
    log_shm_->close_shm(is_creator_);
  label_free2:
    ProcessMutex::unlock();
  label_free1:
    ProcessMutex::destory();

    return false;
  }

  void deinit() {
    ProcessMutex::lock();

    log_shm_->slots_deinit();
    log_shm_->memory_unmap();
    log_shm_->close_shm(false);

    ProcessMutex::unlock();

    ProcessMutex::destory();
  }

  void produce(void *src, size_t size) {
    if (!log_shm_->header->is_daemon_ready.load(std::memory_order_relaxed))
      [[unlikely]] {
      auto buffer = (LogBuffer *)src;
      log_write(buffer->level, (void *)buffer->data, buffer->data_size);
      return;
    }

    uint64_t cur_write_idx =
      log_shm_->header->write_index.fetch_add(1, std::memory_order_acq_rel);
    size_t slot_idx = cur_write_idx & (Utils::Log::SHM_CAPACITY - 1);
    ShmSlot &slot = log_shm_->slots[slot_idx];

    int retries = 0;
    while (slot.state.load(std::memory_order_acquire) != SlotState::FREE) {
      if (++retries > 1000) {
        std::this_thread::yield();
      }
    }

    slot.timestamp_ms.store(Utils::Time::get_monotonic_steady_ms(),
                            std::memory_order_relaxed);
    slot.state.store(SlotState::WRITING, std::memory_order_release);

    std::memcpy(&slot.buffer, src, size);

    slot.state.store(SlotState::READY, std::memory_order_release);
  }

  void log_write(int level, const void *buffer, size_t size) {
    auto header = log_shm_->header;

    std::lock_guard<std::mutex> lock(mutex);

    int fd = level <= sirius_log_level_warn ? fd_err : fd_out;

    UTILS_WRITE(fd, buffer, size);
  }

  void fd_configure(sirius_log_config_t *cfg) {
    auto &fs = log_shm_->header->fs;

    auto handle_fd_config = [&](auto &config, int &target_fd, int &fs_fd,
                                std::atomic<bool> &update_flag) {
      if (config.log_path) {
        if (int fd =
              ss_fs_open(config.log_path,
                         ss_fs_acc_wronly | ss_fs_opt_creat | ss_fs_opt_trunc,
                         SS_FS_PERM_RW);
            fd != -1) {
          std::lock_guard lock(mutex);
          target_fd = fd;
          fs_fd = target_fd;
          update_flag.store(true, std::memory_order_relaxed);
        }
      } else if (config.fd_enable && target_fd != config.fd) {
        std::lock_guard lock(mutex);
        target_fd = config.fd;
        fs_fd = target_fd;
        update_flag.store(true, std::memory_order_relaxed);
      }
    };

    handle_fd_config(cfg->out, fd_out, fs.fd_out, fs.fd_out_update);
    handle_fd_config(cfg->err, fd_err, fs.fd_err, fs.fd_err_update);
  }

 private:
  bool is_activated_;
  bool is_creator_;
  std::string daemon_arg_;
  std::unique_ptr<LogShm> log_shm_;

#if defined(_WIN32) || defined(_WIN64)
  bool start_daemon() {
    auto daemon_exe = Daemon::Exe::path();
    if (daemon_exe.empty())
      return false;

    std::string cmd_line = Daemon::arg();

    STARTUPINFOA si {};
    si.cb = sizeof(STARTUPINFOA);
    PROCESS_INFORMATION pi {};

    BOOL ret =
      CreateProcessA(daemon_exe.string().c_str(), cmd_line.data(), nullptr,
                     nullptr, TRUE, 0, nullptr, nullptr, &si, &pi);
    if (!ret) {
      Utils::Log::win_last_error("CreateProcessA");
      return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    UTILS_DPRINTF(STDOUT_FILENO,
                  log_level_str_info "%sTry to start the daemon\n",
                  Utils::Log::NATIVE);

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
                      log_level_str_error "%sNo daemon was found\n",
                      Utils::Log::NATIVE);
        return false;
      }
      if (retries % 100 == 0) {
        UTILS_DPRINTF(STDOUT_FILENO,
                      log_level_str_info
                      "%sTrying to acquire daemon. Total attempts: %d; "
                      "Attempted times: %d\n",
                      Utils::Log::NATIVE, retry_times, retries);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
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

static force_inline bool initialization_check() {
  if (g_api_initialization) [[likely]]
    return true;

  std::lock_guard<std::mutex> lock(g_api_mutex);

  if (!g_api_initialization) {
    auto log_manager = std::make_unique<LogManager>();

    g_api_initialization = log_manager->init();

    if (g_api_initialization) {
      g_log_manager = std::move(log_manager);
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

  return Daemon::Exe::set_path(path);
}
#else
extern "C" sirius_api int sirius_log_set_daemon_path(const char *path) {
  return 0;
}
#endif

extern "C" sirius_api void sirius_log_configure(sirius_log_config_t *cfg) {
  if (!initialization_check()) [[unlikely]]
    return;

  if (!cfg)
    return;

  g_log_manager->fd_configure(cfg);
}

#define ISSUE_ERROR_STANDARD_FUNCTION(function) \
  do { \
    const char *e = \
      "\n" \
      "  The LOG module issues an ERROR\n" \
      "  " function " error\n"; \
    g_log_manager->log_write(sirius_log_level_error, e, strlen(e)); \
  } while (0)

#define ISSUE_ERROR_LOG_LOST() \
  do { \
    const char *e = \
      "\n" \
      "  The LOG module issues an ERROR\n" \
      "  Log length exceeds the buffer, log will be lost\n"; \
    g_log_manager->log_write(sirius_log_level_error, e, strlen(e)); \
  } while (0)

#define ISSUE_WARNING_LOG_TRUNCATED() \
  do { \
    const char *e = \
      "\n" \
      "  The LOG module issues a WARNING\n" \
      "  Log length exceeds the buffer, log will be truncated\n"; \
    g_log_manager->log_write(sirius_log_level_error, e, strlen(e)); \
  } while (0)

extern "C" sirius_api void sirius_log_impl(int level, const char *level_str,
                                           const char *color,
                                           const char *module, const char *file,
                                           const char *func, int line,
                                           const char *fmt, ...) {
  if (!initialization_check()) [[unlikely]]
    return;

  LogBuffer buffer {};
  int len = 0;

  buffer.level = level;

  time_t raw_time;
  struct tm tm_info;
  time(&raw_time);
  UTILS_LOCALTIME_R(&raw_time, &tm_info);

  /**
   * @ref https://linux.die.net/man/3/snprintf
   */
  int written =
    snprintf(buffer.data, sirius_log_buf_size,
             "%s%s [%02d:%02d:%02d %s %" PRIu64 " %s (%s|%d)]%s ", color,
             level_str, tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, module,
             _sirius_thread_id(), file, func, line, log_color_none);

  if (written < 0) {
    ISSUE_ERROR_STANDARD_FUNCTION("snprintf");
    return;
  }

  len += written;
  if (len >= sirius_log_buf_size) [[unlikely]] {
    ISSUE_ERROR_LOG_LOST();
    return;
  }

  /**
   * @ref https://linux.die.net/man/3/vsnprintf
   */
  va_list args;
  va_start(args, fmt);
  written = vsnprintf(buffer.data + len, sirius_log_buf_size - len, fmt, args);
  va_end(args);

  if (written < 0) {
    ISSUE_ERROR_STANDARD_FUNCTION("vsnprintf");
    return;
  }

  len += written;
  if (len >= sirius_log_buf_size) [[unlikely]] {
    len = sirius_log_buf_size - 1;
    ISSUE_WARNING_LOG_TRUNCATED();
  }

  buffer.data_size = len;

  g_log_manager->produce(&buffer,
                         sizeof(LogBuffer) - sirius_log_buf_size + len);
}

extern "C" sirius_api void sirius_logsp_impl(int level, const char *level_str,
                                             const char *color,
                                             const char *module,
                                             const char *fmt, ...) {
  if (!initialization_check()) [[unlikely]]
    return;

  LogBuffer buffer {};
  int len = 0;

  buffer.level = level;

  time_t raw_time;
  struct tm tm_info;
  time(&raw_time);
  UTILS_LOCALTIME_R(&raw_time, &tm_info);

  int written =
    snprintf(buffer.data, sirius_log_buf_size, "%s%s [%02d:%02d:%02d %s]%s ",
             color, level_str, tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec,
             module, log_color_none);

  if (written < 0) {
    ISSUE_ERROR_STANDARD_FUNCTION("snprintf");
    return;
  }

  len += written;
  if (len >= sirius_log_buf_size) [[unlikely]] {
    ISSUE_ERROR_LOG_LOST();
    return;
  }

  va_list args;
  va_start(args, fmt);
  written = vsnprintf(buffer.data + len, sirius_log_buf_size - len, fmt, args);
  va_end(args);

  if (written < 0) {
    ISSUE_ERROR_STANDARD_FUNCTION("vsnprintf");
    return;
  }

  len += written;
  if (len >= sirius_log_buf_size) [[unlikely]] {
    len = sirius_log_buf_size - 1;
    ISSUE_WARNING_LOG_TRUNCATED();
  }

  buffer.data_size = len;

  g_log_manager->produce(&buffer,
                         sizeof(LogBuffer) - sirius_log_buf_size + len);
}
