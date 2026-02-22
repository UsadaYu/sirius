#pragma once

#include "utils/io.h"
#include "utils/namespace.hpp"
#include "utils/process/process.hpp"

namespace Utils {
namespace Process {
enum class LockState : int {
  kSuccess = 0,
  kOwnerDead = 1,
  kBusy = 2,

  // kFail = 3,
};

class FMutex {
#define FMUTEX_SUFFIX \
  (Io::row("PID: {0}", pid()) + Io::row(utils_pretty_function))

 private:
  struct FHeader {
    uint8_t is_dirty; // 0: Clean; 1: Dirty
    uint64_t owner_pid;
  };

#if defined(_WIN32) || defined(_WIN64)
  static inline const HANDLE kInvalidFd = INVALID_HANDLE_VALUE;
  using MutexHandle = HANDLE;
#else
  static inline constexpr int kInvalidFd = -1;
  using MutexHandle = int;
#endif

 private:
  explicit FMutex(MutexHandle fd) noexcept : fd_(fd) {}

 public:
  FMutex(const FMutex &) = delete;
  FMutex &operator=(const FMutex &) = delete;
  FMutex(FMutex &&) noexcept = default;

  static auto create(const char *file_name)
    -> std::expected<FMutex, std::string> {
    if (!file_name) {
      return std::unexpected(Io::io().s_error(SIRIUS_FILE_NAME, __LINE__) +
                             "Invalid argument. Null `file_name`" +
                             FMUTEX_SUFFIX);
    }

    auto lock_path = Ns::Mutex::instance().file_lock_path(file_name);
    if (!lock_path.has_value()) {
      return std::unexpected(lock_path.error() + FMUTEX_SUFFIX);
    }

    MutexHandle fd;

#if defined(_WIN32) || defined(_WIN64)
    fd = CreateFileW(lock_path.value().c_str(), GENERIC_READ | GENERIC_WRITE,
                     FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                     FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fd == INVALID_HANDLE_VALUE) {
      const DWORD dw_err = GetLastError();
      return std::unexpected(Io::win_last_error(dw_err, "CreateFileW") +
                             FMUTEX_SUFFIX);
    }
#else
    fd = open(lock_path.value().c_str(), O_RDWR | O_CREAT,
              File::string_to_mode(_SIRIUS_POSIX_FILE_MODE));
    if (fd == -1) {
      const int errno_err = errno;
      return std::unexpected(Io::errno_error(errno_err, "open") +
                             FMUTEX_SUFFIX);
    }
#endif

    return FMutex(fd);
  }

  ~FMutex() noexcept {
    destroy();
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

  auto lock() -> std::expected<LockState, std::string> {
#if defined(_WIN32) || defined(_WIN64)
    OVERLAPPED overlapped {};
    if (!LockFileEx(fd_, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &overlapped)) {
      const DWORD dw_err = GetLastError();
      return std::unexpected(Io::win_last_error(dw_err, "LockFileEx") +
                             FMUTEX_SUFFIX);
    }
#else
    struct flock lock_ptr;
    lock_ptr.l_type = F_WRLCK;
    lock_ptr.l_whence = SEEK_SET;
    lock_ptr.l_start = 0;
    lock_ptr.l_len = 0;

    if (fcntl(fd_, F_SETLKW, &lock_ptr) == -1) {
      const int errno_err = errno;
      return std::unexpected(Io::errno_error(errno_err, "fcntl") +
                             FMUTEX_SUFFIX);
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
        std::string es = Io::io().s_warn("") + Io::row("PID: {0}", pid()) +
          Io::row("The process (PID: {0}) that previously held the lock may "
                  "have crashed\n",
                  header.owner_pid);
        UTILS_WRITE(STDERR_FILENO, es.c_str(), es.size());
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

  auto unlock() -> std::expected<void, std::string> {
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
      return std::unexpected(Io::win_last_error(dw_err, "UnlockFileEx") +
                             FMUTEX_SUFFIX);
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
      return std::unexpected(Io::errno_error(errno_err, "fcntl") +
                             FMUTEX_SUFFIX);
    }
#endif

    return {};
  }

  bool valid() const noexcept {
    return fd_ != kInvalidFd;
  }

 private:
  MutexHandle fd_;

#undef FMUTEX_SUFFIX
};

/**
 * @note On POSIX, only one construction and one destruction are required.
 */
class GMutex {
#define GMUTEX_SUFFIX \
  (Io::row("PID: {0}", pid()) + Io::row(utils_pretty_function))

 private:
#if defined(_WIN32) || defined(_WIN64)
  explicit GMutex(HANDLE mutex) : mutex_(mutex) {}
#else
  explicit GMutex(pthread_mutex_t *mutex) : mutex_(mutex) {}
#endif

 public:
  GMutex(const GMutex &) = delete;
  GMutex &operator=(const GMutex &) = delete;

  GMutex(GMutex &&other) noexcept : mutex_(other.mutex_) {
    other.mutex_ = nullptr;
  }

#if defined(_WIN32) || defined(_WIN64)
  ~GMutex() noexcept {
    destroy();
  }
#else
  ~GMutex() = default;
#endif

#if defined(_WIN32) || defined(_WIN64)
  static auto create(const char *mutex_name)
    -> std::expected<GMutex, std::string> {
    if (!mutex_name) {
      return std::unexpected(Io::io().s_error(SIRIUS_FILE_NAME, __LINE__) +
                             "Invalid argument. Null `mutex_name`" +
                             GMUTEX_SUFFIX);
    }

    HANDLE mutex =
      CreateMutexA(nullptr, FALSE,
                   Ns::Mutex::instance().win_generate_name(mutex_name).c_str());
    if (!mutex) {
      const DWORD dw_err = GetLastError();
      return std::unexpected(Io::win_last_error(dw_err, "CreateMutexA") +
                             GMUTEX_SUFFIX);
    }

    return GMutex(mutex);
  }
#else
  static auto create(pthread_mutex_t *mutex, bool is_creator)
    -> std::expected<GMutex, std::string> {
    if (!mutex) {
      return std::unexpected(Io::io().s_error(SIRIUS_FILE_NAME, __LINE__) +
                             "Invalid argument. Null `mutex`" + GMUTEX_SUFFIX);
    }

    if (!is_creator)
      return GMutex(mutex);

    int ret;
    std::string es;
    pthread_mutexattr_t attr;

    ret = pthread_mutexattr_init(&attr);
    if (ret) {
      return std::unexpected(Io::errno_error(ret, "pthread_mutexattr_init") +
                             GMUTEX_SUFFIX);
    }

    ret = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    if (ret) {
      es = Io::errno_error(ret, "pthread_mutexattr_setpshared") + GMUTEX_SUFFIX;
      goto label_free;
    }

    ret = pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    if (ret) {
      es = Io::errno_error(ret, "pthread_mutexattr_setrobust") + GMUTEX_SUFFIX;
      goto label_free;
    }

    ret = pthread_mutex_init(mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    if (ret) {
      es = Io::errno_error(ret, "pthread_mutex_init") + GMUTEX_SUFFIX;
      return std::unexpected(es);
    }

    return GMutex(mutex);

  label_free:
    pthread_mutexattr_destroy(&attr);

    return std::unexpected(es);
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

  auto lock() -> std::expected<LockState, std::string> {
    return T_lock(false);
  }

  auto trylock() -> std::expected<LockState, std::string> {
    return T_lock(true);
  }

  auto unlock() -> std::expected<void, std::string> {
    std::string es;

#if defined(_WIN32) || defined(_WIN64)
    if (!ReleaseMutex(mutex_)) {
      const DWORD dw_err = GetLastError();
      return std::unexpected(Io::win_last_error(dw_err, "ReleaseMutex") +
                             GMUTEX_SUFFIX);
    }
#else
    int ret = pthread_mutex_unlock(mutex_);
    if (ret) {
      return std::unexpected(Io::errno_error(ret, "pthread_mutex_unlock") +
                             GMUTEX_SUFFIX);
    }
#endif

    return {};
  }

  bool valid() const noexcept {
    return mutex_ != nullptr;
  }

 private:
#if defined(_WIN32) || defined(_WIN64)
  HANDLE mutex_ = nullptr;
#else
  pthread_mutex_t *mutex_ = nullptr;
#endif

  auto T_lock(bool is_trylock = false)
    -> std::expected<LockState, std::string> {
    std::string es;

#if defined(_WIN32) || defined(_WIN64)
    DWORD dw_err = ERROR_SUCCESS;

    DWORD wait_ms = is_trylock ? 0 : INFINITE;

    static const std::string kFunc =
      std::format("WaitForSingleObject (trylock: {0})", is_trylock);

    DWORD wait_ret = WaitForSingleObject(mutex_, wait_ms);
    switch (wait_ret) {
    [[likely]] case WAIT_OBJECT_0:
      return LockState::kSuccess;
    case WAIT_ABANDONED:
      goto label_owner_dead;
    case WAIT_TIMEOUT:
      return LockState::kBusy;
    case WAIT_FAILED:
      dw_err = GetLastError();
      return std::unexpected(Io::win_last_error(dw_err, kFunc) + GMUTEX_SUFFIX);
    default:
      return std::unexpected(Io::io().s_error(SIRIUS_FILE_NAME, __LINE__) +
                             GMUTEX_SUFFIX);
    }
#else
    int (*lock_ptr)(pthread_mutex_t *) =
      is_trylock ? pthread_mutex_trylock : pthread_mutex_lock;

    static const std::string kFunc =
      std::format("lock_ptr (trylock: {0})", is_trylock);

    int ret = lock_ptr(mutex_);
    switch (ret) {
    [[likely]] case 0:
      return LockState::kSuccess;
    case EOWNERDEAD:
      pthread_mutex_consistent(mutex_);
      goto label_owner_dead;
    case EBUSY:
      return LockState::kBusy;
    case ENOTRECOVERABLE:
    default:
      return std::unexpected(Io::errno_error(ret, kFunc) + GMUTEX_SUFFIX);
    }

    return LockState::kSuccess;
#endif

  label_owner_dead:
    es = Io::io().s_warn("") + Io::row("PID: {0}", pid()) +
      Io::row("The process that previously held the lock may have crashed\n");
    UTILS_WRITE(STDERR_FILENO, es.c_str(), es.size());

    return LockState::kOwnerDead;
  }

#undef GMUTEX_SUFFIX
};
} // namespace Process
} // namespace Utils
