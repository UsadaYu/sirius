#pragma once

#include "utils/io.hpp"
#include "utils/ns.hpp"
#include "utils/process/process.hpp"

namespace sirius {
namespace utils {
namespace process {
enum class LockState : int {
  kSuccess = 0,
  kOwnerDead = 1,
  kBusy = 2,
};

class FMutex;
class GMutex;
class Inner {
 private:
  friend class FMutex;
  friend class GMutex;

  static std::string line_pid() { return std::format("\nPID: {0}", pid()); }

#if defined(_WIN32) || defined(_WIN64)
  static std::string c_error(const DWORD err_code, std::string_view fn_str) {
    return Io::win_err(err_code, fn_str, "{}", line_pid());
  }
#else
  static std::string c_error(const int err_code, std::string_view fn_str) {
    return Io::errno_err(err_code, fn_str, "{}", line_pid());
  }
#endif
};

class FMutex {
 private:
  struct FHeader {
    uint8_t is_dirty; // 0: Clean; 1: Dirty
    uint64_t owner_pid;
  };

#if defined(_WIN32) || defined(_WIN64)
  static inline const HANDLE kInvalidFd = INVALID_HANDLE_VALUE;
  using hmutex = HANDLE;
#else
  static inline constexpr int kInvalidFd = -1;
  using hmutex = int;
#endif

 private:
  explicit FMutex(hmutex fd, std::filesystem::path lock_path) noexcept
      : fd_(fd), lock_path_(lock_path) {}

 public:
  FMutex(const FMutex &) = delete;
  FMutex &operator=(const FMutex &) = delete;

  FMutex &operator=(FMutex &&other) noexcept {
    if (this != &other) {
      destroy();
      fd_ = other.fd_;
      lock_path_ = other.lock_path_;
      other.fd_ = kInvalidFd;
      other.lock_path_ = "";
    }
    return *this;
  }

  FMutex(FMutex &&other) noexcept
      : fd_(other.fd_), lock_path_(other.lock_path_) {
    other.fd_ = kInvalidFd;
    other.lock_path_ = "";
  }

  /**
   * @note Considering the file may be deleted or replaced.
   * On Windows, the `FILE_SHARE_DELETE` will not be included when
   * `CreateFileW`.
   * On POSIX, prevent this by `stat`.
   */
  static auto create(std::string_view file_name)
    -> std::expected<FMutex, UTrace> {
    if (file_name.empty()) {
      auto es = std::format("\nInvalid argument. Null `file_name`{0}",
                            Inner::line_pid());
      return std::unexpected(UTrace(std::move(es)));
    }

    std::filesystem::path lock_path {};
    if (auto ret = ns::Mutex::instance().file_lock_path(file_name);
        !ret.has_value()) {
      ret.error().msg_append(Inner::line_pid());
      utrace_return(ret);
    } else {
      lock_path = std::move(ret.value());
    }

    hmutex fd;
#if defined(_WIN32) || defined(_WIN64)
    fd = CreateFileW(lock_path.c_str(), GENERIC_READ | GENERIC_WRITE,
                     FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                     FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fd == INVALID_HANDLE_VALUE) {
      const DWORD dw_err = GetLastError();
      return std::unexpected(UTrace(Inner::c_error(dw_err, "CreateFileW")));
    }
#else
    fd = open(lock_path.c_str(), O_RDWR | O_CREAT,
              File::string_to_mode(_SIRIUS_POSIX_FILE_MODE));
    if (fd == -1) {
      const int errno_err = errno;
      return std::unexpected(UTrace(Inner::c_error(errno_err, "open")));
    }
#endif

    return FMutex(fd, std::move(lock_path));
  }

  ~FMutex() noexcept { destroy(); }

  void destroy() noexcept {
    if (fd_ != kInvalidFd) {
#if defined(_WIN32) || defined(_WIN64)
      CloseHandle(fd_);
#else
      close(fd_);
#endif
      fd_ = kInvalidFd;
    }
  }

  auto lock() -> std::expected<LockState, UTrace> {
#if defined(_WIN32) || defined(_WIN64)
    OVERLAPPED overlapped {};
    if (!LockFileEx(fd_, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &overlapped)) {
      const DWORD dw_err = GetLastError();
      return std::unexpected(UTrace(Inner::c_error(dw_err, "LockFileEx")));
    }
#else
    struct flock lock_ptr {};
    lock_ptr.l_type = F_WRLCK;
    lock_ptr.l_whence = SEEK_SET;
    lock_ptr.l_start = 0;
    lock_ptr.l_len = 0;
    if (fcntl(fd_, F_SETLKW, &lock_ptr) == -1) {
      const int errno_err = errno;
      return std::unexpected(UTrace(Inner::c_error(errno_err, "fcntl (lock)")));
    }

    struct stat st_fd {}, st_path {};
    if (fstat(fd_, &st_fd) == -1) {
      const int errno_err = errno;
      return std::unexpected(UTrace(Inner::c_error(errno_err, "fstat")));
    }

    if (stat(lock_path_.c_str(), &st_path) == -1 ||
        st_fd.st_ino != st_path.st_ino) {
      auto es = std::format("File was unlinked or replaced before locking{0}",
                            Inner::line_pid());
      return std::unexpected(UTrace(std::move(es)));
    }
#endif

    LockState lock_state = LockState::kSuccess;
    FHeader header;
    [[maybe_unused]] bool is_first_create = false;

    size_t read_len = 0;
#if defined(_WIN32) || defined(_WIN64)
    DWORD read_bytes;
    OVERLAPPED ov_read {};
    if (ReadFile(fd_, &header, sizeof(FHeader), &read_bytes, &ov_read)) {
      read_len = read_bytes;
    }
#else
    read_len = pread(fd_, &header, sizeof(FHeader), 0);
#endif

    if (read_len != sizeof(FHeader)) {
      /**
       * @note The file may be empty (created for the first time) or broken,
       * which is regarded as normal.
       */
      is_first_create = true;
    } else {
      if (header.is_dirty == 1) {
        lock_state = LockState::kOwnerDead;
        auto es = std::format(
          "\nThe process (PID: {0}) that previously held the lock may have "
          "crashed{1}",
          header.owner_pid, Inner::line_pid());
        io_ln_warnsp("{}", es);
      }
    }

    FHeader new_header;
    new_header.is_dirty = 1;
    new_header.owner_pid = static_cast<uint64_t>(pid());
#if defined(_WIN32) || defined(_WIN64)
    DWORD written_bytes;
    OVERLAPPED ov_write {};
    if (!WriteFile(fd_, &new_header, sizeof(FHeader), &written_bytes,
                   &ov_write)) {
      FlushFileBuffers(fd_);
    }
#else
    if (pwrite(fd_, &new_header, sizeof(FHeader), 0) != -1) {
      fdatasync(fd_);
    }
#endif

    return lock_state;
  }

  auto unlock() -> std::expected<void, UTrace> {
    std::string es;
    FHeader header;
    header.is_dirty = 0;
    header.owner_pid = 0;

#if defined(_WIN32) || defined(_WIN64)
    DWORD written_bytes;
    OVERLAPPED ov_write {};
    if (!WriteFile(fd_, &header, sizeof(FHeader), &written_bytes, &ov_write)) {
      FlushFileBuffers(fd_);
    }

    OVERLAPPED ov_lock {};
    if (!UnlockFileEx(fd_, 0, 1, 0, &ov_lock)) {
      const DWORD dw_err = GetLastError();
      return std::unexpected(UTrace(Inner::c_error(dw_err, "UnlockFileEx")));
    }
#else
    if (pwrite(fd_, &header, sizeof(FHeader), 0) != -1) {
      fdatasync(fd_);
    }

    struct flock lock_ptr;
    lock_ptr.l_type = F_UNLCK;
    lock_ptr.l_whence = SEEK_SET;
    lock_ptr.l_start = 0;
    lock_ptr.l_len = 0;
    if (fcntl(fd_, F_SETLK, &lock_ptr) == -1) {
      const int errno_err = errno;
      return std::unexpected(
        UTrace(Inner::c_error(errno_err, "fcntl (unlock)")));
    }
#endif

    return {};
  }

  bool valid() const noexcept { return fd_ != kInvalidFd; }

 private:
  hmutex fd_;
  std::filesystem::path lock_path_;
};

/**
 * @note On POSIX, only one construction and one destruction are required.
 */
class GMutex {
 private:
#if defined(_WIN32) || defined(_WIN64)
  explicit GMutex(HANDLE mutex) : mutex_(mutex) {}
#else
  explicit GMutex(pthread_mutex_t *mutex) : mutex_(mutex) {}
#endif

 public:
  GMutex(const GMutex &) = delete;
  GMutex &operator=(const GMutex &) = delete;

  GMutex &operator=(GMutex &&other) noexcept {
    if (this != &other) {
#if defined(_WIN32) || defined(_WIN64)
      destroy();
#endif
      mutex_ = other.mutex_;
      other.mutex_ = nullptr;
    }
    return *this;
  }

  GMutex(GMutex &&other) noexcept : mutex_(other.mutex_) {
    other.mutex_ = nullptr;
  }

#if defined(_WIN32) || defined(_WIN64)
  ~GMutex() noexcept { destroy(); }
#else
  ~GMutex() = default;
#endif

#if defined(_WIN32) || defined(_WIN64)
  static auto create(std::string_view mutex_name)
    -> std::expected<GMutex, UTrace> {
    if (mutex_name.empty()) {
      auto es = std::format("\nInvalid argument. Null `mutex_name`{0}",
                            Inner::line_pid());
      return std::unexpected(UTrace(std::move(es)));
    }

    HANDLE mutex = CreateMutexA(
      nullptr, FALSE, ns::Mutex::win_generate_name(mutex_name).c_str());
    if (!mutex) {
      const DWORD dw_err = GetLastError();
      return std::unexpected(UTrace(Inner::c_error(dw_err, "CreateMutexA")));
    }

    return GMutex(mutex);
  }
#else
  static auto create(pthread_mutex_t *mutex, bool is_creator)
    -> std::expected<GMutex, UTrace> {
    if (!mutex) {
      auto es =
        std::format("\nInvalid argument. Null `mutex`{0}", Inner::line_pid());
      return std::unexpected(UTrace(std::move(es)));
    }

    if (!is_creator)
      return GMutex(mutex);

    pthread_mutexattr_t attr;
    if (int ret = pthread_mutexattr_init(&attr); ret) {
      return std::unexpected(
        UTrace(Inner::c_error(ret, "pthread_mutexattr_init")));
    }
    if (int ret = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        ret) {
      pthread_mutexattr_destroy(&attr);
      return std::unexpected(
        UTrace(Inner::c_error(ret, "pthread_mutexattr_setpshared")));
    }
    if (int ret = pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
        ret) {
      pthread_mutexattr_destroy(&attr);
      return std::unexpected(
        UTrace(Inner::c_error(ret, "pthread_mutexattr_setrobust")));
    }
    if (int ret = pthread_mutex_init(mutex, &attr); ret) {
      pthread_mutexattr_destroy(&attr);
      return std::unexpected(UTrace(Inner::c_error(ret, "pthread_mutex_init")));
    }
    pthread_mutexattr_destroy(&attr);

    return GMutex(mutex);
  }
#endif

  void destroy() noexcept {
    if (mutex_ != nullptr) {
#if defined(_WIN32) || defined(_WIN64)
      CloseHandle(mutex_);
#else
      pthread_mutex_destroy(mutex_);
#endif
      mutex_ = nullptr;
    }
  }

  auto lock() -> std::expected<LockState, UTrace> { return lock_impl(false); }

  auto trylock() -> std::expected<LockState, UTrace> { return lock_impl(true); }

  auto unlock() -> std::expected<void, UTrace> {
#if defined(_WIN32) || defined(_WIN64)
    if (!ReleaseMutex(mutex_)) {
      const DWORD dw_err = GetLastError();
      return std::unexpected(UTrace(Inner::c_error(dw_err, "ReleaseMutex")));
    }
#else
    int ret = pthread_mutex_unlock(mutex_);
    if (ret) {
      return std::unexpected(
        UTrace(Inner::c_error(ret, "pthread_mutex_unlock")));
    }
#endif

    return {};
  }

  bool valid() const noexcept { return mutex_ != nullptr; }

 private:
#if defined(_WIN32) || defined(_WIN64)
  HANDLE mutex_;
#else
  pthread_mutex_t *mutex_;
#endif

  auto lock_impl(bool is_trylock = false) -> std::expected<LockState, UTrace> {
#if defined(_WIN32) || defined(_WIN64)
    DWORD dw_err = ERROR_SUCCESS;
    DWORD wait_ms = is_trylock ? 0 : INFINITE;
    const std::string fn_str =
      std::format("WaitForSingleObject (trylock: {0})", is_trylock);

    switch (DWORD wait_ret = WaitForSingleObject(mutex_, wait_ms); wait_ret) {
    [[likely]] case WAIT_OBJECT_0:
      return LockState::kSuccess;
    case WAIT_ABANDONED:
      goto label_owner_dead;
    case WAIT_TIMEOUT:
      return LockState::kBusy;
    case WAIT_FAILED:
      dw_err = GetLastError();
      return std::unexpected(UTrace(Inner::c_error(dw_err, fn_str)));
    default:
      auto es = std::format("\n{0}. `wait_ret`: {1}{2}", fn_str, wait_ret,
                            Inner::line_pid());
      return std::unexpected(UTrace(std::move(es)));
    }
#else
    int (*lock_ptr)(pthread_mutex_t *) =
      is_trylock ? pthread_mutex_trylock : pthread_mutex_lock;
    const std::string fn_str =
      std::format("mutex_lock (trylock: {0})", is_trylock);

    switch (int ret = lock_ptr(mutex_); ret) {
    [[likely]] case 0:
      return LockState::kSuccess;
    case EOWNERDEAD:
      pthread_mutex_consistent(mutex_);
      goto label_owner_dead;
    case EBUSY:
      return LockState::kBusy;
    case ENOTRECOVERABLE:
    default:
      return std::unexpected(UTrace(Inner::c_error(ret, fn_str)));
    }
#endif

  label_owner_dead:
    auto es = std::format(
      "\nThe process that previously held the lock may have crashed{0}",
      Inner::line_pid());
    io_ln_warnsp("{}", es);
    return LockState::kOwnerDead;
  }
};
} // namespace process
} // namespace utils
} // namespace sirius
