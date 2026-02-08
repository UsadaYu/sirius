#pragma once

#include "utils/log.h"
#include "utils/namespace.hpp"

namespace Utils {
namespace GMutex {
#if defined(_WIN32) || defined(_WIN64)
class Win {
 public:
  explicit Win(const char *name = nullptr) noexcept
      : handle_(nullptr), name_(name ? std::string(name) : "") {
    if (name) {
      create();
    }
  }

  ~Win() noexcept {
    destroy();
  }

  Win(Win &&other) noexcept
      : handle_(other.handle_), name_(std::move(other.name_)) {
    other.handle_ = nullptr;
  }

  Win &operator=(Win &&other) noexcept {
    if (this != &other) {
      destroy();
      handle_ = other.handle_;
      name_ = std::move(other.name_);
      other.handle_ = nullptr;
    }

    return *this;
  }

  Win(const Win &) = delete;
  Win &operator=(const Win &) = delete;

  bool create(const char *mutex_name = nullptr) {
    std::string es;

    if (mutex_name) {
      name_ = mutex_name;
    }

    if (name_.empty())
      return false;

    destroy();

    handle_ = CreateMutexA(nullptr, FALSE, Ns::win_lock_name(name_).c_str());
    if (!handle_) {
      es = std::format("{} -> `CreateMutexA`", utils_pretty_function);
      utils_win_last_error(GetLastError(), es.c_str());
      return false;
    }

    return true;
  }

  void destroy() noexcept {
    if (handle_ != nullptr) {
      CloseHandle(handle_);
      handle_ = nullptr;
    }
  }

  bool lock(std::string msg = "unknown") {
    std::string es = std::format("{} -> {} -> `WaitForSingleObject`", msg,
                                 utils_pretty_function);

    while (true) {
      DWORD dw_err = WaitForSingleObject(handle_, INFINITE);
      switch (dw_err) {
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
        utils_win_last_error(GetLastError(), es.c_str());
        return false;
      default:
        return false;
      }
    }
  }

  bool unlock() {
    if (!ReleaseMutex(handle_)) {
      std::string es =
        std::format("{} -> `ReleaseMutex`", utils_pretty_function);
      utils_win_last_error(GetLastError(), es.c_str());
      return false;
    }

    return true;
  }

  HANDLE get() const noexcept {
    return handle_;
  }

  bool valid() const noexcept {
    return handle_ != nullptr;
  }

 private:
  HANDLE handle_;
  std::string name_;
};
#endif
} // namespace GMutex
} // namespace Utils
