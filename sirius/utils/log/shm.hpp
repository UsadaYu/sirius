#pragma once

#include "utils/log/utils.hpp"
#include "utils/process/mutex.hpp"
#include "utils/time.hpp"

#if defined(_WIN32) || defined(_WIN64)
#else
#  include <sys/mman.h>
#endif

#include <functional>
#include <thread>

namespace Utils {
namespace Log {
namespace Shm {
/**
 * @note The use of `std::hardware_destructive_interference_size` is waived
 * here.
 */
inline constexpr size_t kShmCacheLineSize = 64;

enum class MasterType : int {
  kNone = 0,
  kDaemon = 1,
  kNative = 2,
};

enum class SlotState : int {
  kFree = 0,
  kWaiting = 1,
  kReady = 2,
};

enum class FsState : int {
  kNone = 0, // Make no changes.
  kStd = 1,  // Set to default `stdout` / `stderr`.
  kFile = 2, // Write to file.
};

enum class DataType : int {
  kLog = 0,    // likely
  kConfig = 1, // unlikely
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
   * - (1) This variable should be guarded by the process mutex.
   *
   * - (2) Filled by the creator of the shared memory.
   */
  uint32_t magic;

  std::atomic<bool> is_daemon_ready;

  /**
   * @note
   * - (1) The parameters about `attached` should be guarded by the shm mutex.
   *
   * - (2) `attached_count` may need to be guarded by both shm mutex and process
   * lock.
   */
  size_t attached_count;
  enum MasterType attached_master_type[Log::kProcessMax];
  AttachedMap attached_maps[Log::kProcessMax];

  std::atomic<uint64_t> write_index;
  std::atomic<uint64_t> read_index;
  size_t capacity;
  size_t item_size;

#if !defined(_WIN32) && !defined(_WIN64)
  pthread_mutex_t shm_mutex;
#endif

  uint64_t reserved[8];
};

namespace GMutex {
#if defined(_WIN32) || defined(_WIN64)
#  define MUTEX_TYPE_ Process::GMutex
#else
#  define MUTEX_TYPE_ Process::FMutex
#endif

static MUTEX_TYPE_ *process_mutex_ = nullptr;

static inline bool create() {
  process_mutex_ = new MUTEX_TYPE_(Log::kMutexProcessKey);

  return process_mutex_->valid();
}

#undef MUTEX_TYPE_

static inline void destroy() {
  delete process_mutex_;
  process_mutex_ = nullptr;
}

static inline bool lock() {
  if (!process_mutex_)
    return false;

  switch (process_mutex_->lock()) {
  case Process::LockErrno::kSuccess:
  case Process::LockErrno::kOwnerDead:
    return true;
  default:
    return false;
  }
}

static inline bool unlock() {
  return process_mutex_ ? process_mutex_->unlock() : false;
}
} // namespace GMutex

class Shm {
 public:
  Header *header = nullptr;
  Slot *slots = nullptr;

  Shm(MasterType master_type)
      :
#if defined(_WIN32) || defined(_WIN64)
        shm_handle_(nullptr),
#else
        shm_fd_(-1),
        destructor_counter_(0),
#endif
        is_shm_creator_(false),
        master_type_(master_type),
        shm_name_(Ns::Shm::generate_name(Log::kKey)),
        mapped_size_(0),
        master_(nullptr) {
  }

  ~Shm() = default;

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
      is_shm_creator_ = dw_err == ERROR_ALREADY_EXISTS ? false : true;
    } else {
      es = std::format("{0}\n  {1} -> `CreateFileMappingA`", master_string(),
                       utils_pretty_function);
      utils_win_last_error(dw_err, es.c_str());
      return false;
    }

    mapped_size_ = size_needed;

    return true;
  }

  void close_shm() {
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
      es = std::format("{0}\n  {1} -> `MapViewOfFile`", master_string(),
                       utils_pretty_function);
      utils_win_last_error(dw_err, es.c_str());
      return false;
    }
    header = static_cast<Header *>(ptr);

    shm_mutex_ = Process::GMutex(Log::kMutexShmKey);
    if (!shm_mutex_.valid())
      goto label_free1;

    return true;

  label_free1:
    UnmapViewOfFile(header);
    header = nullptr;

    return false;
  }

  void memory_unmap() {
    shm_mutex_.destroy();

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
      is_shm_creator_ = true;
      if (ftruncate(shm_fd_, size_needed) == -1) {
        errno_err = errno;
        es = std::format("{0}\n  {1} -> `ftruncate`", master_string(),
                         utils_pretty_function);
        utils_errno_error(errno_err, es.c_str());
        shm_unlink(shm_name_.c_str());
        close(shm_fd_);
        return false;
      }
    } else if (errno_err == EEXIST) {
      is_shm_creator_ = false;
      shm_fd_ = shm_open(shm_name_.c_str(), O_RDWR, 0);
      if (shm_fd_ < 0) {
        errno_err = errno;
        es = std::format("{0}\n  {1} -> `shm_open` attach", master_string(),
                         utils_pretty_function);
        utils_errno_error(errno_err, es.c_str());
        return false;
      }
    } else {
      es = std::format("{0}\n  {1} -> `shm_open` open", master_string(),
                       utils_pretty_function);
      utils_errno_error(errno_err, es.c_str());
      return false;
    }

    mapped_size_ = size_needed;

    return true;
  }

  void close_shm() {
    if (shm_fd_ != -1) {
      close(shm_fd_);
      shm_fd_ = -1;
    }

    if (destructor_counter_ == 0) {
      utils_dprintf(STDOUT_FILENO,
                    LOG_LEVEL_STR_INFO "%sUnlink the share memory\n",
                    master_string().c_str());
      shm_unlink(shm_name_.c_str());
    }
  }

  bool memory_map() {
    std::string es;

    void *ptr = mmap(nullptr, mapped_size_, PROT_READ | PROT_WRITE, MAP_SHARED,
                     shm_fd_, 0);
    if (ptr == MAP_FAILED) {
      const int errno_err = errno;
      es = std::format("{0}\n  {1} -> `mmap`", master_string(),
                       utils_pretty_function);
      utils_errno_error(errno_err, es.c_str());
      return false;
    }
    header = static_cast<Header *>(ptr);

    shm_mutex_ = Process::GMutex(&header->shm_mutex);
    if ((is_shm_creator_ && !shm_mutex_.create()) || !shm_mutex_.valid())
      goto label_free1;

    return true;

  label_free1:
    munmap(header, mapped_size_);
    header = nullptr;

    return false;
  }

  void memory_unmap() {
    if (!header)
      return;

    {
      lock_guard();

      destructor_counter_ = header->attached_count;
    }

    if (destructor_counter_ == 0) {
      shm_mutex_.destroy();
    }

    munmap(header, mapped_size_);
    header = nullptr;
  }
#endif

  void slots_deinit() {
    if (master_) {
      master_->slot_free();
      slots = nullptr;

      delete master_;
      master_ = nullptr;
    }
  }

  bool slots_init() {
    uint8_t *base_ptr = reinterpret_cast<uint8_t *>(header);

    slots = reinterpret_cast<Slot *>(base_ptr + get_shm_header_offset());

    master_ = new Master(*this, master_type_);

    if (!is_shm_creator_) {
      if (header->magic != Header::Header::kMagic) {
        utils_dprintf(STDERR_FILENO,
                      LOG_LEVEL_STR_ERROR "%sInvalid argument. `magic`: %u\n",
                      master_string().c_str(), header->magic);
        return false;
      }

      return master_->slot_alloc();
    }

    header->is_daemon_ready.store(false);
    header->attached_count = 0;
    for (size_t i = 0; i < Log::kProcessMax; ++i) {
      header->attached_master_type[i] = MasterType::kNone;
    }
    header->write_index.store(0);
    header->read_index.store(0);
    header->capacity = Log::kShmCapacity;
    header->item_size = sizeof(Slot);

    if (master_->slot_alloc()) {
      header->magic = Header::kMagic;
      return true;
    }

    return false;
  }

  bool is_shm_creator() const {
    return is_shm_creator_;
  }

 private:
  // --- LockGuard ---
  class LockGuard {
   public:
    explicit LockGuard(Process::GMutex &mutex)
        : mutex_(mutex), lock_ret_(false) {
      auto ret = mutex_.lock();
      if (ret == Process::LockErrno::kSuccess ||
          ret == Process::LockErrno::kOwnerDead) {
        lock_ret_ = true;
      }
    }

    ~LockGuard() {
      if (lock_ret_) {
        mutex_.unlock();
      }
    }

    LockGuard(const LockGuard &) = delete;
    LockGuard &operator=(const LockGuard &) = delete;

   private:
    Process::GMutex &mutex_;
    bool lock_ret_;
  };

 public:
  LockGuard lock_guard() {
    return LockGuard(shm_mutex_);
  }

 private:
#if defined(_WIN32) || defined(_WIN64)
  HANDLE shm_handle_;
#else
  int shm_fd_;
  size_t destructor_counter_;
#endif
  Process::GMutex shm_mutex_;
  bool is_shm_creator_;
  MasterType master_type_;
  std::string shm_name_;
  size_t mapped_size_;

  std::string master_string() const {
    if (master_type_ == MasterType::kDaemon) {
      static std::string string =
        std::format("{0}[PID {1}] ", Log::kDaemon, Process::pid());
      return string;
    } else if (master_type_ == MasterType::kNative) {
      static std::string string =
        std::format("{0}[PID {1}] ", Log::kNative, Process::pid());
      return string;
    } else {
      static std::string string =
        std::format("{0}[PID {1}] ", Log::kCommon, Process::pid());
      return string;
    }
  }

  size_t get_shm_header_offset() const {
    size_t header_size = sizeof(Header);
    size_t header_aligned =
      (header_size + kShmCacheLineSize - 1) & ~(kShmCacheLineSize - 1);

    return header_aligned;
  }

  size_t get_shm_size() const {
    return get_shm_header_offset() + (Log::kShmCapacity * sizeof(Slot));
  }

  // --- Master ---
  class Master {
   public:
    Master(Shm &parent, MasterType master_type)
        : parent_(parent), header_(parent.header), thread_guard_ready_(false) {}

    ~Master() = default;

    void slot_free() {
      switch (parent_.master_type_) {
      case MasterType::kDaemon:
      case MasterType::kNative:
        return T_slot_free();
      default:
        return;
      }
    }

    bool slot_alloc() {
      switch (parent_.master_type_) {
      case MasterType::kDaemon:
        return daemon_slot_alloc();
      case MasterType::kNative:
        return native_slot_alloc();
      default:
        return false;
      }
    }

   private:
    Shm &parent_;
    Header *&header_;
    std::jthread thread_guard_;
    std::atomic<bool> thread_guard_ready_;

    bool attached_check(size_t index) const {
      return (header_->attached_master_type[index] == parent_.master_type_ &&
              header_->attached_maps[index].pid == Process::pid());
    }

    /**
     * @note This function must be guarded by the shm mutex before it is called
     */
    void attached_count_sub1() {
      header_->attached_count =
        header_->attached_count ? header_->attached_count - 1 : 0;
    }

    void attached_free() {
      parent_.lock_guard();

      for (size_t i = 0; i < Log::kProcessMax; ++i) {
        if (attached_check(i)) {
          std::memset(header_->attached_maps + i, 0, sizeof(AttachedMap));
          header_->attached_master_type[i] = MasterType::kNone;
          attached_count_sub1();
          break;
        }
        if (i == Log::kProcessMax - 1) {
          utils_dprintf(STDERR_FILENO,
                        LOG_LEVEL_STR_WARN "%sNo valid pid was matched\n",
                        parent_.master_string().c_str());
        }
      }
    }

    bool attached_alloc() {
      parent_.lock_guard();

      for (size_t i = 0; i < Log::kProcessMax; ++i) {
        if (header_->attached_master_type[i] == MasterType::kNone) {
          ++header_->attached_count;
          header_->attached_master_type[i] = parent_.master_type_;
          header_->attached_maps[i].pid = Process::pid();
          header_->attached_maps[i].timestamp_ms =
            Time::get_monotonic_steady_ms();
          break;
        }
        if (i == Log::kProcessMax - 1) {
          utils_dprintf(STDERR_FILENO,
                        LOG_LEVEL_STR_ERROR
                        "%sCache full. Too many processes\n",
                        parent_.master_string().c_str());
          return false;
        }
      }

      return true;
    }

    void T_slot_free() {
      thread_guard_.request_stop();
      if (thread_guard_.joinable()) {
        thread_guard_.join();
      }

      attached_free();

      utils_dprintf(STDOUT_FILENO, LOG_LEVEL_STR_INFO "%sSlot free\n",
                    parent_.master_string().c_str());
    }

    bool daemon_slot_alloc() {
      if (header_->is_daemon_ready.load(std::memory_order_relaxed)) {
        utils_dprintf(STDOUT_FILENO,
                      LOG_LEVEL_STR_WARN "%sAnother daemon already exists\n",
                      parent_.master_string().c_str());
        return false;
      }

      if (!attached_alloc())
        return false;

      thread_guard_ =
        std::jthread(std::bind_front(&Shm::Master::daemon_thread_guard, this));

      utils_dprintf(STDOUT_FILENO, LOG_LEVEL_STR_INFO "%sSlot alloc\n",
                    parent_.master_string().c_str());

      return true;
    }

    void daemon_thread_guard(std::stop_token stop_token) {
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
          parent_.lock_guard();

          for (size_t i = 0; i < Log::kProcessMax; ++i) {
            if (header_->attached_master_type[i] != MasterType::kNative) {
              continue;
            }

            AttachedMap *attached_map = header_->attached_maps + i;
            if (timestamp_ms - attached_map->timestamp_ms <
                Log::kProcessGuardTimeoutMilliseconds) [[likely]] {
              continue;
            }

            std::memset(attached_map, 0, sizeof(AttachedMap));
            header_->attached_master_type[i] = MasterType::kNone;
            attached_count_sub1();
          }

          if (header_->attached_count == Log::kProcessNbDaemon)
            break;
        }
      }
    }

    bool native_slot_alloc() {
      if (!attached_alloc())
        return false;

      thread_guard_ =
        std::jthread(std::bind_front(&Shm::Master::native_thread_guard, this));
      constexpr int kRetryTimes = 10;
      for (int i = 0; i < kRetryTimes; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (thread_guard_ready_.load(std::memory_order_relaxed))
          break;
        if (i == kRetryTimes - 1) {
          T_slot_free();
          utils_dprintf(STDOUT_FILENO,
                        LOG_LEVEL_STR_ERROR "%sFail to start `thread_guard`\n",
                        parent_.master_string().c_str());
          return false;
        }
      }

      utils_dprintf(STDOUT_FILENO, LOG_LEVEL_STR_INFO "%sSlot alloc\n",
                    parent_.master_string().c_str());

      return true;
    }

    void native_thread_guard(std::stop_token stop_token) {
      size_t index = 0;

      {
        parent_.lock_guard();

        while (!attached_check(index)) {
          if (index == Log::kProcessMax - 1) {
            utils_dprintf(STDERR_FILENO,
                          LOG_LEVEL_STR_WARN "%sNo valid pid was matched\n",
                          parent_.master_string().c_str());
            return;
          }
          ++index;
        }
      }

      /**
       * @note Although the probability of occurrence is not high, the
       * initialization function may be called repeatedly. Slightly shortening
       * the duration of a single sleep can help threads exit quickly.
       */
      constexpr int kOnceSleepMs = 1000;
      int splits = Log::kProcessFeedGuardMilliseconds / kOnceSleepMs;
      splits = UTILS_MAX(1, splits);

      thread_guard_ready_.store(true, std::memory_order_relaxed);

      while (!stop_token.stop_requested()) {
        uint64_t timestamp_ms = Time::get_monotonic_steady_ms();

        {
          parent_.lock_guard();

          if (attached_check(index)) [[likely]] {
            header_->attached_maps[index].timestamp_ms = timestamp_ms;
          } else {
            utils_dprintf(STDERR_FILENO,
                          LOG_LEVEL_STR_WARN
                          "%sInvalid argument. `attached` thread exit\n",
                          parent_.master_string().c_str());
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
  };

 private:
  Master *master_;
};
} // namespace Shm
} // namespace Log
} // namespace Utils
