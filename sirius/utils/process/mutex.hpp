#pragma once

#include "utils/log.h"
#include "utils/namespace.hpp"
#include "utils/process/process.hpp"

namespace Utils {
namespace Process {
// class Fmutex {
//  public:
//   explicit Mutex(const char *name = nullptr) noexcept
//       :
// #if defined(_WIN32) || defined(_WIN64)
//         handle_(nullptr),
// #else
//         fd_(-1),
// #endif
//         name_(name ? std::string(name) : "") {
//     if (name) {
//       create();
//     }
//   }

//   ~Mutex() noexcept {
//     destroy();
//   }
// };

class GMutex {
 public:
  explicit GMutex(const char *name = nullptr) noexcept
      :
#if defined(_WIN32) || defined(_WIN64)
        handle_(nullptr),
#else
        fd_(-1),
#endif
        name_(name ? std::string(name) : "") {
    if (name) {
      create();
    }
  }

  ~GMutex() noexcept {
    destroy();
  }

  GMutex(GMutex &&other) noexcept : name_(std::move(other.name_)) {
#if defined(_WIN32) || defined(_WIN64)
    handle_ = other.handle_;
    other.handle_ = nullptr;
#else
    fd_ = other.fd_;
    other.fd_ = -1;
#endif
  }

  GMutex &operator=(GMutex &&other) noexcept {
    if (this != &other) {
      destroy();
      name_ = std::move(other.name_);
#if defined(_WIN32) || defined(_WIN64)
      handle_ = other.handle_;
      other.handle_ = nullptr;
#else
      fd_ = other.fd_;
      other.fd_ = -1;
#endif
    }

    return *this;
  }

  GMutex(const GMutex &) = delete;
  GMutex &operator=(const GMutex &) = delete;

  bool create(const char *mutex_name = nullptr) {
    std::string es;

    if (mutex_name) {
      name_ = mutex_name;
    }

    if (name_.empty())
      return false;

    destroy();

#if defined(_WIN32) || defined(_WIN64)
    handle_ =
      CreateMutexA(nullptr, FALSE, Ns::Mutex::win_generate_name(name_).c_str());
    if (!handle_) {
      const DWORD dw_err = GetLastError();
      es = std::format("{} -> `CreateMutexA`", utils_pretty_function);
      utils_win_last_error(dw_err, es.c_str());
      return false;
    }
#else
    auto path = Ns::Mutex::file_lock_path(name_);
    fd_ = open(path.c_str(), O_RDWR | O_CREAT,
               File::string_to_mode(_SIRIUS_POSIX_FILE_MODE));
    if (fd_ == -1) {
      const int errno_err = errno;
      es = std::format("{} -> `open`", utils_pretty_function);
      utils_errno_error(errno_err, es.c_str());
      return false;
    }
#endif

    return true;
  }

  void destroy() noexcept {
#if defined(_WIN32) || defined(_WIN64)
    if (handle_ != nullptr) {
      CloseHandle(handle_);
      handle_ = nullptr;
    }
#else
    if (fd_ != -1) {
      close(fd_);
      fd_ = -1;
    }
#endif
  }

  bool lock(std::string msg = "unknown") {
    std::string es;

#if defined(_WIN32) || defined(_WIN64)
    es = std::format("{} -> {} -> `WaitForSingleObject`", msg,
                     utils_pretty_function);

    DWORD dw_err = ERROR_SUCCESS;

    while (true) {
      DWORD wait_ret = WaitForSingleObject(handle_, INFINITE);
      switch (wait_ret) {
      [[likely]] case WAIT_OBJECT_0:
        return true;
      case WAIT_ABANDONED:
        utils_dprintf(
          STDERR_FILENO,
          "--------------------------------\n"
          "- Warning: %s\n"
          "- A process that previously held the lock may have crashed\n"
          "--------------------------------\n",
          es.c_str());
        continue;
      case WAIT_FAILED:
        dw_err = GetLastError();
        utils_win_last_error(dw_err, es.c_str());
        return false;
      default:
        return false;
      }
    }
#else
    struct flock lock_ptr;
    lock_ptr.l_type = F_WRLCK;
    lock_ptr.l_whence = SEEK_SET;
    lock_ptr.l_start = 0;
    lock_ptr.l_len = 0;

    if (fcntl(fd_, F_SETLKW, &lock_ptr) == -1) {
      const int errno_err = errno;
      es = std::format("{} -> {} -> `fcntl` lock", msg, utils_pretty_function);
      utils_errno_error(errno_err, es.c_str());
      return false;
    }

    return true;
#endif
  }

  bool unlock() {
    std::string es;

#if defined(_WIN32) || defined(_WIN64)
    if (!ReleaseMutex(handle_)) {
      const DWORD dw_err = GetLastError();
      es = std::format("{} -> `ReleaseMutex`", utils_pretty_function);
      utils_win_last_error(dw_err, es.c_str());
      return false;
    }
#else
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
#if defined(_WIN32) || defined(_WIN64)
    return handle_ != nullptr;
#else
    return fd_ != -1;
#endif
  }

 private:
#if defined(_WIN32) || defined(_WIN64)
  HANDLE handle_;
#else
  int fd_;
#endif
  std::string name_;
};
} // namespace Process
} // namespace Utils
