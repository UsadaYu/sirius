#pragma once

#ifndef UTILS_LOG_SHM_HPP_IS_DAEMON_
#  error "This header file can only be included by the log module"
#endif

#include "utils/log/utils.hpp"
#include "utils/process.hpp"
#include "utils/time.hpp"

#if defined(_WIN32) || defined(_WIN64)
#else
#  include <sys/mman.h>
#endif

#include <functional>
#include <future>
#include <thread>

/**
 * @note The use of `std::hardware_destructive_interference_size` is waived
 * here.
 */
inline constexpr size_t kShmCacheLineSize = 64;

namespace Utils {
namespace Log {
namespace Shm {
enum class SlotState : int {
  FREE = 0,
  WRITING = 1,
  READY = 2,
};

enum class FsState : int {
  None = 0, // Make no changes.
  Std = 1,  // Set to default `stdout` / `stderr`.
  File = 2, // Write to file.
};

enum class DataType : int {
  Log = 0,    // likely
  Config = 1, // unlikely
};

struct Buffer {
  DataType type;

  union {
    struct {
      size_t buf_size;
      char buf[Log::kLogBufferSize];
    } log;

    struct {
      char path[Log::kLogPathMax];
      FsState state;
    } fs;
  } data;

  /**
   * @note
   * - (1) level <= SIRIUS_LOG_LEVEL_WARN: stderr.
   *
   * - (2) level > SIRIUS_LOG_LEVEL_WARN: stdout.
   */
  int level;
};

#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable: 4324)
#endif
struct alignas(kShmCacheLineSize) Slot {
  std::atomic<SlotState> state;
  std::atomic<uint64_t> timestamp_ms;

  Buffer buffer;
};
#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

struct AttachedMap {
#if defined(_WIN32) || defined(_WIN64)
  DWORD
#else
  pid_t
#endif
  pid;

  uint64_t timestamp_ms;
};

struct Header {
  static constexpr uint32_t kMagic = 0xDEADBEEF;

  /**
   * @note
   * - (1) This variable should be guarded by the process lock.
   *
   * - (2) Filled by the creator of the shared memory.
   */
  uint32_t magic;

  std::atomic<bool> is_daemon_ready;

  /**
   * @note
   * - (1) The parameters about `attached` should be guarded by the shm lock.
   *
   * - (2) `attached_count` may need to be guarded by both shm lock and process
   * lock.
   */
  size_t attached_count;
  bool attached_used[Log::kProcessMax];
  AttachedMap attached_maps[Log::kProcessMax];

  std::atomic<uint64_t> write_index;
  std::atomic<uint64_t> read_index;
  size_t capacity;
  size_t item_size;

  uint64_t reserved[8];
};

namespace GMutex {
static Process::Mutex *process_mutex_ = nullptr;

static inline bool create() {
  process_mutex_ = new Process::Mutex(Log::kMutexProcessKey);

  return process_mutex_->valid();
}

static inline void destroy() {
  delete process_mutex_;
  process_mutex_ = nullptr;
}

static inline bool lock() {
  return process_mutex_ ? process_mutex_->lock(utils_pretty_function) : false;
}

static inline bool unlock() {
  return process_mutex_ ? process_mutex_->unlock() : false;
}
} // namespace GMutex

class Shm {
 public:
  bool shm_creator = false;
  Header *header = nullptr;
  Slot *slots = nullptr;

#if defined(_WIN32) || defined(_WIN64)
  Shm()
      : shm_name_(Ns::get_shm_name(Log::kKey)),
        shm_handle_(nullptr),
        mapped_size_(0) {}
#else
  Shm()
      : shm_name_(Ns::get_shm_name(Log::kKey)), shm_fd_(-1), mapped_size_(0) {}
#endif

  ~Shm() {}

// --- Shared Memory ---
#if defined(_WIN32) || defined(_WIN64)
  bool open_shm() {
    std::string es;
    size_t size_needed = get_shm_size();
    DWORD dw_err;

    DWORD size_high = (DWORD)((uint64_t)size_needed >> 32);
    DWORD size_low = (DWORD)((uint64_t)size_needed & 0xFFFFFFFF);

    shm_handle_ =
      CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                         size_high, size_low, shm_name_.c_str());
    dw_err = GetLastError();
    if (shm_handle_) {
      shm_creator = dw_err == ERROR_ALREADY_EXISTS ? false : true;
    } else {
      es = std::format("{} -> `CreateFileMappingA`", utils_pretty_function);
      utils_win_last_error(dw_err, es.c_str());
      return false;
    }

    mapped_size_ = size_needed;

    shm_mutex_ = Process::Mutex(Log::kMutexShmKey);
    if (!shm_mutex_.valid()) {
      CloseHandle(shm_handle_);
      shm_handle_ = nullptr;
      return false;
    }

    return true;
  }

  void close_shm(bool shm_should_destroy) {
    (void)shm_should_destroy;

    shm_mutex_.destroy();

    if (shm_handle_) {
      CloseHandle(shm_handle_);
      shm_handle_ = nullptr;
    }
  }

  bool memory_map() {
    std::string es;

    void *ptr =
      MapViewOfFile(shm_handle_, FILE_MAP_ALL_ACCESS, 0, 0, mapped_size_);
    if (!ptr) {
      const DWORD dw_err = GetLastError();
      es = std::format("{} -> `MapViewOfFile`", utils_pretty_function);
      utils_win_last_error(dw_err, es.c_str());
      return false;
    }
    header = static_cast<Header *>(ptr);

    return true;
  }

  void memory_unmap() {
    if (header) {
      UnmapViewOfFile(header);
      header = nullptr;
    }
  }
#else
  bool open_shm() {
    int errno_err = 0;
    std::string es;
    size_t size_needed = get_shm_size();

    shm_fd_ = shm_open(shm_name_.c_str(), O_CREAT | O_EXCL | O_RDWR,
                       File::string_to_mode(_SIRIUS_POSIX_FILE_MODE));
    errno_err = errno;
    if (shm_fd_ >= 0) {
      shm_creator = true;
      if (ftruncate(shm_fd_, size_needed) == -1) {
        errno_err = errno;
        es = std::format("{} -> `ftruncate`", utils_pretty_function);
        utils_errno_error(errno_err, es.c_str());
        shm_unlink(shm_name_.c_str());
        close(shm_fd_);
        return false;
      }
    } else if (errno_err == EEXIST) {
      shm_creator = false;
      shm_fd_ = shm_open(shm_name_.c_str(), O_RDWR, 0);
      if (shm_fd_ < 0) {
        errno_err = errno;
        es = std::format("{} -> `shm_open` attach", utils_pretty_function);
        utils_errno_error(errno_err, es.c_str());
        return false;
      }
    } else {
      es = std::format("{} -> `shm_open` open", utils_pretty_function);
      utils_errno_error(errno_err, es.c_str());
      return false;
    }

    mapped_size_ = size_needed;

    return true;
  }

  void close_shm(bool shm_should_destroy) {
    if (shm_fd_ != -1) {
      close(shm_fd_);
      shm_fd_ = -1;
    }

    if (shm_should_destroy) {
      utils_dprintf(STDOUT_FILENO,
                    LOG_LEVEL_STR_INFO "%sUnlink the share memory\n",
                    Log::kCommon);
      shm_unlink(shm_name_.c_str());
    }
  }

  bool memory_map() {
    std::string es;

    void *ptr = mmap(nullptr, mapped_size_, PROT_READ | PROT_WRITE, MAP_SHARED,
                     shm_fd_, 0);
    if (ptr == MAP_FAILED) {
      const int errno_err = errno;
      es = std::format("{} -> `mmap`", utils_pretty_function);
      utils_errno_error(errno_err, es.c_str());
      return false;
    }
    header = static_cast<ShmHeader::Header *>(ptr);

    return true;
  }

  void memory_unmap() {
    if (header) {
      munmap(header, mapped_size_);
      header = nullptr;
    }
  }
#endif

// --- LockGuard ---
#if defined(_WIN32) || defined(_WIN64)
  class LockGuard {
   public:
    explicit LockGuard(Process::Mutex &mutex) : mutex_(mutex) {
      mutex_.lock(utils_pretty_function);
    }
    ~LockGuard() {
      mutex_.unlock();
    }
    LockGuard(const LockGuard &) = delete;
    LockGuard &operator=(const LockGuard &) = delete;

   private:
    Process::Mutex &mutex_;
  };

  LockGuard lock_guard() {
    return LockGuard(shm_mutex_);
  }
#else
#endif

  void slots_deinit() {
    slot_free();
    slots = nullptr;
  }

  bool slots_init() {
    uint8_t *base_ptr = reinterpret_cast<uint8_t *>(header);

    slots = reinterpret_cast<Slot *>(base_ptr + get_shm_header_offset());

    if (!shm_creator) {
      if (header->magic != Header::Header::kMagic) {
        utils_dprintf(STDERR_FILENO,
                      LOG_LEVEL_STR_ERROR "%sInvalid argument. `magic`: %u\n",
                      Log::kCommon, header->magic);
        return false;
      }
      return slot_alloc();
    }

    header->is_daemon_ready.store(false);
    header->attached_count = 0;
    for (size_t i = 0; i < Log::kProcessMax; ++i) {
      header->attached_used[i] = false;
    }
    header->write_index.store(0);
    header->read_index.store(0);
    header->capacity = Log::kShmCapacity;
    header->item_size = sizeof(Slot);

    if (slot_alloc()) {
      header->magic = Header::kMagic;
      return true;
    }

    return false;
  }

 private:
  std::string shm_name_;
#if defined(_WIN32) || defined(_WIN64)
  HANDLE shm_handle_;
  Process::Mutex shm_mutex_;
#else
  int shm_fd_;
#endif
  size_t mapped_size_;
  std::jthread thread_guard_;

  size_t get_shm_header_offset() const {
    size_t header_size = sizeof(Header);
    size_t header_aligned =
      (header_size + kShmCacheLineSize - 1) & ~(kShmCacheLineSize - 1);

    return header_aligned;
  }

  size_t get_shm_size() const {
    return get_shm_header_offset() + (Log::kShmCapacity * sizeof(Slot));
  }

  void attached_count_sub1() {
    header->attached_count =
      header->attached_count ? header->attached_count - 1 : 0;
  }

  bool attached_check(size_t index) {
    return (header->attached_used[index] &&
            header->attached_maps[index].pid == Process::id());
  }

#if UTILS_LOG_SHM_HPP_IS_DAEMON_
  void thread_guard(std::stop_token stop_token) {
    constexpr int kOnceSleepMs = 1000;
    int splits = Log::kProcessGuardTimeoutMilliseconds / kOnceSleepMs;
    splits = UTILS_MAX(1, splits);

    while (!stop_token.stop_requested()) {
      for (int i = 0; i < splits; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kOnceSleepMs));
        if (stop_token.stop_requested()) [[unlikely]]
          return;
      }

      uint64_t timestamp_ms = Time::get_monotonic_steady_ms();

      {
        lock_guard();

        for (size_t i = 0; i < Log::kProcessMax; ++i) {
          if (!header->attached_used[i])
            continue;

          AttachedMap *am = header->attached_maps + i;
          if (timestamp_ms - am->timestamp_ms <
              Log::kProcessGuardTimeoutMilliseconds) [[likely]] {
            continue;
          }

          std::memset(am, 0, sizeof(AttachedMap));
          header->attached_used[i] = false;
          attached_count_sub1();
        }

        if (header->attached_count == 0)
          break;
      }
    }
  }

  void slot_free() {
    thread_guard_.request_stop();
    if (thread_guard_.joinable()) {
      thread_guard_.join();
    }

    utils_dprintf(STDOUT_FILENO,
                  LOG_LEVEL_STR_INFO "%sPID: %" PRIu64 ". Slot free\n",
                  Log::kDaemon, (uint64_t)Process::id());
  }

  bool slot_alloc() {
    if (header->is_daemon_ready.load(std::memory_order_relaxed)) {
      utils_dprintf(STDOUT_FILENO,
                    LOG_LEVEL_STR_WARN "%sAnother daemon already exists\n",
                    Log::kDaemon);
      return false;
    }

    thread_guard_ = std::jthread(std::bind_front(&Shm::thread_guard, this));

    utils_dprintf(STDOUT_FILENO,
                  LOG_LEVEL_STR_INFO "%sPID: %" PRIu64 ". Slot alloc\n",
                  Log::kDaemon, (uint64_t)Process::id());

    return true;
  }
#else
  void thread_guard(std::stop_token stop_token, std::promise<void> promise) {
    size_t index = 0;

    {
      lock_guard();

      while (!attached_check(index)) {
        if (index == Log::kProcessMax - 1) {
          utils_dprintf(STDERR_FILENO,
                        LOG_LEVEL_STR_WARN "%sPID: %" PRIu64
                                           ". No valid pid was matched\n",
                        Log::kNative, (uint64_t)Process::id());
          return;
        }
        ++index;
      }
    }

    /**
     * @note Although the probability of occurrence is not high, the
     * initialization function may be called repeatedly. Slightly shortening the
     * duration of a single sleep can help threads exit quickly.
     */
    constexpr int kOnceSleepMs = 1000;
    int splits = Log::kProcessFeedGuardMilliseconds / kOnceSleepMs;
    splits = UTILS_MAX(1, splits);

    promise.set_value();

    while (!stop_token.stop_requested()) {
      uint64_t timestamp_ms = Time::get_monotonic_steady_ms();

      {
        lock_guard();

        if (attached_check(index)) [[likely]] {
          header->attached_maps[index].timestamp_ms = timestamp_ms;
        } else {
          utils_dprintf(STDERR_FILENO,
                        LOG_LEVEL_STR_WARN "%sInvalid argument. Pid: %" PRIu64
                                           ". `attached` thread exit\n",
                        Log::kNative, (uint64_t)Process::id());
          return;
        }
      }

      for (int i = 0; i < splits; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kOnceSleepMs));
        if (stop_token.stop_requested())
          return;
      }
    }
  }

  void slot_free() {
    thread_guard_.request_stop();
    if (thread_guard_.joinable()) {
      thread_guard_.join();
    }

    {
      lock_guard();

      for (size_t i = 0; i < Log::kProcessMax; ++i) {
        auto *am = header->attached_maps + i;
        if (header->attached_used[i] && am->pid == Process::id()) {
          std::memset(am, 0, sizeof(AttachedMap));
          header->attached_used[i] = false;
          attached_count_sub1();
          break;
        }
        if (i == Log::kProcessMax - 1) {
          utils_dprintf(STDERR_FILENO,
                        LOG_LEVEL_STR_WARN "%sPID: %" PRIu64
                                           ". No valid pid was matched\n",
                        Log::kNative, (uint64_t)Process::id());
        }
      }
    }

    utils_dprintf(STDOUT_FILENO,
                  LOG_LEVEL_STR_INFO "%sPID: %" PRIu64 ". Slot free\n",
                  Log::kNative, (uint64_t)Process::id());
  }

  bool slot_alloc() {
    {
      lock_guard();

      for (size_t i = 0; i < Log::kProcessMax; ++i) {
        if (!header->attached_used[i]) {
          ++header->attached_count;
          header->attached_used[i] = true;
          header->attached_maps[i].pid = Process::id();
          header->attached_maps[i].timestamp_ms =
            Time::get_monotonic_steady_ms();
          break;
        }
        if (i == Log::kProcessMax - 1) {
          utils_dprintf(STDERR_FILENO,
                        LOG_LEVEL_STR_ERROR
                        "%sCache full. Too many processes\n",
                        Log::kNative);
          return false;
        }
      }
    }

    std::promise<void> promise_guard;
    std::future<void> future_guard = promise_guard.get_future();
    thread_guard_ = std::jthread([this, promise = std::move(promise_guard)](
                                   std::stop_token stop_token) mutable {
      thread_guard(stop_token, std::move(promise));
    });
    auto wait_ret = future_guard.wait_for(std::chrono::milliseconds(500));
    if (wait_ret != std::future_status::ready) {
      slot_free();
      utils_dprintf(STDOUT_FILENO,
                    LOG_LEVEL_STR_ERROR "%sFail to start `thread_guard`\n",
                    Log::kNative);
      return false;
    }

    utils_dprintf(STDOUT_FILENO,
                  LOG_LEVEL_STR_INFO "%sPID: %" PRIu64 ". Slot alloc\n",
                  Log::kNative, (uint64_t)Process::id());

    return true;
  }
#endif
};
} // namespace Shm
} // namespace Log
} // namespace Utils
