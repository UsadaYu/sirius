#include "sirius/foundation/log.h"

#include <mutex>
#include <thread>
#include <vector>

#include "lib/foundation/initializer.h"
#include "sirius/foundation/thread.h"
#include "utils/log/exe.hpp"
#include "utils/log/shm.hpp"

#if defined(_WIN32) || defined(_WIN64)
#else
#  include <sys/wait.h>
#endif

namespace sirius {
using u_io = utils::Io;
namespace u_log = utils::log;
namespace ul_shm = u_log::shm;

namespace {
enum class PutType : int {
  kIn = 0,
  kOut = 1,
  kErr = 2,
};

inline bool put_type_check(utils::utils::IntegralOrEnum auto type) {
  int index = static_cast<int>(type);

  if (index < static_cast<int>(PutType::kIn) ||
      index > static_cast<int>(PutType::kErr)) [[unlikely]] {
    return false;
  }

  return true;
}
} // namespace

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
  PrivateManager() = default;

  ~PrivateManager() = default;

  void log_write(int level, const void *buffer, size_t size) {
    std::lock_guard lock(mutex_);

    int fd = level <= SIRIUS_LOG_LEVEL_WARN ? fd_err_ : fd_out_;

    utils_write(fd, buffer, size);
  }

  bool fs_configure(const char *log_path, size_t path_size, PutType put_type) {
    if (path_size != 0) {
      (void)log_path;
      /* ----------------------------------------- */
      /* ---------------- Not Yet ---------------- */
      /* ----------------------------------------- */
    } else {
      std::lock_guard lock(mutex_);

      if (put_type == PutType::kOut) {
        fd_out_ = STDOUT_FILENO;
      } else {
        fd_err_ = STDERR_FILENO;
      }
    }

    return true;
  }

 private:
  std::mutex mutex_ {};
  int fd_out_ = STDOUT_FILENO;
  int fd_err_ = STDERR_FILENO;
};

static PrivateManager g_private_manager;
// ------------------------------------------------------------

class SharedManager {
 public:
  SharedManager() = default;

  ~SharedManager() {
    deinit();
  }

  auto init() -> std::expected<void, std::string> {
    if (initialized_.exchange(true, std::memory_order_relaxed))
      return {};

    std::string ret_msg;
#define E(e, label) \
  do { \
    if (auto ret = (e); !ret.has_value()) { \
      ret_msg = ret.error(); \
      goto label; \
    } \
  } while (0)

    E(ul_shm::GMutex::instance().lock(), label_free1);
    if (auto ret = log_shm_.shm_alloc(u_log::MasterType::kNative);
        !ret.has_value()) {
      ret_msg = ret.error();
      goto label_free2;
    } else {
      master_ = std::move(ret.value());
    }
    E(master_->slots_alloc(), label_free3);

    (void)ul_shm::GMutex::instance().unlock();

    if (try_spawner()) {
      E(spawn_daemon(), label_free4);
    }
    E(wait_for_daemon(), label_free4);

    return {};

  label_free4:
    (void)ul_shm::GMutex::instance().lock();
    master_->slots_free();
  label_free3:
    master_.reset();
    log_shm_.shm_free();
  label_free2:
    (void)ul_shm::GMutex::instance().unlock();
  label_free1:
    initialized_.store(false, std::memory_order_relaxed);

    return std::unexpected(ret_msg);
#undef E
  }

  void deinit() {
    if (!initialized_.exchange(false, std::memory_order_relaxed))
      return;

    (void)ul_shm::GMutex::instance().lock();

    master_->slots_free();
    master_.reset();
    log_shm_.shm_free();

    (void)ul_shm::GMutex::instance().unlock();
  }

  void produce_shared(void *src, size_t size) {
    auto spawn = [&]() -> bool {
      return try_spawner() ? spawn_daemon().has_value() : false;
    };

    if (!shared_valid()) [[unlikely]] {
      if (!spawn())
        return native_write(static_cast<ul_shm::Buffer *>(src));
    }

    uint64_t cur_write_idx = master_->get_shm_header()->write_index.fetch_add(
      1, std::memory_order_acq_rel);
    size_t slot_idx = cur_write_idx & (u_log::kShmCapacity - 1);
    ul_shm::Slot &slot = master_->get_shm_slots()[slot_idx];

    int retries = 0;
    while (slot.state.load(std::memory_order_acquire) !=
           ul_shm::SlotState::kFree) {
      ++retries;
      if (retries % 10 == 0) {
        std::this_thread::yield();
      }
      if (retries % 40 == 0) {
        std::this_thread::sleep_for(std::chrono::nanoseconds(100));
      }
      if (retries % 200 == 0) {
        retries = 0;
        (void)spawn();
      }
    }

    slot.state.store(ul_shm::SlotState::kWaiting, std::memory_order_release);
    slot.timestamp_ms.store(utils::time::get_monotonic_steady_ms(),
                            std::memory_order_relaxed);

    slot.pid = utils::process::pid();
    std::memcpy(&slot.buffer, src, size);

    slot.state.store(ul_shm::SlotState::kReady, std::memory_order_release);
  }

  bool shared_valid() const {
    return master_->get_shm_header()->is_daemon_ready.load(
             std::memory_order_relaxed) &&
      initialized_.load(std::memory_order_relaxed);
  }

  bool fs_configure(const char *log_path, size_t path_size, PutType put_type) {
    if (!shared_valid())
      return false;

    ul_shm::Buffer buffer {};
    auto &data = buffer.data.fs;

    buffer.type = ul_shm::DataType::kConfig;
    buffer.level = put_type == PutType::kOut ? SIRIUS_LOG_LEVEL_DEBUG
                                             : SIRIUS_LOG_LEVEL_ERROR;

    if (path_size) {
      std::memcpy(data.path, log_path, path_size);
      data.path[path_size] = '\0';
      path_size += 1;
      data.type = ul_shm::FsType::kFile;
    } else {
      data.type = ul_shm::FsType::kStd;
    }

    produce_shared(&buffer,
                   sizeof(ul_shm::Buffer) - u_log::kLogPathMax + path_size);

    return true;
  }

 private:
  static constexpr uint64_t kWaitDaemonTimeoutMs = 5000;

  std::atomic<bool> initialized_ = false;
  ul_shm::Shm &log_shm_ = ul_shm::Shm::instance();
  std::unique_ptr<ul_shm::Shm::Master> master_ {};

  /**
   * @note
   * - (1) The `spawn_daemon` function is process-safe, but it is recommended
   * to avoid repeated calls. So try to use this function before spawn the
   * daemon.
   *
   * - (2) This function prohibited from being guarded by the process mutex.
   */
  bool try_spawner() {
    const auto cur_ts = utils::time::get_monotonic_steady_ms();

    if (auto ret = ul_shm::GMutex::instance().lock(); !ret.has_value()) {
      return false;
    }

    if (master_->daemon_state() == ul_shm::Shm::Master::DaemonState::kAlive) {
      (void)ul_shm::GMutex::instance().unlock();
      return false;
    }

    auto &pre_ts = master_->get_shm_header()->spawner_timestamp;
    if (cur_ts - pre_ts > kWaitDaemonTimeoutMs) {
      pre_ts = cur_ts;
      (void)ul_shm::GMutex::instance().unlock();
      return true;
    }

    (void)ul_shm::GMutex::instance().unlock();

    return false;
  }

#if defined(_WIN32) || defined(_WIN64)
  auto spawn_daemon() -> std::expected<void, std::string> {
    std::string es;

    std::filesystem::path daemon_exe;
    if (auto ret = u_log::exe::Exe::instance().get_path(); !ret.has_value()) {
      return std::unexpected(
        IO_E("{0}", u_io::errno_err(ret.error(), "get_path (log exe)")));
    } else {
      daemon_exe = ret.value();
    }

    std::string cmd_line = std::format("\"{0}\" ", daemon_exe.string());

    for (const auto &cmd : u_log::exe::Args::cmds_daemon()) {
      cmd_line.append(std::format("\"{0}\" ", cmd));
    }

    STARTUPINFOA si {};
    si.cb = sizeof(STARTUPINFOA);
    PROCESS_INFORMATION pi {};

    utils::io_outln("Try to start the daemon");

    BOOL ret = CreateProcessA(nullptr, cmd_line.data(), nullptr, nullptr, TRUE,
                              //
                              0,
                              // DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP,
                              nullptr, nullptr, &si, &pi);
    if (!ret) {
      const DWORD dw_err = GetLastError();
      return std::unexpected(
        IO_E("{0}", u_io::win_err(dw_err, "CreateProcessA")));
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

    std::filesystem::path daemon_exe;
    if (auto ret = u_log::exe::Exe::instance().get_path(); !ret.has_value()) {
      return std::unexpected(
        IO_E("{0}", u_io::errno_err(ret.error(), "get_path (log exe)")));
    } else {
      daemon_exe = ret.value();
    }

    auto arg_cmds = u_log::exe::Args::cmds_daemon();
    std::vector<char *> argv;

    std::string exe_path = daemon_exe.string();
    argv.push_back(const_cast<char *>(exe_path.c_str()));

    for (const auto &cmd : arg_cmds) {
      argv.push_back(const_cast<char *>(cmd.c_str()));
    }
    argv.push_back(nullptr);

    utils::io_outln("Try to start the daemon");

    pid_t pid1 = fork();
    if (pid1 < 0) {
      const int errno_err = errno;
      return std::unexpected(
        IO_E("{0}", u_io::errno_err(errno_err, "get_path (log exe)")));
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
            IO_E("{0}", u_io::errno_err(errno_err, "waitpid")));
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
      es = IO_E("{0}", u_io::errno_err(errno_err, "setsid"));
      utils::io_ln_fd(STDERR_FILENO, es);
      _exit(kNormalExitStatus + 1);
    }

    pid_t pid2 = fork();
    if (pid2 < 0) {
      const int errno_err = errno;
      es = IO_E("{0}", u_io::errno_err(errno_err, "fork (pid2)"));
      utils::io_ln_fd(STDERR_FILENO, es);
      _exit(kNormalExitStatus + 2);
    }
    if (pid2 > 0) {
      _exit(kNormalExitStatus);
    }

    umask(0);
    if (chdir("/") < 0) {
      const int errno_err = errno;
      es = IO_E("{0}", u_io::errno_err(errno_err, "chdir"));
      utils::io_ln_fd(STDERR_FILENO, es);
      _exit(errno_err);
    }

#  if 1
    int fd = open("/dev/null", O_RDWR);
    if (fd != -1) {
      dup2(fd, STDIN_FILENO);
      dup2(fd, STDOUT_FILENO);
      dup2(fd, STDERR_FILENO);
      if (fd > 2) {
        close(fd);
      }
    }
#  endif

    execvp(argv[0], argv.data());
    const int errno_err = errno;
    es = IO_E("{0}", u_io::errno_err(errno_err, "execvp"));
    utils::io_ln_fd(STDERR_FILENO, es);

    _exit(errno_err);

    return {};
  }
#endif

  auto wait_for_daemon() -> std::expected<void, std::string> {
    auto header = master_->get_shm_header();

    int retries = 0;
    constexpr uint64_t kOnceSleepMs = 50;
    constexpr int kRetryTimes = kWaitDaemonTimeoutMs / kOnceSleepMs;
    while (!header->is_daemon_ready.load(std::memory_order_relaxed)) {
      if (retries++ > kRetryTimes) {
        return std::unexpected(IO_E("No daemon was found"));
      }
      if (retries % (kRetryTimes / 5) == 0) {
        auto es = IO_ISP(
          "\nTrying to acquire daemon. "
          "Total attempts: {0}; Attempted times: {1}",
          kRetryTimes, retries);
        utils::io_errln(es);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(kOnceSleepMs));
    }

    return {};
  }

  void native_write(ul_shm::Buffer *buffer) {
    if (buffer->type == ul_shm::DataType::kLog) {
      g_private_manager.log_write(buffer->level,
                                  static_cast<void *>(buffer->data.log.buf),
                                  buffer->data.log.buf_size);
    }
  }
};

// --- Global Instance & Hooks ---
namespace {
inline std::atomic<bool> g_shared_initialized = false;
inline std::mutex g_shared_mutex {};
inline uint64_t g_shared_try_times = 0;
inline uint64_t g_shared_try_timestamp_ms = 0;
inline constexpr uint64_t kSharedRetryTimeIntervalMilliseconds = 15 * 1000;
inline std::unique_ptr<SharedManager> g_shared_manager {};

inline void g_shared_log_manager_deinit(void) {
  g_shared_manager->deinit();
}

inline bool shared_initialization_check() {
  if (g_shared_initialized.load(std::memory_order_relaxed))
    return true;

  std::lock_guard lock(g_shared_mutex);

  if (g_shared_try_times > 1) {
    if (utils::time::get_monotonic_steady_ms() - g_shared_try_timestamp_ms <
        kSharedRetryTimeIntervalMilliseconds) {
      return false;
    }
    g_shared_try_times = 0;
  }

  g_shared_try_times += 1;
  g_shared_try_timestamp_ms = utils::time::get_monotonic_steady_ms();

  if (!g_shared_initialized.load(std::memory_order_relaxed)) {
    auto log_manager = std::make_unique<SharedManager>();

    if (auto ret = log_manager->init(); !ret.has_value()) {
      auto es = ret.error().append("\n");
      g_private_manager.log_write(SIRIUS_LOG_LEVEL_ERROR, es.c_str(),
                                  es.size());
    } else {
      g_shared_initialized.store(true, std::memory_order_relaxed);
      g_shared_manager = std::move(log_manager);
      std::atexit(g_shared_log_manager_deinit);
    }
  }

  return g_shared_initialized.load(std::memory_order_relaxed);
}
} // namespace

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
  void fs_configure(const sirius_log_fs_t &config, PutType put_type) {
    if (!put_type_check(put_type)) [[unlikely]]
      return;

    size_t path_size = 0;
    if (config.log_path) {
      path_size = utils_strnlen_s(config.log_path, u_log::kLogPathMax);
      if (path_size <= 0 || path_size >= u_log::kLogPathMax) {
        auto es = IO_WSP("Invalid argument. `log_path`").append("\n");
        g_private_manager.log_write(SIRIUS_LOG_LEVEL_WARN, es.c_str(),
                                    es.size());
        return;
      }
    }

    auto &to_shared = put_type == PutType::kOut ? out_to_shared : err_to_shared;

    bool to_shared_state;
    std::function<bool(const char *, size_t, PutType)> fs_configure_fn;

    if (config.shared == SiriusThreadProcess::kSiriusThreadProcessPrivate) {
      to_shared_state = false;
      fs_configure_fn = [&](const char *c, size_t s, PutType p) {
        return g_private_manager.fs_configure(c, s, p);
      };
    } else {
      if (!shared_initialization_check())
        return;

      to_shared_state = true;
      fs_configure_fn = [&](const char *c, size_t s, PutType p) {
        return g_shared_manager->fs_configure(c, s, p);
      };
    }

    if (fs_configure_fn(config.log_path, path_size, put_type)) {
      to_shared.store(to_shared_state, std::memory_order_relaxed);
    }
  }
};

namespace {
static FsManager g_fs_manager;

#if defined(_WIN32) || defined(_WIN64)
inline void win_enable_ansi() {
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

inline void error_log_lost() {
  auto es =
    IO_E("\nLog length exceeds the buffer, log will be lost").append("\n");
  g_private_manager.log_write(SIRIUS_LOG_LEVEL_ERROR, es.c_str(), es.size());
}

inline void error_lib_func(std::string_view fn_str) {
  auto es = IO_E("\n`{0}` error", fn_str).append("\n");
  g_private_manager.log_write(SIRIUS_LOG_LEVEL_ERROR, es.c_str(), es.size());
}

inline void error_log_truncated() {
  auto es = IO_WSP("\nLog length exceeds the buffer, log will be truncated")
              .append("\n");
  g_private_manager.log_write(SIRIUS_LOG_LEVEL_WARN, es.c_str(), es.size());
}

inline void log_write(ul_shm::Buffer &buffer, size_t data_len) {
  auto &fs_to_shared = buffer.level <= SIRIUS_LOG_LEVEL_WARN
    ? g_fs_manager.err_to_shared
    : g_fs_manager.out_to_shared;

  bool to_shared = fs_to_shared.load(std::memory_order_relaxed) &&
    shared_initialization_check();

  if (to_shared) {
    g_shared_manager->produce_shared(
      &buffer, sizeof(ul_shm::Buffer) - u_log::kLogBufferSize + data_len);
  } else {
    g_private_manager.log_write(buffer.level, (void *)buffer.data.log.buf,
                                buffer.data.log.buf_size);
  }
}

inline void level_prefix(int level, std::string &usr_prefix, const char *module,
                         const char *file, int line) {
  switch (level) {
  case SIRIUS_LOG_LEVEL_ERROR:
    usr_prefix = u_io::instance().s_error(file, line, module);
    break;
  case SIRIUS_LOG_LEVEL_WARN:
    usr_prefix = u_io::instance().s_warn(file, line, module);
    break;
  case SIRIUS_LOG_LEVEL_INFO:
    usr_prefix = u_io::instance().s_info(file, line, module);
    break;
  case SIRIUS_LOG_LEVEL_DEBUG:
    usr_prefix = u_io::instance().s_debug(file, line, module);
    break;
  default:
    usr_prefix = u_io::s_pre("Print", _SIRIUS_LOG_MODULE_NAME, file, line);
    break;
  }
}

inline size_t count_trailing_new_lines(std::string_view str) {
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
} // namespace
} // namespace sirius

using namespace sirius;

extern "C" bool constructor_foundation_log() {
#if defined(_WIN32) || defined(_WIN64)
  win_enable_ansi();
#endif

  return true;
}

extern "C" void destructor_foundation_log() {}

extern "C" sirius_api int sirius_log_set_exe_path(const char *path) {
  if (g_shared_initialized.load(std::memory_order_relaxed)) {
    sirius_error("The Daemon has started, it is too late to configure\n");
    return EBUSY;
  }

  return u_log::exe::Exe::instance().set_path(path);
}

extern "C" sirius_api void
sirius_log_configure(const sirius_log_config_t *config) {
  if (!config)
    return;

  g_fs_manager.fs_configure(config->out, PutType::kOut);
  g_fs_manager.fs_configure(config->err, PutType::kErr);
}

extern "C" sirius_api void sirius_log_impl(int level, const char *module,
                                           const char *file, int line,
                                           const char *fmt, ...) {
  ul_shm::Buffer buffer {};
  auto &data = buffer.data.log;

  buffer.type = ul_shm::DataType::kLog;
  buffer.level = level;

  std::string usr_prefix;
  size_t usr_prefix_size = 0;
  usr_prefix.reserve(u_io::kPrefixLength + 16);

  level_prefix(level, usr_prefix, module, file, line);
  usr_prefix_size = strlen(usr_prefix.c_str());

  if (usr_prefix_size >= u_log::kLogBufferSize) [[unlikely]]
    return error_log_lost();

  char usr_vdata[u_log::kLogBufferSize];
  size_t usr_vdata_size = 0;
  size_t usr_vdata_max = u_log::kLogBufferSize - usr_prefix_size;

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
  usr_data = u_io::row_gs("{0}", usr_vdata)
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
  ul_shm::Buffer buffer {};
  auto &data = buffer.data.log;

  buffer.type = ul_shm::DataType::kLog;
  buffer.level = level;

  std::string usr_prefix;
  size_t usr_prefix_size = 0;
  usr_prefix.reserve(u_io::kPrefixLength + 16);

  level_prefix(level, usr_prefix, module, "", 0);
  usr_prefix_size = strlen(usr_prefix.c_str());

  if (usr_prefix_size >= u_log::kLogBufferSize) [[unlikely]]
    return error_log_lost();

  char usr_vdata[u_log::kLogBufferSize];
  size_t usr_vdata_size = 0;
  size_t usr_vdata_max = u_log::kLogBufferSize - usr_prefix_size;

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
  usr_data = u_io::row_gs("{0}", usr_vdata)
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
