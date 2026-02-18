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

#if defined(_WIN32) || defined(_WIN64)
#else
#  include <sys/wait.h>
#endif

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

class PrivateManager {
 public:
  PrivateManager() : fd_out_(STDOUT_FILENO), fd_err_(STDERR_FILENO) {}

  ~PrivateManager() = default;

  void log_write(int level, const void *buffer, size_t size) {
    std::lock_guard lock(mutex_);

    int fd = level <= SIRIUS_LOG_LEVEL_WARN ? fd_err_ : fd_out_;

    UTILS_WRITE(fd, buffer, size);
  }

  bool fs_configure(const char *log_path, size_t path_size,
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

    return true;
  }

 private:
  std::mutex mutex_;
  int fd_out_;
  int fd_err_;
};

static PrivateManager g_private_manager;

class SharedManager {
 public:
  SharedManager()
      : is_initialization_(false),
        log_shm_(std::make_unique<Utils::Log::Shm::Shm>(
          Utils::Log::Shm::MasterType::kNative)) {}

  ~SharedManager() {
    deinit();
  }

  [[nodiscard]] bool init() {
    if (is_initialization_.exchange(true, std::memory_order_relaxed)) {
      return false;
    }

    if (!Utils::Log::Shm::GMutex::create())
      return false;
    if (!Utils::Log::Shm::GMutex::lock())
      goto label_free1;
    if (!log_shm_->init())
      goto label_free2;

    Utils::Log::Shm::GMutex::unlock();

    if (log_shm_->is_shm_creator() && !spawn_daemon())
      goto label_free3;
    if (!wait_for_daemon())
      goto label_free3;

    is_initialization_.store(true, std::memory_order_relaxed);

    return true;

  label_free3:
    Utils::Log::Shm::GMutex::lock();
    log_shm_->deinit();
  label_free2:
    Utils::Log::Shm::GMutex::unlock();
  label_free1:
    Utils::Log::Shm::GMutex::destroy();

    is_initialization_.store(false, std::memory_order_relaxed);

    return false;
  }

  void deinit() {
    if (!is_initialization_.exchange(false, std::memory_order_relaxed)) {
      return;
    }

    Utils::Log::Shm::GMutex::lock();

    log_shm_->deinit();

    Utils::Log::Shm::GMutex::unlock();

    Utils::Log::Shm::GMutex::destroy();
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

    slot.pid = Utils::Process::pid();
    std::memcpy(&slot.buffer, src, size);

    slot.state.store(Utils::Log::Shm::SlotState::kReady,
                     std::memory_order_release);
  }

  bool shared_valid() const {
    return log_shm_->header->is_daemon_ready.load(std::memory_order_relaxed) &&
      is_initialization_.load(std::memory_order_relaxed);
  }

  bool fs_configure(const char *log_path, size_t path_size,
                    StdType::Put put_type) {
    if (!shared_valid())
      return false;

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

    return true;
  }

 private:
  std::atomic<bool> is_initialization_;
  std::unique_ptr<Utils::Log::Shm::Shm> log_shm_;

#if defined(_WIN32) || defined(_WIN64)
  bool spawn_daemon() {
    std::string es;

    auto daemon_exe = Utils::Log::Daemon::exe_instance.get_path();
    if (daemon_exe.empty())
      return false;

    std::string cmd_line = "\"" + daemon_exe.string() + "\" ";

    for (const auto &cmd : Utils::Log::Daemon::args_instance.arg_spawn_cmds()) {
      cmd_line += "\"" + cmd + "\" ";
    }

    STARTUPINFOA si {};
    si.cb = sizeof(STARTUPINFOA);
    PROCESS_INFORMATION pi {};

    utils_dprintf(STDOUT_FILENO,
                  LOG_LEVEL_STR_INFO "%sTry to start the daemon\n",
                  Utils::Log::kNative);

    BOOL ret = CreateProcessA(nullptr, cmd_line.data(), nullptr, nullptr, TRUE,
                              DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP,
                              nullptr, nullptr, &si, &pi);
    if (!ret) {
      const DWORD dw_err = GetLastError();
      es = std::format("{0}{1} -> `CreateProcessA`", Utils::Log::kNative,
                       utils_pretty_function);
      utils_win_last_error(dw_err, es.c_str());
      return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return true;
  }
#else
  /**
   * @note The `fork` function will copy a large amount of `VIRT`, but the `RSS`
   * remains small, so this is not a major issue.
   * The main issue lies in the fact that in a multi-threaded environment, it
   * may lead to fatal problems such as `deadlocks` and `C++ destructors`
   * errors, so here synchronize Windows, using `Helper Binary` way.
   */
  bool spawn_daemon() {
    std::string es;

    auto errno_error = [&es](int errno_err, const std::string msg = "") {
      es = std::format("{0}{1} -> {2}", Utils::Log::kNative,
                       utils_pretty_function, msg);
      utils_errno_error(errno_err, es.c_str());
    };

#  ifndef LOG_DAEMON_FORK_AND_EXEC
#    define LOG_DAEMON_FORK_AND_EXEC 1
#  endif

#  if LOG_DAEMON_FORK_AND_EXEC
    auto daemon_exe = Utils::Log::Daemon::exe_instance.get_path();
    if (daemon_exe.empty())
      return false;

    std::vector<std::string> arg_cmds =
      Utils::Log::Daemon::args_instance.arg_spawn_cmds();
    std::vector<char *> argv;

    std::string exe_path = daemon_exe.string();
    argv.push_back(const_cast<char *>(exe_path.c_str()));

    for (const auto &cmd : arg_cmds) {
      argv.push_back(const_cast<char *>(cmd.c_str()));
    }
    argv.push_back(nullptr);
#  endif

    pid_t pid1 = fork();
    if (pid1 < 0) {
      errno_error(errno, "`fork` pid1");
      return false;
    }

    constexpr int kNormalExitStatus = 0;

    // --- Parent Process ---
    if (pid1 > 0) {
      int wstatus;

      if (waitpid(pid1, &wstatus, 0) < 0) {
        const int errno_err = errno;
        if (errno_err != ECHILD) {
          errno_error(errno_err, "`waitpid`");
          return false;
        }
      }

      if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != kNormalExitStatus) {
        utils_dprintf(STDERR_FILENO,
                      LOG_LEVEL_STR_ERROR
                      "%sFailed to initialize. wstatus: %d\n",
                      Utils::Log::kNative, wstatus);
        return false;
      }

      return true;
    }

    // --- Subprocess ---
    /**
     * @note Here, the `_exit` function is used instead of `exit` to prevent
     * tools like ASAN from flooding the memory leak log.
     */

    if (setsid() < 0) {
      errno_error(errno, "`setsid`");
      std::exit(kNormalExitStatus + 1);
    }

    utils_dprintf(STDOUT_FILENO,
                  LOG_LEVEL_STR_INFO "%sTry to start the daemon\n",
                  Utils::Log::kNative);

    pid_t pid2 = fork();
    if (pid2 < 0) {
      errno_error(errno, "`fork` pid2");
      std::exit(kNormalExitStatus + 2);
    }
    if (pid2 > 0) {
      std::exit(kNormalExitStatus);
    }

    umask(0);
    if (chdir("/") < 0) {
      const int errno_err = errno;
      errno_error(errno_err, "`chdir`");
      _exit(errno_err);
    }

#  if 0
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    int fd = open("/dev/null", O_RDWR);
    if (fd >= 0) {
      dup2(fd, STDIN_FILENO);
      dup2(fd, STDOUT_FILENO);
      dup2(fd, STDERR_FILENO);
      if (fd > 2) {
        close(fd);
      }
    }
#  endif

#  if LOG_DAEMON_FORK_AND_EXEC
    execvp(argv[0], argv.data());

    const int errno_err = errno;
    errno_error(errno_err, "`execvp`");
    _exit(errno_err);
#  else
    {
      auto log_manager =
        std::make_unique<Utils::Log::Daemon::Daemon::LogManager>();
      log_manager->main();
    }

    _exit(0);
#  endif

    return true;
#  undef LOG_DAEMON_FORK_AND_EXEC
  }
#endif

  bool wait_for_daemon() {
    auto header = log_shm_->header;

    int retries = 0;
    constexpr int kRetryTimes = 500;
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

  bool produce_fallback(void *src) {
    if (shared_valid()) [[likely]]
      return false;

    auto buffer = (Utils::Log::Shm::Buffer *)src;

    if (buffer->type == Utils::Log::Shm::DataType::kLog) [[likely]] {
      g_private_manager.log_write(buffer->level, (void *)buffer->data.log.buf,
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
static std::atomic<bool> g_shared_initialization = false;
static std::mutex g_shared_mutex;
static uint64_t g_shared_try_times = 0;
static uint64_t g_shared_try_timestamp_ms = 0;
static constexpr uint64_t kSharedRetryTimeIntervalMilliseconds = 15 * 1000;
static std::unique_ptr<SharedManager> g_shared_manager;

static void g_shared_log_manager_deinit(void) {
  g_shared_manager->deinit();
}

static force_inline bool shared_initialization_check() {
  if (g_shared_initialization.load(std::memory_order_relaxed))
    return true;

  std::lock_guard lock(g_shared_mutex);

  if (g_shared_try_times > 1) {
    if (Utils::Time::get_monotonic_steady_ms() - g_shared_try_timestamp_ms <
        kSharedRetryTimeIntervalMilliseconds) {
      return false;
    }
    g_shared_try_times = 0;
  }

  g_shared_try_times += 1;
  g_shared_try_timestamp_ms = Utils::Time::get_monotonic_steady_ms();

  if (!g_shared_initialization.load(std::memory_order_relaxed)) {
    auto log_manager = std::make_unique<SharedManager>();

    g_shared_initialization.store(log_manager->init(),
                                  std::memory_order_relaxed);

    if (g_shared_initialization.load(std::memory_order_relaxed)) {
      g_shared_manager = std::move(log_manager);
      std::atexit(g_shared_log_manager_deinit);
    }
  }

  return g_shared_initialization.load(std::memory_order_relaxed);
}

extern "C" sirius_api int sirius_log_set_daemon_path(const char *path) {
  if (g_shared_initialization) {
    sirius_error("The Daemon has started, it is too late to configure\n");
    return EBUSY;
  }

  return Utils::Log::Daemon::exe_instance.set_path(path);
}

class FsManager {
 public:
  std::atomic<bool> out_to_shared = true;
  std::atomic<bool> err_to_shared = true;

  FsManager() = default;

  ~FsManager() = default;

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

    auto &to_shared =
      put_type == StdType::Put::kOut ? out_to_shared : err_to_shared;

    bool to_shared_state;
    std::function<bool(const char *, size_t, StdType::Put)> fs_configure_fn;

    if (config.shared == SiriusThreadProcess::kSiriusThreadProcessPrivate) {
      to_shared_state = false;
      fs_configure_fn = [&](const char *c, size_t s, StdType::Put p) {
        return g_private_manager.fs_configure(c, s, p);
      };
    } else {
      if (!shared_initialization_check())
        return;

      to_shared_state = true;
      fs_configure_fn = [&](const char *c, size_t s, StdType::Put p) {
        return g_shared_manager->fs_configure(c, s, p);
      };
    }

    if (fs_configure_fn(config.log_path, path_size, put_type)) {
      to_shared.store(to_shared_state, std::memory_order_relaxed);
    }
  }
};

static FsManager g_fs_manager;

extern "C" sirius_api void
sirius_log_configure(const sirius_log_config_t *config) {
  if (!config)
    return;

  g_fs_manager.fs_configure(config->out, StdType::Put::kOut);
  g_fs_manager.fs_configure(config->err, StdType::Put::kErr);
}

#define ISSUE_ERROR_STANDARD_FUNCTION(function) \
  do { \
    const char *e = \
      "\n" \
      "  The LOG module issues an ERROR\n" \
      "  " function " error\n"; \
    g_private_manager.log_write(SIRIUS_LOG_LEVEL_ERROR, e, strlen(e)); \
  } while (0)

#define ISSUE_ERROR_LOG_LOST() \
  do { \
    const char *e = \
      "\n" \
      "  The LOG module issues an ERROR\n" \
      "  Log length exceeds the buffer, log will be lost\n"; \
    g_private_manager.log_write(SIRIUS_LOG_LEVEL_ERROR, e, strlen(e)); \
  } while (0)

#define ISSUE_WARNING_LOG_TRUNCATED() \
  do { \
    const char *e = \
      "\n" \
      "  The LOG module issues a WARNING\n" \
      "  Log length exceeds the buffer, log will be truncated\n"; \
    g_private_manager.log_write(SIRIUS_LOG_LEVEL_ERROR, e, strlen(e)); \
  } while (0)

static inline void log_write(Utils::Log::Shm::Buffer &buffer, size_t data_len) {
  auto &fs_to_shared = buffer.level <= SIRIUS_LOG_LEVEL_WARN
    ? g_fs_manager.err_to_shared
    : g_fs_manager.out_to_shared;

  bool to_shared = fs_to_shared.load(std::memory_order_relaxed) &&
    shared_initialization_check();

  if (to_shared) {
    g_shared_manager->produce_shared(
      &buffer,
      sizeof(Utils::Log::Shm::Buffer) - Utils::Log::kLogBufferSize + data_len);
  } else {
    g_private_manager.log_write(buffer.level, (void *)buffer.data.log.buf,
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
