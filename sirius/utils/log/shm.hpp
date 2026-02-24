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
using MHandle = Process::GMutex;
#else
using MHandle = Process::FMutex;
#endif

inline std::optional<MHandle> mutex_;

inline auto create() -> std::expected<void, std::string> {
  auto ret = MHandle::create(Log::kMutexProcessKey);

  if (!ret.has_value()) {
    return std::unexpected(
      ret.error().append(Io::row_gs("\n{0}", utils_pretty_func)));
  }

  if (!ret.value().valid()) {
    return std::unexpected(Io::io()
                             .s_error(SIRIUS_FILE_NAME, __LINE__)
                             .append(Io::row_gs("The mutex address is empty")));
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
    return std::unexpected(Io::io()
                             .s_error(SIRIUS_FILE_NAME, __LINE__)
                             .append(Io::row_gs("Invalid argument. `mutex`")));
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

class Shm {
#define SHM_SUFFIX \
  ( \
    Io::row_gs("\nPID: {0}" \
               "\n{1}", \
               Process::pid(), utils_pretty_func))

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
        is_initialization_(false),
        is_shm_creator_(false),
        master_type_(master_type),
        shm_name_(Ns::Shm::generate_name(Log::kKey)),
        mapped_size_(0),
        master_(nullptr) {
  }

  ~Shm() {
    deinit();
  }

  auto init() -> std::expected<void, std::string> {
    if (is_initialization_.exchange(true, std::memory_order_relaxed)) {
      return {};
    }

    std::expected<void, std::string> ret;

    if (ret = open_shm(); !ret.has_value())
      return std::unexpected(ret.error());
    if (ret = memory_map(); !ret.has_value())
      goto label_free1;
    if (ret = slots_init(); !ret.has_value())
      goto label_free2;

    is_initialization_.store(true, std::memory_order_relaxed);

    return {};

    // slots_deinit();
  label_free2:
    memory_unmap();
  label_free1:
    close_shm();

    is_initialization_.store(false, std::memory_order_relaxed);

    return std::unexpected(ret.error());
  }

  void deinit() {
    if (!is_initialization_.exchange(false, std::memory_order_relaxed)) {
      return;
    }

    slots_deinit();
    memory_unmap();
    close_shm();
  }

  bool is_shm_creator() const {
    if (is_initialization_.load(std::memory_order_relaxed)) [[likely]] {
      return is_shm_creator_;
    }

    return false;
  }

 private:
  // --- LockGuard ---
  class LockGuard {
   public:
    explicit LockGuard(Process::GMutex &mutex)
        : mutex_(mutex), lock_ret_(false) {
      if (auto ret = mutex_.lock(); ret.has_value()) {
        lock_ret_ = true;
      } else {
        auto es = ret.error()
                    .append(Io::row_gs("\n{0}", utils_pretty_func))
                    .append("\n");
        UTILS_WRITE(STDERR_FILENO, es.c_str(), es.size());
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

 public:
  LockGuard lock_guard() {
    return LockGuard(shm_mutex_.value());
  }

 private:
#if defined(_WIN32) || defined(_WIN64)
  HANDLE shm_handle_;
#else
  int shm_fd_;
  size_t destructor_counter_;
#endif
  std::atomic<bool> is_initialization_;
  std::optional<Process::GMutex> shm_mutex_;
  bool is_shm_creator_;
  MasterType master_type_;
  std::string shm_name_;
  size_t mapped_size_;

// --- Shared Memory ---
#if defined(_WIN32) || defined(_WIN64)
  auto open_shm() -> std::expected<void, std::string> {
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
      return std::unexpected(
        Io::win_last_error(dw_err, "CreateFileMappingA").append(SHM_SUFFIX));
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
        Io::win_last_error(dw_err, "MapViewOfFile").append(SHM_SUFFIX));
    }
    header = static_cast<Header *>(ptr);

    if (auto ret = shm_mutex_create(); !ret.has_value()) {
      UnmapViewOfFile(header);
      header = nullptr;
      return std::unexpected(ret.error());
    }

    return {};
  }

  void memory_unmap() {
    shm_mutex_destroy();

    if (header) {
      UnmapViewOfFile(header);
      header = nullptr;
    }
  }
#else
  auto open_shm() -> std::expected<void, std::string> {
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
        shm_unlink(shm_name_.c_str());
        close(shm_fd_);
        return std::unexpected(
          Io::errno_error(errno_err, "ftruncate").append(SHM_SUFFIX));
      }
    } else if (errno_err == EEXIST) {
      is_shm_creator_ = false;
      shm_fd_ = shm_open(shm_name_.c_str(), O_RDWR, 0);
      if (shm_fd_ < 0) {
        errno_err = errno;
        return std::unexpected(
          Io::errno_error(errno_err, "shm_open (attach)").append(SHM_SUFFIX));
      }
    } else {
      return std::unexpected(
        Io::errno_error(errno_err, "shm_open (open)").append(SHM_SUFFIX));
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
      auto es = Io::io()
                  .s_info("")
                  .append(Io::row_gs("Unlink the share memory"))
                  .append("\n");
      UTILS_WRITE(STDOUT_FILENO, es.c_str(), es.size());
      shm_unlink(shm_name_.c_str());
    }
  }

  auto memory_map() -> std::expected<void, std::string> {
    std::string es;

    void *ptr = mmap(nullptr, mapped_size_, PROT_READ | PROT_WRITE, MAP_SHARED,
                     shm_fd_, 0);
    if (ptr == MAP_FAILED) {
      const int errno_err = errno;
      return std::unexpected(
        Io::errno_error(errno_err, "mmap").append(SHM_SUFFIX));
    }
    header = static_cast<Header *>(ptr);

    if (auto ret = shm_mutex_create(); !ret.has_value()) {
      munmap(header, mapped_size_);
      header = nullptr;
      return std::unexpected(ret.error());
    }

    return {};
  }

  void memory_unmap() {
    if (!header)
      return;

    {
      lock_guard();

      destructor_counter_ = header->attached_count;
    }

    shm_mutex_destroy();

    munmap(header, mapped_size_);
    header = nullptr;
  }
#endif

  void shm_mutex_destroy() {
    if (!shm_mutex_.has_value())
      return;

#if defined(_WIN32) || defined(_WIN64)
    shm_mutex_->destroy();
    shm_mutex_.reset();
#else
    if (destructor_counter_ == 0) {
      shm_mutex_->destroy();
      shm_mutex_.reset();
    }
#endif
  }

  auto shm_mutex_create() -> std::expected<void, std::string> {
    std::string es;

    auto create_ret =
#if defined(_WIN32) || defined(_WIN64)
      Process::GMutex::create(Log::kMutexShmKey);
#else
      Process::GMutex::create(&header->shm_mutex, is_shm_creator_);
#endif
    if (!create_ret.has_value()) {
      return std::unexpected(create_ret.error().append(SHM_SUFFIX));
    }

    if (!create_ret.value().valid()) {
      return std::unexpected(
        Io::io().s_error(SIRIUS_FILE_NAME, __LINE__).append(SHM_SUFFIX));
    }

    shm_mutex_.emplace(std::move(create_ret.value()));

    return {};
  }

  void slots_deinit() {
    if (master_) {
      master_->slot_free();
      slots = nullptr;

      delete master_;
      master_ = nullptr;
    }
  }

  auto slots_init() -> std::expected<void, std::string> {
    uint8_t *base_ptr = reinterpret_cast<uint8_t *>(header);

    slots = reinterpret_cast<Slot *>(base_ptr + get_shm_header_offset());

    master_ = new Master(*this);

    if (!is_shm_creator_) {
      if (header->magic != Header::Header::kMagic) {
#if !defined(_WIN32) && !defined(_WIN64)
        shm_unlink(shm_name_.c_str());
#endif
        return std::unexpected(
          Io::io()
            .s_error(SIRIUS_FILE_NAME, __LINE__)
            .append(Io::row_gs("Invalid argument. `magic`: {0}", header->magic))
            .append(SHM_SUFFIX));
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

    if (auto ret = master_->slot_alloc(); !ret.has_value()) {
      return std::unexpected(ret.error());
    }

    header->magic = Header::kMagic;

    return {};
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
    Master(Shm &parent)
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

    auto slot_alloc() -> std::expected<void, std::string> {
      switch (parent_.master_type_) {
      case MasterType::kDaemon:
        return daemon_slot_alloc();
      case MasterType::kNative:
        return native_slot_alloc();
      default:
        return std::unexpected(
          Io::io()
            .s_error(SIRIUS_FILE_NAME, __LINE__)
            .append(Io::row_gs("Invalid argument. `master_type_`: {0}",
                               static_cast<int>(parent_.master_type_))
                      .append(SHM_SUFFIX)));
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
     * @note This function must be guarded by the shm mutex before it is
     * called
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
          auto es = Io::io()
                      .s_warn("")
                      .append(Io::row_gs("No valid pid was matched"))
                      .append(SHM_SUFFIX)
                      .append("\n");
          UTILS_WRITE(STDERR_FILENO, es.c_str(), es.size());
        }
      }
    }

    auto attached_alloc() -> std::expected<void, std::string> {
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
          return std::unexpected(
            Io::io()
              .s_error(SIRIUS_FILE_NAME, __LINE__)
              .append(Io::row_gs("\nCache full. Too many processes"))
              .append(SHM_SUFFIX));
        }
      }

      return {};
    }

    void T_slot_free() {
      thread_guard_.request_stop();
      if (thread_guard_.joinable()) {
        thread_guard_.join();
      }

      attached_free();

      auto es =
        Io::io().s_info("").append(Io::row_gs("Slot free")).append("\n");
      UTILS_WRITE(STDOUT_FILENO, es.c_str(), es.size());
    }

    auto daemon_slot_alloc() -> std::expected<void, std::string> {
      if (header_->is_daemon_ready.load(std::memory_order_relaxed)) {
        return std::unexpected(Io::io().s_warn("").append(
          Io::row_gs("Another daemon already exists")));
      }

      if (auto ret = attached_alloc(); !ret.has_value())
        return std::unexpected(ret.error());

      thread_guard_ =
        std::jthread(std::bind_front(&Shm::Master::daemon_thread_guard, this));

      auto es =
        Io::io().s_info("").append(Io::row_gs("Slot alloc")).append("\n");
      UTILS_WRITE(STDOUT_FILENO, es.c_str(), es.size());

      return {};
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

    auto native_slot_alloc() -> std::expected<void, std::string> {
      if (auto ret = attached_alloc(); !ret.has_value())
        return std::unexpected(ret.error());

      thread_guard_ =
        std::jthread(std::bind_front(&Shm::Master::native_thread_guard, this));
      constexpr int kRetryTimes = 10;
      for (int i = 0; i < kRetryTimes; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (thread_guard_ready_.load(std::memory_order_relaxed))
          break;
        if (i == kRetryTimes - 1) {
          T_slot_free();
          return std::unexpected(
            Io::io()
              .s_error(SIRIUS_FILE_NAME, __LINE__)
              .append(Io::row_gs("\nFail to start `thread_guard`"))
              .append(SHM_SUFFIX));
        }
      }

      auto es =
        Io::io().s_info("").append(Io::row_gs("Slot alloc")).append("\n");
      UTILS_WRITE(STDOUT_FILENO, es.c_str(), es.size());

      return {};
    }

    void native_thread_guard(std::stop_token stop_token) {
      size_t index = 0;

      {
        parent_.lock_guard();

        while (!attached_check(index)) {
          if (index == Log::kProcessMax - 1) {
            auto es = Io::io()
                        .s_warn("")
                        .append(Io::row_gs("No valid pid was matched"))
                        .append(SHM_SUFFIX)
                        .append("\n");
            UTILS_WRITE(STDERR_FILENO, es.c_str(), es.size());
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
            auto es =
              Io::io()
                .s_warn("")
                .append(Io::row_gs("Invalid argument. `attached` thread exit"))
                .append(SHM_SUFFIX);
            UTILS_WRITE(STDERR_FILENO, es.c_str(), es.size());
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

#undef SHM_SUFFIX
};
} // namespace Shm
} // namespace Log
} // namespace Utils
