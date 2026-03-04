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

enum class SlotState : int {
  kFree = 0,
  kWaiting = 1,
  kReady = 2,
};

enum class FsType : int {
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
      char buf[kLogBufferSize];
    } log;

    struct {
      char path[kLogPathMax];
      FsType type;
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

  Process::Pid pid;
  Buffer buffer;
};
#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

struct AttachedMap {
  Process::Pid pid;
  uint64_t timestamp_ms;
};

struct Header {
  static constexpr uint32_t kMagicHeader = 0xDEADBEEF;
  static constexpr uint32_t kMagicDaemonCipher = 0xC0FFEE;

  /**
   * @note
   * - (1) This variable should be guarded by the process mutex.
   *
   * - (2) Filled by the creator of the shared memory.
   */
  uint32_t magic;

  std::atomic<uint32_t> daemon_cipher;
  std::atomic<bool> is_daemon_ready;

  /**
   * @note The parameters about `attached` should be guarded by the `mutex_shm`.
   */
  size_t attached_count;
  enum MasterType attached_master_type[kProcessMax];
  AttachedMap attached_maps[kProcessMax];

  std::atomic<uint64_t> write_index;
  std::atomic<uint64_t> read_index;
  size_t capacity;
  size_t item_size;

#if !defined(_WIN32) && !defined(_WIN64)
  pthread_mutex_t mutex_shm;
  pthread_mutex_t mutex_crash;
#endif

  uint64_t reserved[8];
};

namespace GMutex {
#if defined(_WIN32) || defined(_WIN64)
using MHandle = Process::GMutex;
#else
using MHandle = Process::FMutex;
#endif

inline std::optional<MHandle> mutex_;

inline auto create() -> std::expected<void, std::string> {
  auto ret = MHandle::create(kMutexProcessKey);

  if (!ret.has_value()) {
    return std::unexpected(
      ret.error().append(Io::row_gs("\n{0}", utils_pretty_func)));
  }

  mutex_.emplace(std::move(ret.value()));

  return {};
}

inline void destroy() {
  if (!mutex_.has_value())
    return;

  mutex_->destroy();
  mutex_.reset();
}

inline auto lock() -> std::expected<void, std::string> {
  if (!mutex_.has_value()) {
    return std::unexpected(IO_E("Invalid argument. `mutex`"));
  }

  auto ret = mutex_->lock();

  return ret.has_value() ? std::expected<void, std::string> {}
                         : std::unexpected(ret.error().append(
                             Io::row_gs("\n{0}", utils_pretty_func)));
}

inline auto unlock() -> std::expected<void, std::string> {
  return mutex_->unlock();
}
} // namespace GMutex

class SharedMem {
#define SUFFIX_ \
  ( \
    std::format("\nPID: {0}" \
                "\n{1}", \
                Process::pid(), utils_pretty_func))

 private:
  class LockGuard {
   public:
    explicit LockGuard(Process::GMutex &mutex)
        : mutex_(mutex), lock_ret_(false) {
      if (auto ret = mutex_.lock(); ret.has_value()) {
        lock_ret_ = true;
      } else {
        io_errln(ret.error());
      }
    }

    ~LockGuard() {
      if (lock_ret_) {
        (void)mutex_.unlock();
      }
    }

    LockGuard(const LockGuard &) = delete;
    LockGuard &operator=(const LockGuard &) = delete;

   private:
    Process::GMutex &mutex_;
    bool lock_ret_;
  };

  LockGuard lock_guard() {
    return LockGuard(mutex_shm_.value());
  }

 private:
  struct MasterToken {
    explicit MasterToken() = default;
  };

 public:
  class Master {
   public:
    Master(MasterToken, SharedMem &parent, enum MasterType master_type)
        : parent_(parent),
          header_(parent.header_),
          slots_(nullptr),
          master_type_(master_type),
          thread_guard_ready_(false) {}

    ~Master() = default;

   public:
    bool is_shm_creator() const {
      return parent_.is_shm_creator_;
    }

    /**
     * @brief Check if the daemon is still alive.
     * If the daemon has died, reset the related resources and return `false`.
     *
     * @note This function takes a long time to call once.
     */
    bool daemon_alive_or_reset() {
      header_->daemon_cipher.store(0, std::memory_order_relaxed);

      std::this_thread::sleep_for(
        std::chrono::milliseconds(kProcessFeedGuardMilliseconds + 300));

      if (Header::kMagicDaemonCipher !=
          header_->daemon_cipher.load(std::memory_order_relaxed)) {
        for (size_t i = 0; i < kProcessMax; ++i) {
          if (header_->attached_master_type[i] == MasterType::kDaemon) {
            slot_reset(i);
            break;
          }
        }
        header_->is_daemon_ready.store(false);
        return false;
      }

      return true;
    }

    LockGuard lock_guard() {
      return parent_.lock_guard();
    }

    Header *get_shm_header() const {
      return header_;
    }

    void slots_free() {
      if (header_ && slots_) {
        master_free();
        slots_ = nullptr;
      }
    }

    auto slots_alloc() -> std::expected<Slot *, std::string> {
      if (is_shm_creator()) {
        header_->is_daemon_ready.store(false);
        header_->attached_count = 0;
        for (size_t i = 0; i < kProcessMax;
             header_->attached_master_type[i++] = MasterType::kNone)
          ;
        header_->write_index.store(0);
        header_->read_index.store(0);
        header_->capacity = kShmCapacity;
        header_->item_size = sizeof(Slot);

        if (auto ret = master_alloc(); !ret.has_value())
          return std::unexpected(ret.error());
        header_->magic = Header::kMagicHeader;
      } else {
        if (header_->magic != Header::Header::kMagicHeader) {
          return std::unexpected(
            IO_E("Invalid argument. `magic`: {0}{1}", header_->magic, SUFFIX_));
        }

        if (auto ret = master_alloc(); !ret.has_value())
          return std::unexpected(ret.error());
      }

      uint8_t *base_ptr = reinterpret_cast<uint8_t *>(header_);
      slots_ =
        reinterpret_cast<Slot *>(base_ptr + parent_.get_shm_header_offset());

      return slots_;
    }

   private:
    SharedMem &parent_;
    Header *&header_;
    Slot *slots_;
    MasterType master_type_;
    std::jthread thread_guard_;
    std::atomic<bool> thread_guard_ready_;

    void master_free() {
      switch (master_type_) {
      case MasterType::kDaemon:
      case MasterType::kNative:
        return master_free_impl();
      default:
        return;
      }
    }

    auto master_alloc() -> std::expected<void, std::string> {
      switch (master_type_) {
      case MasterType::kDaemon:
        return daemon_master_alloc();
      case MasterType::kNative:
        return native_master_alloc();
      default:
        return std::unexpected(IO_E("Invalid argument. `master_type_`: {0}{1}",
                                    static_cast<int>(master_type_), SUFFIX_));
      }
    }

    bool slot_check(size_t index) const {
      return (header_->attached_master_type[index] == master_type_ &&
              header_->attached_maps[index].pid == Process::pid());
    }

    /**
     * @note This function must be guarded by the shm mutex before it is
     * called
     */
    void slot_reset(size_t index) {
      std::memset(header_->attached_maps + index, 0, sizeof(AttachedMap));
      header_->attached_master_type[index] = MasterType::kNone;
      header_->attached_count =
        header_->attached_count ? header_->attached_count - 1 : 0;
    }

    void slot_free() {
      lock_guard();

      for (size_t i = 0; i < kProcessMax; ++i) {
        if (slot_check(i)) {
          slot_reset(i);
          break;
        }
        if (i == kProcessMax - 1) {
          io_errln(IO_WSP("No valid pid was matched{0}", SUFFIX_));
        }
      }
    }

    auto slot_alloc() -> std::expected<void, std::string> {
      lock_guard();

      for (size_t i = 0; i < kProcessMax; ++i) {
        if (header_->attached_master_type[i] == MasterType::kNone) {
          ++header_->attached_count;
          header_->attached_master_type[i] = master_type_;
          header_->attached_maps[i].pid = Process::pid();
          header_->attached_maps[i].timestamp_ms =
            Time::get_monotonic_steady_ms();
          break;
        }
        if (i == kProcessMax - 1) {
          return std::unexpected(
            IO_E("\nCache full. Too many processes{0}", SUFFIX_));
        }
      }

      return {};
    }

    void master_free_impl() {
      thread_guard_.request_stop();
      if (thread_guard_.joinable()) {
        thread_guard_.join();
      }

      slot_free();

      io_outln(IO_ISP("Slot free"));
    }

    auto daemon_master_alloc() -> std::expected<void, std::string> {
      if (header_->is_daemon_ready.load(std::memory_order_relaxed)) {
        return std::unexpected(IO_WSP("Another daemon already exists"));
      }

      if (auto ret = slot_alloc(); !ret.has_value())
        return std::unexpected(ret.error());

      thread_guard_ =
        std::jthread(std::bind_front(&Master::daemon_thread_guard, this));

      io_outln(IO_ISP("Slot alloc"));

      return {};
    }

    void daemon_thread_guard(std::stop_token stop_token) {
      int splits =
        kProcessGuardTimeoutMilliseconds / kProcessFeedGuardMilliseconds;
      splits = UTILS_MAX(1, splits);

      while (!stop_token.stop_requested()) {
        for (int i = 0; i < splits; ++i) {
          std::this_thread::sleep_for(
            std::chrono::milliseconds(kProcessFeedGuardMilliseconds));
          if (stop_token.stop_requested()) [[unlikely]]
            return;
          header_->daemon_cipher.store(Header::kMagicDaemonCipher,
                                       std::memory_order_relaxed);
        }

        {
          uint64_t timestamp_ms = Time::get_monotonic_steady_ms();

          lock_guard();

          for (size_t i = 0; i < kProcessMax; ++i) {
            if (header_->attached_master_type[i] != MasterType::kNative) {
              continue;
            }

            AttachedMap *attached_map = header_->attached_maps + i;
            if (timestamp_ms - attached_map->timestamp_ms >
                kProcessGuardTimeoutMilliseconds) {
              slot_reset(i);
            }
          }

          if (header_->attached_count == kProcessNbDaemon)
            break;
        }
      }
    }

    auto native_master_alloc() -> std::expected<void, std::string> {
      if (auto ret = slot_alloc(); !ret.has_value())
        return std::unexpected(ret.error());

      thread_guard_ =
        std::jthread(std::bind_front(&Master::native_thread_guard, this));
      constexpr int kRetryTimes = 10;
      for (int i = 0; i < kRetryTimes; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (thread_guard_ready_.load(std::memory_order_relaxed))
          break;
        if (i == kRetryTimes - 1) {
          master_free_impl();
          return std::unexpected(
            IO_E("\nFail to start `thread_guard`{0}", SUFFIX_));
        }
      }

      io_outln(IO_ISP("Slot alloc"));

      return {};
    }

    void native_thread_guard(std::stop_token stop_token) {
      size_t index = 0;

      {
        lock_guard();

        while (!slot_check(index)) {
          if (index == kProcessMax - 1) {
            io_errln(IO_WSP("No valid pid was matched{0}", SUFFIX_));
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
      int splits = kProcessFeedGuardMilliseconds / kOnceSleepMs;
      splits = UTILS_MAX(1, splits);

      thread_guard_ready_.store(true, std::memory_order_relaxed);

      while (!stop_token.stop_requested()) {
        uint64_t timestamp_ms = Time::get_monotonic_steady_ms();

        {
          lock_guard();

          if (slot_check(index)) [[likely]] {
            header_->attached_maps[index].timestamp_ms = timestamp_ms;
          } else {
            io_errln(
              IO_WSP("Invalid argument. `attached` thread exit{0}", SUFFIX_));
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
  SharedMem()
      :
#if defined(_WIN32) || defined(_WIN64)
        shm_handle_(nullptr),
#else
        shm_fd_(-1),
        destructor_counter_(0),
#endif
        initialized_(false),
        is_shm_creator_(false),
        shm_name_(Ns::Shm::generate_name(kShmKey)),
        mapped_size_(0),
        header_(nullptr) {
  }

  ~SharedMem() {
    shm_free();
  }

 public:
  SharedMem(const SharedMem &) = delete;
  SharedMem &operator=(const SharedMem &) = delete;

  static SharedMem &instance() {
    static SharedMem instance;

    return instance;
  }

  auto shm_alloc(enum MasterType master_type)
    -> std::expected<std::unique_ptr<Master>, std::string> {
    if (initialized_.exchange(true, std::memory_order_seq_cst)) {
      return std::unexpected(IO_E("Repeated initialization{0}", SUFFIX_));
    }

    std::expected<void, std::string> ret;

    if (ret = open_shm(); !ret.has_value())
      goto label_free1;
    if (ret = memory_map(); !ret.has_value())
      goto label_free2;

    return std::make_unique<Master>(MasterToken {}, *this, master_type);

  label_free2:
    close_shm();
  label_free1:
    initialized_.store(false, std::memory_order_seq_cst);

    return std::unexpected(ret.error());
  }

  void shm_free() {
    if (!initialized_.exchange(false, std::memory_order_seq_cst))
      return;

    memory_unmap();
    close_shm();
  }

 private:
#if defined(_WIN32) || defined(_WIN64)
  HANDLE shm_handle_;
#else
  int shm_fd_;
  size_t destructor_counter_;
#endif
  std::atomic<bool> initialized_;
  bool is_shm_creator_;
  std::string shm_name_;
  size_t mapped_size_;
  Header *header_;
  std::optional<Process::GMutex> mutex_shm_;
#warning "Remember to add process crash handling"
  std::optional<Process::GMutex> mutex_crash_;

  constexpr size_t get_shm_header_offset() const {
    size_t header_size = sizeof(Header);
    size_t header_aligned =
      (header_size + kShmCacheLineSize - 1) & ~(kShmCacheLineSize - 1);

    return header_aligned;
  }

  constexpr size_t get_shm_size() const {
    return get_shm_header_offset() + (kShmCapacity * sizeof(Slot));
  }

#if defined(_WIN32) || defined(_WIN64)
  auto open_shm() -> std::expected<void, std::string> {
    size_t size_needed = get_shm_size();

    DWORD size_high = (DWORD)((uint64_t)size_needed >> 32);
    DWORD size_low = (DWORD)((uint64_t)size_needed & 0xFFFFFFFF);

    shm_handle_ =
      CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                         size_high, size_low, shm_name_.c_str());
    DWORD dw_err = GetLastError();
    if (shm_handle_) {
      is_shm_creator_ = dw_err == ERROR_ALREADY_EXISTS ? false : true;
    } else {
      return std::unexpected(
        IO_E("{}", Io::win_err(dw_err, "CreateFileMappingA", "{}", SUFFIX_)));
    }

    mapped_size_ = size_needed;

    return {};
  }

  void close_shm() {
    if (shm_handle_) {
      CloseHandle(shm_handle_);
      shm_handle_ = nullptr;
    }
  }

  auto memory_map() -> std::expected<void, std::string> {
    void *ptr =
      MapViewOfFile(shm_handle_, FILE_MAP_ALL_ACCESS, 0, 0, mapped_size_);
    if (!ptr) {
      const DWORD dw_err = GetLastError();
      return std::unexpected(
        IO_E("{}", Io::win_err(dw_err, "MapViewOfFile", "{}", SUFFIX_)));
    }
    header_ = static_cast<Header *>(ptr);

    if (auto ret = mutex_create(); !ret.has_value()) {
      UnmapViewOfFile(header_);
      header_ = nullptr;
      return std::unexpected(ret.error());
    }

    return {};
  }

  void memory_unmap() {
    mutex_destroy();

    if (header_) {
      UnmapViewOfFile(header_);
      header_ = nullptr;
    }
  }
#else
  auto open_shm() -> std::expected<void, std::string> {
    int errno_err = 0;
    size_t size_needed = get_shm_size();

    shm_fd_ = shm_open(shm_name_.c_str(), O_CREAT | O_EXCL | O_RDWR,
                       File::string_to_mode(_SIRIUS_POSIX_FILE_MODE));
    errno_err = errno;
    if (shm_fd_ >= 0) {
      is_shm_creator_ = true;
      if (ftruncate(shm_fd_, size_needed) == -1) {
        errno_err = errno;
        shm_unlink(shm_name_.c_str());
        close(shm_fd_);
        return std::unexpected(
          IO_E("{}", Io::errno_err(errno_err, "ftruncate", "{}", SUFFIX_)));
      }
    } else if (errno_err == EEXIST) {
      is_shm_creator_ = false;
      shm_fd_ = shm_open(shm_name_.c_str(), O_RDWR, 0);
      if (shm_fd_ < 0) {
        errno_err = errno;
        return std::unexpected(IO_E(
          "{}", Io::errno_err(errno_err, "shm_open (attach)", "{}", SUFFIX_)));
      }
    } else {
      return std::unexpected(
        IO_E("{}", Io::errno_err(errno_err, "shm_open (open)", "{}", SUFFIX_)));
    }

    mapped_size_ = size_needed;

    return {};
  }

  void close_shm() {
    if (shm_fd_ != -1) {
      close(shm_fd_);
      shm_fd_ = -1;
    }

    if (destructor_counter_ == 0) {
      io_outln("Unlink the share memory");
      shm_unlink(shm_name_.c_str());
    }
  }

  auto memory_map() -> std::expected<void, std::string> {
    void *ptr = mmap(nullptr, mapped_size_, PROT_READ | PROT_WRITE, MAP_SHARED,
                     shm_fd_, 0);
    if (ptr == MAP_FAILED) {
      const int errno_err = errno;
      return std::unexpected(
        IO_E("{}", Io::errno_err(errno_err, "mmap", "{}", SUFFIX_)));
    }
    header_ = static_cast<Header *>(ptr);

    if (auto ret = mutex_create(); !ret.has_value()) {
      munmap(header_, mapped_size_);
      header_ = nullptr;
      return std::unexpected(ret.error());
    }

    return {};
  }

  void memory_unmap() {
    if (!header_)
      return;

    {
      lock_guard();

      destructor_counter_ = header_->attached_count;
    }

    mutex_destroy();

    munmap(header_, mapped_size_);
    header_ = nullptr;
  }
#endif

  auto mutex_create() -> std::expected<void, std::string> {
    auto fn_create = [&]() {
#if defined(_WIN32) || defined(_WIN64)
      return Process::GMutex::create(kMutexShmKey);
#else
      return Process::GMutex::create(&header_->mutex_shm, is_shm_creator_);
#endif
    };

    if (auto ret = fn_create(); !ret.has_value()) {
      return std::unexpected(ret.error().append(Io::row_gs("{0}", SUFFIX_)));
    } else {
      mutex_shm_.emplace(std::move(ret.value()));
    }

    if (auto ret = fn_create(); !ret.has_value()) {
      if (is_shm_creator_) {
        mutex_shm_->destroy();
        mutex_shm_.reset();
      }
      return std::unexpected(ret.error().append(Io::row_gs("{0}", SUFFIX_)));
    } else {
      mutex_crash_.emplace(std::move(ret.value()));
    }

    return {};
  }

  void mutex_destroy() {
    auto fn_destory = [&](std::optional<Process::GMutex> &mutex) {
      if (mutex.has_value()) {
#if defined(_WIN32) || defined(_WIN64)
        mutex->destroy();
        mutex.reset();
#else
        if (destructor_counter_ == 0) {
          mutex->destroy();
          mutex.reset();
        }
#endif
      }
    };

    fn_destory(mutex_crash_);
    fn_destory(mutex_shm_);
  }

#undef SUFFIX_
};
} // namespace Shm
} // namespace Log
} // namespace Utils
