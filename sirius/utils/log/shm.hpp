#pragma once

#ifndef UTILS_LOG_SHM_HPP_IS_DAEMON_
#  error "This header file can only be included by the log module"
#endif

#include "utils/gmutex.hpp"
#include "utils/log/utils.hpp"
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
constexpr size_t SHM_CACHE_LINE_SIZE = 64;

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
      char buf[Log::LOG_BUF_SIZE];
    } log;

    struct {
      char path[Log::LOG_PATH_MAX];
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
struct alignas(SHM_CACHE_LINE_SIZE) Slot {
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
  static constexpr uint32_t MAGIC = 0xDEADBEEF;

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
  bool attached_used[Log::PROCESS_MAX];
  AttachedMap attached_maps[Log::PROCESS_MAX];

  std::atomic<uint64_t> write_index;
  std::atomic<uint64_t> read_index;
  size_t capacity;
  size_t item_size;

  uint64_t reserved[8];
};

namespace ProcessMutex {
#if defined(_WIN32) || defined(_WIN64)
static GMutex::Win *process_lock_;

static inline bool create() {
  process_lock_ = new GMutex::Win(Log::MUTEX_PROCESS_KEY);

  return true;
}

static inline void destory() {
  delete process_lock_;
}

static inline bool lock() {
  return process_lock_->lock("ProcessMutex");
}

static inline bool unlock() {
  return process_lock_->unlock();
}
#else
static int process_lock_ = -1;

static inline bool create() {
  std::string es;

  auto path = Ns::posix_lockfile_path(Log::MUTEX_PROCESS_KEY);
  process_lock_ = open(path.c_str(), O_RDWR | O_CREAT,
                       File::string_to_mode(_SIRIUS_POSIX_FILE_MODE));
  if (process_lock_ == -1) {
    es = std::format("{} -> `open`", utils_pretty_function);
    utils_errno_error(errno, es.c_str());
    return false;
  }

  return true;
}

static inline void destory() {
  if (process_lock_ != -1) {
    close(process_lock_);
    process_lock_ = -1;
  }
}

static inline bool lock() {
  std::string es;
  struct flock lock;
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;

  if (fcntl(process_lock_, F_SETLKW, &lock) == -1) {
    es = std::format("{} -> `fcntl` lock", utils_pretty_function);
    utils_errno_error(errno, es.c_str());
    return false;
  }

  return true;
}

static inline bool unlock() {
  std::string es;
  struct flock lock;
  lock.l_type = F_UNLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;

  if (fcntl(process_lock_, F_SETLK, &lock) == -1) {
    es = std::format("{} -> `fcntl` unlock", utils_pretty_function);
    utils_errno_error(errno, es.c_str());
    return false;
  }

  return true;
}
#endif
} // namespace ProcessMutex

class Shm {
 public:
  bool shm_creator = false;
  Header *header = nullptr;
  Slot *slots = nullptr;

#if defined(_WIN32) || defined(_WIN64)
  Shm()
      : shm_name_(Ns::get_shm_name(Log::KEY)),
        shm_handle_(nullptr),
        mapped_size_(0) {}
#else
  Shm() : shm_name_(Ns::get_shm_name(Log::KEY)), shm_fd_(-1), mapped_size_(0) {}
#endif

  ~Shm() {}

// --- Shared Memory ---
#if defined(_WIN32) || defined(_WIN64)
  bool open_shm() {
    std::string es;
    size_t size_needed = get_shm_size();

    DWORD size_high = (DWORD)((uint64_t)size_needed >> 32);
    DWORD size_low = (DWORD)((uint64_t)size_needed & 0xFFFFFFFF);

    shm_handle_ =
      CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                         size_high, size_low, shm_name_.c_str());
    if (shm_handle_) {
      shm_creator = GetLastError() == ERROR_ALREADY_EXISTS ? false : true;
    } else {
      es = std::format("{} -> `CreateFileMappingA`", utils_pretty_function);
      utils_win_last_error(GetLastError(), es.c_str());
      return false;
    }

    mapped_size_ = size_needed;

    shm_lock_ = GMutex::Win(Log::MUTEX_SHM_KEY);
    if (shm_lock_.get() == nullptr) {
      CloseHandle(shm_handle_);
      shm_handle_ = nullptr;
      return false;
    }

    return true;
  }

  void close_shm(bool shm_should_destory) {
    (void)shm_should_destory;

    shm_lock_.destroy();

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
      es = std::format("{} -> `MapViewOfFile`", utils_pretty_function);
      utils_win_last_error(GetLastError(), es.c_str());
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
    std::string es;
    size_t size_needed = get_shm_size();

    shm_fd_ = shm_open(shm_name_.c_str(), O_CREAT | O_EXCL | O_RDWR,
                       File::string_to_mode(_SIRIUS_POSIX_FILE_MODE));
    if (shm_fd_ >= 0) {
      shm_creator = true;
      if (ftruncate(shm_fd_, size_needed) == -1) {
        es = std::format("{} -> `ftruncate`", utils_pretty_function);
        utils_errno_error(errno, es.c_str());
        shm_unlink(shm_name_.c_str());
        close(shm_fd_);
        return false;
      }
    } else if (errno == EEXIST) {
      shm_creator = false;
      shm_fd_ = shm_open(shm_name_.c_str(), O_RDWR, 0);
      if (shm_fd_ < 0) {
        es = std::format("{} -> `shm_open` attach", utils_pretty_function);
        utils_errno_error(errno, es.c_str());
        return false;
      }
    } else {
      es = std::format("{} -> `shm_open` open", utils_pretty_function);
      utils_errno_error(errno, es.c_str());
      return false;
    }

    mapped_size_ = size_needed;

    return true;
  }

  void close_shm(bool shm_should_destory) {
    if (shm_fd_ != -1) {
      close(shm_fd_);
      shm_fd_ = -1;
    }

    if (shm_should_destory) {
      utils_dprintf(STDOUT_FILENO,
                    LOG_LEVEL_STR_INFO "%sUnlink the share memory\n",
                    Log::COMMON);
      shm_unlink(shm_name_.c_str());
    }
  }

  bool memory_map() {
    std::string es;

    void *ptr = mmap(nullptr, mapped_size_, PROT_READ | PROT_WRITE, MAP_SHARED,
                     shm_fd_, 0);
    if (ptr == MAP_FAILED) {
      es = std::format("{} -> `mmap`", utils_pretty_function);
      utils_errno_error(errno, es.c_str());
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

// --- Mutex ---
#if defined(_WIN32) || defined(_WIN64)
  bool shm_mutex_lock() {
    return shm_lock_.lock("shm_mutex_lock");
  }

  bool shm_mutex_unlock() {
    return shm_lock_.unlock();
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
      if (header->magic != Header::Header::MAGIC) {
        utils_dprintf(STDERR_FILENO,
                      LOG_LEVEL_STR_ERROR "%sInvalid argument. `magic`: %u\n",
                      Log::COMMON, header->magic);
        return false;
      }
      return slot_alloc();
    }

    header->is_daemon_ready.store(false);
    header->attached_count = 0;
    for (size_t i = 0; i < Log::PROCESS_MAX; ++i) {
      header->attached_used[i] = false;
    }
    header->write_index.store(0);
    header->read_index.store(0);
    header->capacity = Log::SHM_CAPACITY;
    header->item_size = sizeof(Slot);

    if (slot_alloc()) {
      header->magic = Header::MAGIC;
      return true;
    }

    return false;
  }

 private:
  std::string shm_name_;
#if defined(_WIN32) || defined(_WIN64)
  HANDLE shm_handle_;
  GMutex::Win shm_lock_;
#else
  int shm_fd_;
#endif
  size_t mapped_size_;
  std::jthread thread_guard_;

  size_t get_shm_header_offset() const {
    size_t header_size = sizeof(Header);
    size_t header_aligned =
      (header_size + SHM_CACHE_LINE_SIZE - 1) & ~(SHM_CACHE_LINE_SIZE - 1);

    return header_aligned;
  }

  size_t get_shm_size() const {
    return get_shm_header_offset() + (Log::SHM_CAPACITY * sizeof(Slot));
  }

  auto get_process_id() const {
#if defined(_WIN32) || defined(_WIN64)
    static auto pid = GetCurrentProcessId();
#else
    static auto pid = getpid();
#endif

    return pid;
  }

  void attached_count_sub1() {
    header->attached_count =
      header->attached_count ? header->attached_count - 1 : 0;
  }

  bool attached_check(size_t index) {
    auto pid = get_process_id();

    return (header->attached_used[index] &&
            header->attached_maps[index].pid == pid);
  }

#if UTILS_LOG_SHM_HPP_IS_DAEMON_
  void thread_guard(std::stop_token stop_token) {
    const int once_ms = 1000;
    int splits = Log::PROCESS_GUARD_TIMEOUT_MS / once_ms;
    splits = UTILS_MAX(1, splits);

    while (!stop_token.stop_requested()) {
      for (int i = 0; i < splits; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(once_ms));
        if (stop_token.stop_requested()) [[unlikely]]
          return;
      }

      uint64_t timestamp_ms = Time::get_monotonic_steady_ms();

      shm_mutex_lock();

      for (size_t i = 0; i < Log::PROCESS_MAX; ++i) {
        if (!header->attached_used[i])
          continue;

        AttachedMap *am = header->attached_maps + i;
        if (timestamp_ms - am->timestamp_ms < Log::PROCESS_GUARD_TIMEOUT_MS)
          [[likely]] {
          continue;
        }

        std::memset(am, 0, sizeof(AttachedMap));
        header->attached_used[i] = false;
        attached_count_sub1();
      }

      if (header->attached_count == 0) {
        shm_mutex_unlock();
        break;
      }

      shm_mutex_unlock();
    }
  }

  void slot_free() {
    auto pid = get_process_id();

    thread_guard_.request_stop();
    if (thread_guard_.joinable()) {
      thread_guard_.join();
    }

    utils_dprintf(STDOUT_FILENO,
                  LOG_LEVEL_STR_INFO "%sPID: %" PRIu64 ". Slot free\n",
                  Log::DAEMON, (uint64_t)pid);
  }

  bool slot_alloc() {
    auto pid = get_process_id();

    if (header->is_daemon_ready.load(std::memory_order_relaxed)) {
      utils_dprintf(STDOUT_FILENO,
                    LOG_LEVEL_STR_WARN "%sAnother daemon already exists\n",
                    Log::DAEMON);
      return false;
    }

    thread_guard_ = std::jthread(std::bind_front(&Shm::thread_guard, this));

    utils_dprintf(STDOUT_FILENO,
                  LOG_LEVEL_STR_INFO "%sPID: %" PRIu64 ". Slot alloc\n",
                  Log::DAEMON, (uint64_t)pid);

    return true;
  }
#else
  void thread_guard(std::stop_token stop_token, std::promise<void> promise) {
    size_t index = 0;
    auto pid = get_process_id();

    shm_mutex_lock();

    while (!attached_check(index)) {
      if (index == Log::PROCESS_MAX - 1) {
        utils_dprintf(STDERR_FILENO,
                      LOG_LEVEL_STR_WARN "%sPID: %" PRIu64
                                         ". No valid pid was matched\n",
                      Log::NATIVE, (uint64_t)pid);
        shm_mutex_unlock();
        return;
      }
      ++index;
    }

    shm_mutex_unlock();

    /**
     * @note Although the probability of occurrence is not high, the
     * initialization function may be called repeatedly. Slightly shortening the
     * duration of a single sleep can help threads exit quickly.
     */
    const int once_ms = 500;
    int splits = Log::PROCESS_FEED_GUARD_MS / once_ms;
    splits = UTILS_MAX(1, splits);

    promise.set_value();

    while (!stop_token.stop_requested()) {
      uint64_t timestamp_ms = Time::get_monotonic_steady_ms();

      shm_mutex_lock();

      if (attached_check(index)) [[likely]] {
        header->attached_maps[index].timestamp_ms = timestamp_ms;
      } else {
        utils_dprintf(STDERR_FILENO,
                      LOG_LEVEL_STR_WARN "%sInvalid argument. Pid: %" PRIu64
                                         ". `attached` thread exit\n",
                      Log::NATIVE, (uint64_t)pid);
        shm_mutex_unlock();
        return;
      }

      shm_mutex_unlock();

      for (int i = 0; i < splits; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(once_ms));
        if (stop_token.stop_requested())
          return;
      }
    }
  }

  void slot_free() {
    auto pid = get_process_id();

    thread_guard_.request_stop();
    if (thread_guard_.joinable()) {
      thread_guard_.join();
    }

    shm_mutex_lock();

    for (size_t i = 0; i < Log::PROCESS_MAX; ++i) {
      auto *am = header->attached_maps + i;
      if (header->attached_used[i] && am->pid == pid) {
        std::memset(am, 0, sizeof(AttachedMap));
        header->attached_used[i] = false;
        attached_count_sub1();
        break;
      }
      if (i == Log::PROCESS_MAX - 1) {
        utils_dprintf(STDERR_FILENO,
                      LOG_LEVEL_STR_WARN "%sPID: %" PRIu64
                                         ". No valid pid was matched\n",
                      Log::NATIVE, (uint64_t)pid);
      }
    }

    shm_mutex_unlock();

    utils_dprintf(STDOUT_FILENO,
                  LOG_LEVEL_STR_INFO "%sPID: %" PRIu64 ". Slot free\n",
                  Log::NATIVE, (uint64_t)pid);
  }

  bool slot_alloc() {
    auto pid = get_process_id();

    shm_mutex_lock();

    for (size_t i = 0; i < Log::PROCESS_MAX; ++i) {
      if (!header->attached_used[i]) {
        ++header->attached_count;
        header->attached_used[i] = true;
        header->attached_maps[i].pid = pid;
        header->attached_maps[i].timestamp_ms = Time::get_monotonic_steady_ms();
        shm_mutex_unlock();
        break;
      }
      if (i == Log::PROCESS_MAX - 1) {
        utils_dprintf(STDERR_FILENO,
                      LOG_LEVEL_STR_ERROR "%sCache full. Too many processes\n",
                      Log::NATIVE);
        shm_mutex_unlock();
        return false;
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
                    Log::NATIVE);
      return false;
    }

    utils_dprintf(STDOUT_FILENO,
                  LOG_LEVEL_STR_INFO "%sPID: %" PRIu64 ". Slot alloc\n",
                  Log::NATIVE, (uint64_t)pid);

    return true;
  }
#endif
};
} // namespace Shm
} // namespace Log
} // namespace Utils
