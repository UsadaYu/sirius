/**
 * @note Exception handling is required when the daemon is killed or crashed,
 * which will be added later.
 */

#include "sirius/foundation/log.h"

#include <mutex>
#include <thread>
#include <vector>

#include "lib/foundation/initializer.h"
#include "sirius/foundation/thread.h"
#include "utils/log/exe.hpp"
#include "utils/time.hpp"

#if defined(_WIN32) || defined(_WIN64)
#else
#  include <sys/wait.h>
#endif

using UIo = Utils::Io;
namespace ULog = Utils::Log;

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

// ------------------------------------------------------------
/**
 * @note
 * ------------------------------
 * Subsequently, this class will be moved to the `io.hpp` header file and
 * implemented in a singleton pattern.
 * ------------------------------
 */
class PrivateManager {
 public:
  PrivateManager() : fd_out_(STDOUT_FILENO), fd_err_(STDERR_FILENO) {}

  ~PrivateManager() = default;

  void log_write(int level, const void *buffer, size_t size) {
    std::lock_guard lock(mutex_);

    int fd = level <= SIRIUS_LOG_LEVEL_WARN ? fd_err_ : fd_out_;

    utils_write(fd, buffer, size);
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
// ------------------------------------------------------------

class SharedManager {
 public:
  SharedManager()
      : is_initialization_(false), log_shm_(ULog::Shm::SharedMem::instance()) {}

  ~SharedManager() {
    deinit();
  }

  auto init() -> std::expected<void, std::string> {
    if (is_initialization_.exchange(true, std::memory_order_relaxed)) {
      return {};
    }

    std::string ret_msg;

    if (auto ret = ULog::Shm::GMutex::create(); !ret.has_value()) {
      ret_msg = ret.error();
      goto label_free1;
    }
    if (auto ret = ULog::Shm::GMutex::lock(); !ret.has_value()) {
      ret_msg = ret.error();
      goto label_free2;
    }
    if (auto ret = log_shm_.shm_alloc(ULog::MasterType::kNative);
        !ret.has_value()) {
      ret_msg = ret.error();
      goto label_free3;
    } else {
      master_ = std::move(ret.value());
    }
    if (auto ret = master_->slots_alloc(); !ret.has_value()) {
      ret_msg = ret.error();
      goto label_free4;
    } else {
      slots_ = ret.value();
    }

    (void)ULog::Shm::GMutex::unlock();

    if (master_->is_shm_creator()) {
      if (auto ret = spawn_daemon(); !ret.has_value()) {
        ret_msg = ret.error();
        goto label_free5;
      }
    }
    if (auto ret = wait_for_daemon(); !ret.has_value()) {
      ret_msg = ret.error();
      goto label_free5;
    }

    return {};

  label_free5:
    (void)ULog::Shm::GMutex::lock();
    master_->slots_free();
  label_free4:
    master_.reset();
    log_shm_.shm_free();
  label_free3:
    (void)ULog::Shm::GMutex::unlock();
  label_free2:
    ULog::Shm::GMutex::destroy();
  label_free1:
    is_initialization_.store(false, std::memory_order_relaxed);

    return std::unexpected(ret_msg);
  }

  void deinit() {
    if (!is_initialization_.exchange(false, std::memory_order_relaxed)) {
      return;
    }

    (void)ULog::Shm::GMutex::lock();

    master_->slots_free();
    master_.reset();
    log_shm_.shm_free();

    (void)ULog::Shm::GMutex::unlock();

    ULog::Shm::GMutex::destroy();
  }

  void produce_shared(void *src, size_t size) {
    if (produce_fallback(src)) [[unlikely]]
      return;

    uint64_t cur_write_idx = master_->get_shm_header()->write_index.fetch_add(
      1, std::memory_order_acq_rel);
    size_t slot_idx = cur_write_idx & (ULog::kShmCapacity - 1);
    ULog::Shm::Slot &slot = slots_[slot_idx];

    int retries = 0;
    while (slot.state.load(std::memory_order_acquire) !=
           ULog::Shm::SlotState::kFree) {
      ++retries;
      if (retries % 10 == 0) {
        std::this_thread::yield();
      } else if (retries > 50) {
        retries = 0;
        std::this_thread::sleep_for(std::chrono::nanoseconds(100));
      }
    }

    slot.state.store(ULog::Shm::SlotState::kWaiting, std::memory_order_release);
    slot.timestamp_ms.store(Utils::Time::get_monotonic_steady_ms(),
                            std::memory_order_relaxed);

    slot.pid = Utils::Process::pid();
    std::memcpy(&slot.buffer, src, size);

    slot.state.store(ULog::Shm::SlotState::kReady, std::memory_order_release);
  }

  bool shared_valid() const {
    return master_->get_shm_header()->is_daemon_ready.load(
             std::memory_order_relaxed) &&
      is_initialization_.load(std::memory_order_relaxed);
  }

  bool fs_configure(const char *log_path, size_t path_size,
                    StdType::Put put_type) {
    if (!shared_valid())
      return false;

    ULog::Shm::Buffer buffer {};
    auto &data = buffer.data.fs;

    buffer.type = ULog::Shm::DataType::kConfig;
    buffer.level = put_type == StdType::Put::kOut ? SIRIUS_LOG_LEVEL_DEBUG
                                                  : SIRIUS_LOG_LEVEL_ERROR;

    if (path_size) {
      std::memcpy(data.path, log_path, path_size);
      data.path[path_size] = '\0';
      path_size += 1;
      data.type = ULog::Shm::FsType::kFile;
    } else {
      data.type = ULog::Shm::FsType::kStd;
    }

    produce_shared(&buffer,
                   sizeof(ULog::Shm::Buffer) - ULog::kLogPathMax + path_size);

    return true;
  }

 private:
  std::atomic<bool> is_initialization_;
  ULog::Shm::SharedMem &log_shm_;
  std::unique_ptr<ULog::Shm::SharedMem::Master> master_;
  ULog::Shm::Slot *slots_;

#if defined(_WIN32) || defined(_WIN64)
  auto spawn_daemon() -> std::expected<void, std::string> {
    std::string es;

    std::filesystem::path daemon_exe;
    if (auto ret = ULog::Exe::Exe::instance().get_path(); !ret.has_value()) {
      return std::unexpected(
        IO_E("{0}", UIo::errno_err(ret.error(), "get_path (log exe)")));
    } else {
      daemon_exe = ret.value();
    }

    std::string cmd_line = "\"" + daemon_exe.string() + "\" ";

    for (const auto &cmd : ULog::Exe::Args::cmds_daemon()) {
      cmd_line += "\"" + cmd + "\" ";
    }

    STARTUPINFOA si {};
    si.cb = sizeof(STARTUPINFOA);
    PROCESS_INFORMATION pi {};

    Utils::io_outln("Try to start the daemon");

    BOOL ret = CreateProcessA(nullptr, cmd_line.data(), nullptr, nullptr, TRUE,
                              //
                              0,
                              // DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP,
                              nullptr, nullptr, &si, &pi);
    if (!ret) {
      const DWORD dw_err = GetLastError();
      return std::unexpected(
        IO_E("{0}", UIo::win_err(dw_err, "CreateProcessA")));
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return {};
  }
#else
  /**
   * @note The `fork` function will copy a large amount of `VIRT`, but the `RSS`
   * remains small, so this is not a major issue.
   * The main issue lies in the fact that in a multi-threaded environment, it
   * may lead to fatal problems such as `deadlocks` and `C++ destructors`
   * errors, so here synchronize Windows, using `Helper Binary` way.
   */
  auto spawn_daemon() -> std::expected<void, std::string> {
    std::string es;

#  ifndef LOG_DAEMON_FORK_AND_EXEC
#    define LOG_DAEMON_FORK_AND_EXEC 1
#  endif

#  if LOG_DAEMON_FORK_AND_EXEC
    std::filesystem::path daemon_exe;
    if (auto ret = ULog::Exe::Exe::instance().get_path(); !ret.has_value()) {
      return std::unexpected(
        IO_E("{0}", UIo::errno_err(ret.error(), "get_path (log exe)")));
    } else {
      daemon_exe = ret.value();
    }

    std::vector<std::string> arg_cmds = ULog::Exe::Args::cmds_daemon();
    std::vector<char *> argv;

    std::string exe_path = daemon_exe.string();
    argv.push_back(const_cast<char *>(exe_path.c_str()));

    for (const auto &cmd : arg_cmds) {
      argv.push_back(const_cast<char *>(cmd.c_str()));
    }
    argv.push_back(nullptr);
#  endif

    Utils::io_outln("Try to start the daemon");

    pid_t pid1 = fork();
    if (pid1 < 0) {
      const int errno_err = errno;
      return std::unexpected(
        IO_E("{0}", UIo::errno_err(errno_err, "get_path (log exe)")));
    }

    constexpr int kNormalExitStatus = 0;

    // --- Parent Process ---
    if (pid1 > 0) {
      int wstatus;

      if (waitpid(pid1, &wstatus, 0) < 0) {
        const int errno_err = errno;
        if (errno_err != ECHILD) {
          const int errno_err = errno;
          return std::unexpected(
            IO_E("{0}", UIo::errno_err(errno_err, "waitpid")));
        }
      }

      if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != kNormalExitStatus) {
        return std::unexpected(
          IO_E("Failed to initialize. wstatus: {0}", wstatus));
      }

      return {};
    }

    // --- Subprocess ---
    /**
     * @note Here, the `_exit` function is used instead of `exit` to prevent
     * tools like ASAN from flooding the memory leak log and return an
     * unexpected value.
     */

    if (setsid() < 0) {
      const int errno_err = errno;
      es = IO_E("{0}", UIo::errno_err(errno_err, "setsid"));
      Utils::io_ln_fd(STDERR_FILENO, es);
      _exit(kNormalExitStatus + 1);
    }

    pid_t pid2 = fork();
    if (pid2 < 0) {
      const int errno_err = errno;
      es = IO_E("{0}", UIo::errno_err(errno_err, "fork (pid2)"));
      Utils::io_ln_fd(STDERR_FILENO, es);
      _exit(kNormalExitStatus + 2);
    }
    if (pid2 > 0) {
      _exit(kNormalExitStatus);
    }

    umask(0);
    if (chdir("/") < 0) {
      const int errno_err = errno;
      es = IO_E("{0}", UIo::errno_err(errno_err, "chdir"));
      Utils::io_ln_fd(STDERR_FILENO, es);
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
    es = IO_E("{0}", UIo::errno_err(errno_err, "execvp"));
    Utils::io_ln_fd(STDERR_FILENO, es);

    _exit(errno_err);
#  else
    {
      auto log_manager = std::make_unique<ULog::Exe::Daemon>();
      log_manager->main();
    }

    _exit(0);
#  endif

    return {};
#  undef LOG_DAEMON_FORK_AND_EXEC
  }
#endif

  auto wait_for_daemon() -> std::expected<void, std::string> {
    auto header = master_->get_shm_header();

    int retries = 0;
    constexpr int kRetryTimes = 500;
    while (!header->is_daemon_ready.load(std::memory_order_relaxed)) {
      if (retries++ > kRetryTimes) {
        return std::unexpected(IO_E("No daemon was found"));
      }
      if (retries % 100 == 0) {
        auto es = IO_ISP(
          "\nTrying to acquire daemon. "
          "Total attempts: {0}; Attempted times: {1}",
          kRetryTimes, retries);
        Utils::io_errln(es);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return {};
  }

  bool produce_fallback(void *src) {
    if (shared_valid()) [[likely]]
      return false;

    auto buffer = (ULog::Shm::Buffer *)src;

    if (buffer->type == ULog::Shm::DataType::kLog) [[likely]] {
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

static inline bool shared_initialization_check() {
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

    if (auto ret = log_manager->init(); !ret.has_value()) {
      auto es = ret.error().append("\n");
      g_private_manager.log_write(SIRIUS_LOG_LEVEL_ERROR, es.c_str(),
                                  es.size());
    } else {
      g_shared_initialization.store(true, std::memory_order_relaxed);
      g_shared_manager = std::move(log_manager);
      std::atexit(g_shared_log_manager_deinit);
    }
  }

  return g_shared_initialization.load(std::memory_order_relaxed);
}

extern "C" sirius_api int sirius_log_set_exe_path(const char *path) {
  if (g_shared_initialization.load(std::memory_order_relaxed)) {
    sirius_error("The Daemon has started, it is too late to configure\n");
    return EBUSY;
  }

  return ULog::Exe::Exe::instance().set_path(path);
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
      path_size = utils_strnlen_s(config.log_path, ULog::kLogPathMax);
      if (path_size <= 0 || path_size >= ULog::kLogPathMax) {
        auto es = IO_WSP("Invalid argument. `log_path`").append("\n");
        g_private_manager.log_write(SIRIUS_LOG_LEVEL_WARN, es.c_str(),
                                    es.size());
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

static inline void error_log_lost() {
  auto es =
    IO_E("\nLog length exceeds the buffer, log will be lost").append("\n");
  g_private_manager.log_write(SIRIUS_LOG_LEVEL_ERROR, es.c_str(), es.size());
}

static inline void error_lib_func(std::string_view fn_str) {
  auto es = IO_E("\n`{0}` error", fn_str).append("\n");
  g_private_manager.log_write(SIRIUS_LOG_LEVEL_ERROR, es.c_str(), es.size());
}

static inline void error_log_truncated() {
  auto es = IO_WSP("\nLog length exceeds the buffer, log will be truncated")
              .append("\n");
  g_private_manager.log_write(SIRIUS_LOG_LEVEL_WARN, es.c_str(), es.size());
}

static inline void log_write(ULog::Shm::Buffer &buffer, size_t data_len) {
  auto &fs_to_shared = buffer.level <= SIRIUS_LOG_LEVEL_WARN
    ? g_fs_manager.err_to_shared
    : g_fs_manager.out_to_shared;

  bool to_shared = fs_to_shared.load(std::memory_order_relaxed) &&
    shared_initialization_check();

  if (to_shared) {
    g_shared_manager->produce_shared(
      &buffer, sizeof(ULog::Shm::Buffer) - ULog::kLogBufferSize + data_len);
  } else {
    g_private_manager.log_write(buffer.level, (void *)buffer.data.log.buf,
                                buffer.data.log.buf_size);
  }
}

static force_inline void level_prefix(int level, std::string &usr_prefix,
                                      const char *module, const char *file,
                                      int line) {
  switch (level) {
  case SIRIUS_LOG_LEVEL_ERROR:
    usr_prefix = UIo::instance().s_error(file, line, module);
    break;
  case SIRIUS_LOG_LEVEL_WARN:
    usr_prefix = UIo::instance().s_warn(file, line, module);
    break;
  case SIRIUS_LOG_LEVEL_INFO:
    usr_prefix = UIo::instance().s_info(file, line, module);
    break;
  case SIRIUS_LOG_LEVEL_DEBUG:
    usr_prefix = UIo::instance().s_debug(file, line, module);
    break;
  default:
    break;
  }
}

static force_inline size_t count_trailing_new_lines(std::string_view str) {
  size_t count = 0;

  for (auto it = str.rbegin(); it != str.rend(); ++it) {
    if (*it == '\n') {
      ++count;
    } else {
      break;
    }
  }

  return count;
}

extern "C" sirius_api void sirius_log_impl(int level, const char *module,
                                           const char *file, int line,
                                           const char *fmt, ...) {
  ULog::Shm::Buffer buffer {};
  auto &data = buffer.data.log;

  buffer.type = ULog::Shm::DataType::kLog;
  buffer.level = level;

  std::string usr_prefix;
  size_t usr_prefix_size = 0;
  usr_prefix.reserve(UIo::kPrefixLength + 16);

  level_prefix(level, usr_prefix, module, file, line);
  usr_prefix_size = strlen(usr_prefix.c_str());

  if (usr_prefix_size >= ULog::kLogBufferSize) [[unlikely]] {
    return error_log_lost();
  }

  char usr_vdata[ULog::kLogBufferSize];
  size_t usr_vdata_size = 0;
  size_t usr_vdata_max = ULog::kLogBufferSize - usr_prefix_size;

  /**
   * @ref https://linux.die.net/man/3/vsnprintf
   */
  va_list args;
  va_start(args, fmt);
  usr_vdata_size = vsnprintf(usr_vdata, usr_vdata_max, fmt, args);
  va_end(args);

  if (usr_vdata_size < 0) [[unlikely]] {
    return error_lib_func("vsnprintf");
  }

  std::string usr_data;
  usr_data.reserve(usr_vdata_size + 32);
  usr_data = UIo::row_gs("{0}", usr_vdata)
               .append(count_trailing_new_lines(usr_vdata), '\n');
  size_t usr_data_size = strlen(usr_data.c_str());

  if (usr_data_size >= usr_vdata_max) [[unlikely]] {
    usr_data_size = usr_vdata_max - 1;
    error_log_truncated();
  }

  data.buf_size = usr_prefix_size + usr_data_size;

  std::memcpy(data.buf, usr_prefix.c_str(), usr_prefix_size);
  std::memcpy(data.buf + usr_prefix_size, usr_data.c_str(), usr_data_size);

  log_write(buffer, data.buf_size);
}

extern "C" sirius_api void sirius_logsp_impl(int level, const char *module,
                                             const char *fmt, ...) {
  ULog::Shm::Buffer buffer {};
  auto &data = buffer.data.log;

  buffer.type = ULog::Shm::DataType::kLog;
  buffer.level = level;

  std::string usr_prefix;
  size_t usr_prefix_size = 0;
  usr_prefix.reserve(UIo::kPrefixLength + 16);

  level_prefix(level, usr_prefix, module, "", 0);
  usr_prefix_size = strlen(usr_prefix.c_str());

  if (usr_prefix_size >= ULog::kLogBufferSize) [[unlikely]] {
    return error_log_lost();
  }

  char usr_vdata[ULog::kLogBufferSize];
  size_t usr_vdata_size = 0;
  size_t usr_vdata_max = ULog::kLogBufferSize - usr_prefix_size;

  /**
   * @ref https://linux.die.net/man/3/vsnprintf
   */
  va_list args;
  va_start(args, fmt);
  usr_vdata_size = vsnprintf(usr_vdata, usr_vdata_max, fmt, args);
  va_end(args);

  if (usr_vdata_size < 0) [[unlikely]] {
    return error_lib_func("vsnprintf");
  }

  std::string usr_data;
  usr_data.reserve(usr_vdata_size + 32);
  usr_data = UIo::row_gs("{0}", usr_vdata)
               .append(count_trailing_new_lines(usr_vdata), '\n');
  size_t usr_data_size = strlen(usr_data.c_str());

  if (usr_data_size >= usr_vdata_max) [[unlikely]] {
    usr_data_size = usr_vdata_max - 1;
    error_log_truncated();
  }

  data.buf_size = usr_prefix_size + usr_data_size;

  std::memcpy(data.buf, usr_prefix.c_str(), usr_prefix_size);
  std::memcpy(data.buf + usr_prefix_size, usr_data.c_str(), usr_data_size);

  log_write(buffer, data.buf_size);
}
