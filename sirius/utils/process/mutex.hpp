#pragma once

#include "utils/log.h"
#include "utils/namespace.hpp"
#include "utils/process/process.hpp"

namespace Utils {
namespace Process {
enum class LockErrno : int {
  kSuccess = 0,
  kOwnerDead = 1,
  kFail = 2,
  kBusy = 3,
};

class FMutex {
  struct FHeader {
    uint8_t is_dirty; // 0: Clean; 1: Dirty
    uint64_t owner_pid;
  };

 private:
#if defined(_WIN32) || defined(_WIN64)
  static inline const HANDLE kInvalidFd = INVALID_HANDLE_VALUE;
#else
  static constexpr int kInvalidFd = -1;
#endif

 public:
  explicit FMutex(const char *file_name = nullptr) noexcept
      : fd_(kInvalidFd), file_name_(file_name ? std::string(file_name) : "") {
    if (file_name) {
      create();
    }
  }

  ~FMutex() noexcept {
    destroy();
  }

  FMutex(FMutex &&other) noexcept : file_name_(std::move(other.file_name_)) {
    fd_ = other.fd_;
    other.fd_ = kInvalidFd;
  }

  FMutex &operator=(FMutex &&other) noexcept {
    if (this != &other) {
      destroy();
      file_name_ = std::move(other.file_name_);
      fd_ = other.fd_;
      other.fd_ = kInvalidFd;
    }

    return *this;
  }

  FMutex(const FMutex &) = delete;
  FMutex &operator=(const FMutex &) = delete;

  bool create(const char *file_name = nullptr) {
    std::string es;

    if (file_name) {
      file_name_ = file_name;
    }

    if (file_name_.empty())
      return false;

    auto lock_path = Ns::Mutex::file_lock_path(file_name_);

    destroy();

#if defined(_WIN32) || defined(_WIN64)
    fd_ = CreateFileW(lock_path.c_str(), GENERIC_READ | GENERIC_WRITE,
                      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                      FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fd_ == INVALID_HANDLE_VALUE) {
      fd_ = kInvalidFd;
      const DWORD dw_err = GetLastError();
      es = std::format("{} -> `CreateFile`", utils_pretty_function);
      utils_win_last_error(dw_err, es.c_str());
      return false;
    }
#else
    fd_ = open(lock_path.c_str(), O_RDWR | O_CREAT,
               File::string_to_mode(_SIRIUS_POSIX_FILE_MODE));
    if (fd_ == -1) {
      fd_ = kInvalidFd;
      const int errno_err = errno;
      es = std::format("{} -> `open`", utils_pretty_function);
      utils_errno_error(errno_err, es.c_str());
      return false;
    }
#endif

    return true;
  }

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

  LockErrno lock() {
    std::string es;

#if defined(_WIN32) || defined(_WIN64)
    OVERLAPPED overlapped {};
    if (!LockFileEx(fd_, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &overlapped)) {
      const DWORD dw_err = GetLastError();
      es = std::format("{} -> `LockFileEx`", utils_pretty_function);
      utils_win_last_error(dw_err, es.c_str());
      return LockErrno::kFail;
    }
#else
    struct flock lock_ptr;
    lock_ptr.l_type = F_WRLCK;
    lock_ptr.l_whence = SEEK_SET;
    lock_ptr.l_start = 0;
    lock_ptr.l_len = 0;

    if (fcntl(fd_, F_SETLKW, &lock_ptr) == -1) {
      const int errno_err = errno;
      es = std::format("{} -> `fcntl` lock", utils_pretty_function);
      utils_errno_error(errno_err, es.c_str());
      return LockErrno::kFail;
    }
#endif

    LockErrno lock_errno = LockErrno::kSuccess;
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
        lock_errno = LockErrno::kOwnerDead;
        utils_dprintf(STDERR_FILENO,
                      "--------------------------------\n"
                      "- Warning -> %s\n"
                      "- The process (PID: %" PRIu64
                      ") that previously held the lock may have crashed\n"
                      "--------------------------------\n",
                      utils_pretty_function, header.owner_pid);
      }
    }

    FHeader new_header;
    new_header.is_dirty = 1;
    new_header.owner_pid = (uint64_t)pid();

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

    return lock_errno;
  }

  bool unlock() {
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
      es = std::format("{} -> `UnlockFileEx`", utils_pretty_function);
      utils_win_last_error(dw_err, es.c_str());
      return false;
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
      es = std::format("{} -> `fcntl` unlock", utils_pretty_function);
      utils_errno_error(errno_err, es.c_str());
      return false;
    }
#endif

    return true;
  }

  bool valid() const noexcept {
    return fd_ != kInvalidFd;
  }

 private:
#if defined(_WIN32) || defined(_WIN64)
  HANDLE fd_;
#else
  int fd_;
#endif
  std::string file_name_;
};

/**
 * @note On POSIX, only one construction and one destruction are required, the
 * `create` and `destroy` functions need to be explicitly called..
 */
class GMutex {
 public:
#if defined(_WIN32) || defined(_WIN64)
  explicit GMutex(const char *mutex_name = nullptr) noexcept
      : mutex_(nullptr),
        mutex_name_(mutex_name ? std::string(mutex_name) : "") {
    if (mutex_name) {
      create();
    }
  }
#else
  explicit GMutex(pthread_mutex_t *mutex = nullptr) noexcept : mutex_(mutex) {}
#endif

#if defined(_WIN32) || defined(_WIN64)
  ~GMutex() noexcept {
    destroy();
  }
#else
  ~GMutex() = default;
#endif

#if defined(_WIN32) || defined(_WIN64)
  GMutex(GMutex &&other) noexcept
      : mutex_name_(std::move(other.mutex_name_))
#else
  GMutex(GMutex &&other) noexcept
#endif
  {
    mutex_ = other.mutex_;
    other.mutex_ = nullptr;
  }

  GMutex &operator=(GMutex &&other) noexcept {
    if (this != &other) {
#if defined(_WIN32) || defined(_WIN64)
      destroy();
      mutex_name_ = std::move(other.mutex_name_);
#endif
      mutex_ = other.mutex_;
      other.mutex_ = nullptr;
    }

    return *this;
  }

  GMutex(const GMutex &) = delete;
  GMutex &operator=(const GMutex &) = delete;

#if defined(_WIN32) || defined(_WIN64)
  bool create(const char *mutex_name = nullptr) {
    std::string es;

    if (mutex_name) {
      mutex_name_ = mutex_name;
    }

    if (mutex_name_.empty())
      return false;

    destroy();

    mutex_ = CreateMutexA(nullptr, FALSE,
                          Ns::Mutex::win_generate_name(mutex_name_).c_str());
    if (!mutex_) {
      const DWORD dw_err = GetLastError();
      es = std::format("{} -> `CreateMutexA`", utils_pretty_function);
      utils_win_last_error(dw_err, es.c_str());
      return false;
    }

    return true;
  }
#else
  bool create(pthread_mutex_t *mutex = nullptr) {
    if (mutex) {
      mutex_ = mutex;
    }

    if (!mutex_)
      return false;

    int ret;
    pthread_mutexattr_t attr;
    std::string es;

    ret = pthread_mutexattr_init(&attr);
    if (ret) {
      es = std::format("{} -> `pthread_mutexattr_init`", utils_pretty_function);
      utils_errno_error(ret, es.c_str());
      return false;
    }

    ret = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    if (ret) {
      es = std::format("{} -> `pthread_mutexattr_setpshared`",
                       utils_pretty_function);
      goto label_free;
    }

    ret = pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    if (ret) {
      es = std::format("{} -> `pthread_mutexattr_setrobust`",
                       utils_pretty_function);
      goto label_free;
    }

    ret = pthread_mutex_init(mutex_, &attr);
    pthread_mutexattr_destroy(&attr);
    if (ret) {
      es = std::format("{} -> `pthread_mutex_init`", utils_pretty_function);
      utils_errno_error(ret, es.c_str());
      return false;
    }

    return true;

  label_free:
    utils_errno_error(ret, es.c_str());
    pthread_mutexattr_destroy(&attr);

    return false;
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

  LockErrno lock() {
    return T_lock(false);
  }

  LockErrno trylock() {
    return T_lock(true);
  }

  bool unlock() {
    std::string es;

#if defined(_WIN32) || defined(_WIN64)
    if (!ReleaseMutex(mutex_)) {
      const DWORD dw_err = GetLastError();
      es = std::format("{} -> `ReleaseMutex`", utils_pretty_function);
      utils_win_last_error(dw_err, es.c_str());
      return false;
    }
#else
    int ret = pthread_mutex_unlock(mutex_);
    if (ret) {
      es = std::format("{} -> `pthread_mutex_unlock`", utils_pretty_function);
      utils_errno_error(ret, es.c_str());
      return false;
    }
#endif

    return true;
  }

  bool valid() const noexcept {
    return mutex_ != nullptr;
  }

 private:
#if defined(_WIN32) || defined(_WIN64)
  HANDLE mutex_;
  std::string mutex_name_;
#else
  pthread_mutex_t *mutex_;
#endif

  LockErrno T_lock(bool is_trylock = false) {
    std::string es;

#if defined(_WIN32) || defined(_WIN64)
    DWORD dw_err = ERROR_SUCCESS;

    DWORD wait_ms = is_trylock ? 0 : INFINITE;

    DWORD wait_ret = WaitForSingleObject(mutex_, wait_ms);
    switch (wait_ret) {
    [[likely]] case WAIT_OBJECT_0:
      return LockErrno::kSuccess;
    case WAIT_ABANDONED:
      goto label_owner_dead;
    case WAIT_TIMEOUT:
      return LockErrno::kBusy;
    case WAIT_FAILED:
      dw_err = GetLastError();
      es = std::format("{0} -> `WaitForSingleObject` (trylock: {1})",
                       utils_pretty_function, is_trylock);
      utils_win_last_error(dw_err, es.c_str());
      return LockErrno::kFail;
    default:
      return LockErrno::kFail;
    }
#else
    int (*lock_ptr)(pthread_mutex_t *) =
      is_trylock ? pthread_mutex_trylock : pthread_mutex_lock;

    int ret = lock_ptr(mutex_);
    switch (ret) {
    [[likely]] case 0:
      return LockErrno::kSuccess;
    case EOWNERDEAD:
      pthread_mutex_consistent(mutex_);
      goto label_owner_dead;
    case EBUSY:
      return LockErrno::kBusy;
    case ENOTRECOVERABLE:
    default:
      es = std::format("{0} -> `lock_ptr` (trylock: {1})",
                       utils_pretty_function, is_trylock);
      utils_errno_error(ret, es.c_str());
      return LockErrno::kFail;
    }

    return LockErrno::kSuccess;
#endif

  label_owner_dead:
    utils_dprintf(
      STDERR_FILENO,
      "--------------------------------\n"
      "- Warning -> %s\n"
      "- The process that previously held the lock may have crashed\n"
      "--------------------------------\n",
      utils_pretty_function);

    return LockErrno::kOwnerDead;
  }
};
} // namespace Process
} // namespace Utils
