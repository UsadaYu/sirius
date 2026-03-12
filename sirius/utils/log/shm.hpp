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

namespace sirius {
namespace utils {
namespace log {
/**
 * @note The use of `std::hardware_destructive_interference_size` is waived
 * here.
 */
inline constexpr size_t kShmCacheLineSize = 64;

enum class ShmSlotState : int {
  kFree = 0,
  kWaiting = 1,
  kReady = 2,
};

enum class ShmBufDataFsType : int {
  kNone = 0, // Make no changes.
  kStd = 1,  // Set to default `stdout` / `stderr`.
  kFile = 2, // Write to file.
};

enum class ShmBufDataType : int {
  kLog = 0,    // likely
  kConfig = 1, // unlikely
};

struct ShmBuf {
  ShmBufDataType type;
  union {
    struct {
      size_t buf_size;
      char buf[kLogBufferSize];
    } log;

    struct {
      char path[kLogPathMax];
      ShmBufDataFsType type;
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
struct alignas(kShmCacheLineSize) ShmSlot {
  std::atomic<ShmSlotState> state;
  std::atomic<uint64_t> timestamp_ms;

  int64_t pid;
  ShmBuf buffer;
};
#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

struct ShmSlotMap {
  int64_t pid;
  uint64_t timestamp_ms;
};

struct ShmHeader {
  static constexpr uint32_t kMagicHeader = 0xDEADBEEF;

  /**
   * @note
   * - (1) This variable should be guarded by the process mutex.
   *
   * - (2) Filled by the creator of the shared memory.
   */
  uint32_t magic;
  uint64_t spawner_timestamp;

  std::atomic<bool> is_daemon_ready;

  /** ---
   * @note These parameters should be guarded by the `mutex_shm`.
   */
  size_t slot_attached_count;
  enum MasterType slot_master_type[kProcessMax];
  ShmSlotMap slot_map[kProcessMax];
  //  ---

  std::atomic<uint64_t> write_index;
  std::atomic<uint64_t> read_index;

#if !defined(_WIN32) && !defined(_WIN64)
  pthread_mutex_t mutex_shm;
  pthread_mutex_t mutex_crash;
#endif

  uint64_t reserved[8];
};

class Shm {
 private:
  static constexpr size_t kHeaderOffset =
    (sizeof(ShmHeader) + kShmCacheLineSize - 1) & ~(kShmCacheLineSize - 1);
  static constexpr size_t kTotalShmSize =
    kHeaderOffset + (kShmCapacity * sizeof(ShmSlot));

 private:
  class LockGuard {
   public:
    explicit LockGuard(process::GMutex &mutex) : mutex_(mutex) {
      if (auto ret = mutex_.lock(); ret.has_value()) {
        lock_state_ = ret.value();
      } else {
        throw std::runtime_error(ret.error().join_self_all());
      }
    }

    ~LockGuard() {
      if (lock_state_ == process::LockState::kSuccess ||
          lock_state_ == process::LockState::kOwnerDead) {
        (void)mutex_.unlock();
      }
    }

    LockGuard(const LockGuard &) = delete;
    LockGuard &operator=(const LockGuard &) = delete;

   private:
    process::GMutex &mutex_;
    process::LockState lock_state_ = process::LockState::kBusy;
  };

 private:
  struct MasterToken {
    explicit MasterToken() = default;
  };

 public:
  class Master {
   public:
    enum class DaemonState {
      kAlive,
      kPerfectDead,
      kOwnerDead,
    };

   public:
    Master(MasterToken, Shm &parent, enum MasterType master_type)
        : parent_(parent), header_(parent.header_), master_type_(master_type) {}

    ~Master() = default;

   public:
    bool is_shm_creator() const { return parent_.is_shm_creator_; }
    auto mutex_crash_lock() { return parent_.mutex_crash_->lock(); }
    auto mutex_crash_trylock() { return parent_.mutex_crash_->trylock(); }
    auto mutex_crash_unlock() { return parent_.mutex_crash_->unlock(); }
    LockGuard lock_guard() { return LockGuard(parent_.mutex_shm_.value()); }
    ShmHeader *get_shm_header() const { return header_; }
    ShmSlot *get_shm_slots() const {
      return reinterpret_cast<ShmSlot *>(reinterpret_cast<uint8_t *>(header_) +
                                         kHeaderOffset);
    }

    void slots_free() {
      if (header_) {
        master_free();
      }
    }

    auto slots_alloc() -> std::expected<void, UTrace> {
      if (is_shm_creator()) {
        header_->is_daemon_ready.store(false);
        header_->spawner_timestamp = 0;
        header_->slot_attached_count = 0;
        for (auto &t : header_->slot_master_type) {
          t = MasterType::kNone;
        }
        header_->write_index.store(0);
        header_->read_index.store(0);
      } else {
        if (header_->magic != ShmHeader::kMagicHeader) {
          auto es = std::format("Invalid argument. `magic`: {0}{1}",
                                header_->magic, line_pid());
          return std::unexpected(UTrace(std::move(es)));
        }
      }

      return master_alloc()
        .and_then([&]() -> std::expected<void, UTrace> {
          header_->magic =
            is_shm_creator() ? ShmHeader::kMagicHeader : header_->magic;
          return {};
        })
        .transform_error([&](UTrace &&e) {
          e.msg_append(std::format("Shm creator: {0}", is_shm_creator()));
          utrace_transform_error(e);
        });
    }

    /**
     * @note This function should be guarded by the process mutex.
     */
    DaemonState daemon_state() {
      if (!header_->is_daemon_ready.load(std::memory_order_relaxed))
        return DaemonState::kPerfectDead;

      DaemonState state = DaemonState::kPerfectDead;
      if (auto ret = mutex_crash_trylock(); ret.has_value()) {
        switch (ret.value()) {
        case process::LockState::kBusy:
          return DaemonState::kAlive;
        case process::LockState::kOwnerDead:
          state = DaemonState::kOwnerDead;
          break;
        default:
          state = DaemonState::kPerfectDead;
          break;
        }
        (void)mutex_crash_unlock();
        header_->is_daemon_ready.store(false, std::memory_order_relaxed);
      } else {
        io_ln_error("{0}", ret.error().join_self_all());
      }

      {
        lock_guard();
        for (size_t i = 0; i < kProcessMax; ++i) {
          if (header_->slot_master_type[i] == MasterType::kDaemon) {
            io_ln_infosp("Reset the old daemon slot");
            slot_reset(i);
            break;
          }
        }
      }

      return state;
    }

   private:
    Shm &parent_;
    ShmHeader *&header_;
    MasterType master_type_;
    std::jthread thread_guard_ {};
    std::atomic<bool> thread_guard_ready_ = false;

    auto master_alloc() -> std::expected<void, UTrace> {
      io_ln_infosp("Master: {0}. Alloc", static_cast<int>(master_type_));

      switch (master_type_) {
      case MasterType::kDaemon:
        return daemon_master_alloc().utrace_transform_error_default();
      case MasterType::kNative:
        return native_master_alloc().utrace_transform_error_default();
      default:
        return std::unexpected(UTrace("Invalid argument. `master_type_`"));
      }
    }

    void master_free() {
      switch (master_type_) {
      case MasterType::kDaemon:
      case MasterType::kNative:
        master_free_impl();
        break;
      default:
        break;
      }

      io_ln_infosp("Master: {0}. Free", static_cast<int>(master_type_));
    }

    bool slot_check(size_t index) const {
      return (header_->slot_master_type[index] == master_type_ &&
              header_->slot_map[index].pid == process::pid());
    }

    /**
     * @note This function must be guarded by the shm mutex before it is
     * called
     */
    void slot_reset(size_t index) {
      std::memset(header_->slot_map + index, 0, sizeof(ShmSlotMap));
      header_->slot_master_type[index] = MasterType::kNone;
      header_->slot_attached_count =
        header_->slot_attached_count ? header_->slot_attached_count - 1 : 0;
    }

    void slot_free() {
      bool found = false;

      {
        lock_guard();
        for (size_t i = 0; i < kProcessMax; ++i) {
          if (slot_check(i)) {
            slot_reset(i);
            found = true;
            break;
          }
        }
      }

      if (!found) {
        io_ln_warnsp("No valid pid was matched{0}", line_pid());
      }
    }

    auto slot_alloc() -> std::expected<void, UTrace> {
      lock_guard();

      for (size_t i = 0; i < kProcessMax; ++i) {
        if (header_->slot_master_type[i] == MasterType::kNone) {
          ++header_->slot_attached_count;
          header_->slot_master_type[i] = master_type_;
          header_->slot_map[i].pid = process::pid();
          header_->slot_map[i].timestamp_ms = time::get_monotonic_steady_ms();
          break;
        }
        if (i == kProcessMax - 1) {
          auto es =
            std::format("Cache full. Too many processes{0}", line_pid());
          return std::unexpected(UTrace(std::move(es)));
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
    }

    auto daemon_master_alloc() -> std::expected<void, UTrace> {
      if (daemon_state() == DaemonState::kAlive) {
        return std::unexpected(UTrace("Another daemon already exists"));
      }

      return slot_alloc()
        .and_then([&]() -> std::expected<void, UTrace> {
          thread_guard_ =
            std::jthread(std::bind_front(&Master::daemon_thread_guard, this));
          return {};
        })
        .utrace_transform_error_default();
    }

    void daemon_thread_guard(std::stop_token stop_token) {
      constexpr uint64_t kOnceSleepMs = 2000;
      constexpr int kSplits =
        UTILS_MAX(1, kProcessGuardTimeoutMs / kOnceSleepMs);
      while (!stop_token.stop_requested()) {
        for (int i = 0; i < kSplits; ++i) {
          std::this_thread::sleep_for(std::chrono::milliseconds(kOnceSleepMs));
          if (stop_token.stop_requested())
            return;
        }

        {
          lock_guard();
          if (header_->slot_attached_count == kProcessNbDaemon)
            continue;
          uint64_t timestamp_ms = time::get_monotonic_steady_ms();
          for (size_t i = 0; i < kProcessMax; ++i) {
            if (header_->slot_master_type[i] != MasterType::kNative)
              continue;
            ShmSlotMap &ssm = header_->slot_map[i];
            if (timestamp_ms - ssm.timestamp_ms > kProcessGuardTimeoutMs) {
              slot_reset(i);
            }
          }
        }
      }
    }

    auto native_master_alloc() -> std::expected<void, UTrace> {
      if (auto ret = slot_alloc(); !ret.has_value())
        utrace_return(ret);

      thread_guard_ =
        std::jthread(std::bind_front(&Master::native_thread_guard, this));
      constexpr int kRetryTimes = 10;
      for (int i = 0; i < kRetryTimes; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (thread_guard_ready_.load(std::memory_order_relaxed))
          break;
        if (i == kRetryTimes - 1) {
          master_free_impl();
          return std::unexpected(UTrace("Fail to start `thread_guard`"));
        }
      }

      return {};
    }

    void native_thread_guard(std::stop_token stop_token) {
      size_t index = 0;

      {
        lock_guard();
        for (; !slot_check(index); ++index) {
          if (index == kProcessMax - 1) {
            io_ln_error("No valid pid was matched");
            return;
          }
        }
      }
      thread_guard_ready_.store(true, std::memory_order_relaxed);

      constexpr int kOnceSleepMs = 1000;
      constexpr int kSplits = UTILS_MAX(1, kProcessFeedGuardMs / kOnceSleepMs);
      while (!stop_token.stop_requested()) {
        {
          uint64_t timestamp_ms = time::get_monotonic_steady_ms();
          lock_guard();
          if (slot_check(index)) [[likely]] {
            header_->slot_map[index].timestamp_ms = timestamp_ms;
          } else {
            io_ln_error("\nInvalid argument. `{0}` exit", __func__);
            return;
          }
        }

        for (int i = 0; i < kSplits; ++i) {
          std::this_thread::sleep_for(std::chrono::milliseconds(kOnceSleepMs));
          if (stop_token.stop_requested())
            return;
        }
      }
    }
  };

 private:
  Shm() = default;

  ~Shm() { shm_free(); }

 public:
  Shm(const Shm &) = delete;
  Shm &operator=(const Shm &) = delete;

  static Shm &instance() {
    static Shm instance;
    return instance;
  }

  auto shm_alloc(enum MasterType master_type)
    -> std::expected<std::unique_ptr<Master>, UTrace> {
    if (initialized_.exchange(true, std::memory_order_seq_cst)) {
      auto es = std::format("Repeated initialization{0}", line_pid());
      return std::unexpected(UTrace(std::move(es)));
    }

    return open_shm()
      .and_then([&]() { return memory_map(); })
      .and_then([&]() -> std::expected<std::unique_ptr<Master>, UTrace> {
        return std::make_unique<Master>(MasterToken {}, *this, master_type);
      })
      .transform_error([&](UTrace &&e) {
        close_shm();
        initialized_.store(false, std::memory_order_seq_cst);
        utrace_transform_error(e);
      });
  }

  void shm_free() {
    if (!initialized_.exchange(false, std::memory_order_seq_cst))
      return;

    memory_unmap();
    close_shm();
  }

 private:
#if defined(_WIN32) || defined(_WIN64)
  HANDLE shm_handle_ = nullptr;
#else
  int shm_fd_ = -1;
  size_t destructor_counter_ = 0;
#endif
  std::atomic<bool> initialized_ = false;
  bool is_shm_creator_ = false;
  std::string shm_name_ = ns::shm::generate_name(kShmKey);
  ShmHeader *header_ = nullptr;
  std::optional<process::GMutex> mutex_shm_ {}, mutex_crash_ {};

  static std::string line_pid() {
    return std::format("\nPID: {0}", process::pid());
  }

#if defined(_WIN32) || defined(_WIN64)
  static std::string c_error(const DWORD err_code, std::string_view fn_str) {
    return Io::win_err(err_code, fn_str, "{}", line_pid());
  }
#else
  static std::string c_error(const int err_code, std::string_view fn_str) {
    return Io::errno_err(err_code, fn_str, "{}", line_pid());
  }
#endif

#if defined(_WIN32) || defined(_WIN64)
  auto open_shm() -> std::expected<void, UTrace> {
    DWORD size_high = (DWORD)((uint64_t)kTotalShmSize >> 32);
    DWORD size_low = (DWORD)((uint64_t)kTotalShmSize & 0xFFFFFFFF);

    shm_handle_ =
      CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                         size_high, size_low, shm_name_.c_str());
    DWORD dw_err = GetLastError();
    if (shm_handle_) {
      is_shm_creator_ = dw_err == ERROR_ALREADY_EXISTS ? false : true;
    } else {
      return std::unexpected(UTrace(c_error(dw_err, "CreateFileMappingA")));
    }

    return {};
  }

  void close_shm() {
    if (shm_handle_) {
      CloseHandle(shm_handle_);
      shm_handle_ = nullptr;
    }
  }

  auto memory_map() -> std::expected<void, UTrace> {
    void *ptr =
      MapViewOfFile(shm_handle_, FILE_MAP_ALL_ACCESS, 0, 0, kTotalShmSize);
    if (!ptr) {
      const DWORD dw_err = GetLastError();
      return std::unexpected(UTrace(c_error(dw_err, "MapViewOfFile")));
    }

    return mutex_create()
      .transform_error([&](UTrace &&e) {
        UnmapViewOfFile(ptr);
        utrace_transform_error(e);
      })
      .and_then([&]() -> std::expected<void, UTrace> {
        header_ = static_cast<ShmHeader *>(ptr);
        return {};
      });
  }

  void memory_unmap() {
    {
      LockGuard(mutex_shm_.value());
      if (header_->slot_attached_count == 0) {
        header_->magic = 0;
      }
    }

    mutex_destroy();
    if (header_) {
      UnmapViewOfFile(header_);
      header_ = nullptr;
    }
  }
#else
  auto open_shm() -> std::expected<void, UTrace> {
    int errno_err = 0;

    shm_fd_ = shm_open(shm_name_.c_str(), O_CREAT | O_EXCL | O_RDWR,
                       File::string_to_mode(_SIRIUS_POSIX_FILE_MODE));
    errno_err = errno;
    if (shm_fd_ >= 0) {
      is_shm_creator_ = true;
      if (ftruncate(shm_fd_, kTotalShmSize) == -1) {
        errno_err = errno;
        shm_unlink(shm_name_.c_str());
        close(shm_fd_);
        return std::unexpected(UTrace(c_error(errno_err, "ftruncate")));
      }
    } else if (errno_err == EEXIST) {
      is_shm_creator_ = false;
      shm_fd_ = shm_open(shm_name_.c_str(), O_RDWR, 0);
      if (shm_fd_ < 0) {
        errno_err = errno;
        return std::unexpected(UTrace(c_error(errno_err, "shm_open (attach)")));
      }
    } else {
      return std::unexpected(UTrace(c_error(errno_err, "shm_open (open)")));
    }

    return {};
  }

  void close_shm() {
    if (shm_fd_ != -1) {
      close(shm_fd_);
      shm_fd_ = -1;
    }

    if (destructor_counter_ == 0) {
      io_ln_infosp("Unlink the share memory");
      shm_unlink(shm_name_.c_str());
    }
  }

  auto memory_map() -> std::expected<void, UTrace> {
    void *ptr = mmap(nullptr, kTotalShmSize, PROT_READ | PROT_WRITE, MAP_SHARED,
                     shm_fd_, 0);
    if (ptr == MAP_FAILED) {
      const int errno_err = errno;
      return std::unexpected(UTrace(c_error(errno_err, "mmap")));
    }

    return mutex_create()
      .transform_error([&](UTrace &&e) {
        munmap(ptr, kTotalShmSize);
        utrace_transform_error(e);
      })
      .and_then([&]() -> std::expected<void, UTrace> {
        header_ = static_cast<ShmHeader *>(ptr);
        return {};
      });
  }

  void memory_unmap() {
    if (!header_)
      return;

    {
      LockGuard(mutex_shm_.value());
      if (header_->slot_attached_count == 0) {
        header_->magic = 0;
      }
      destructor_counter_ = header_->slot_attached_count;
    }

    mutex_destroy();
    munmap(header_, kTotalShmSize);
    header_ = nullptr;
  }
#endif

  auto mutex_create() -> std::expected<void, UTrace> {
    auto fn_destroy = [&](std::optional<process::GMutex> &mutex) {
      if (is_shm_creator_ && mutex.has_value()) {
        mutex->destroy();
        mutex.reset();
      }
    };

#if defined(_WIN32) || defined(_WIN64)
#  define E() process::GMutex::create(kMutexShmKey)
#else
#  define E() process::GMutex::create(&header_->mutex_shm, is_shm_creator_)
#endif
    return E()
#undef E
      .and_then([&](auto var) {
        mutex_shm_.emplace(std::move(var));
#if defined(_WIN32) || defined(_WIN64)
#  define E() process::GMutex::create(kMutexCrashKey)
#else
#  define E() process::GMutex::create(&header_->mutex_crash, is_shm_creator_)
#endif
        return E();
#undef E
      })
      .and_then([&](auto var) -> std::expected<void, UTrace> {
        mutex_crash_.emplace(std::move(var));
        return {};
      })
      .transform_error([&](UTrace &&e) {
        fn_destroy(mutex_shm_);
        utrace_transform_error(e);
      });
  }

  void mutex_destroy() {
    auto fn_destory = [&](std::optional<process::GMutex> &mutex) {
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
};

class GMutex {
#if defined(_WIN32) || defined(_WIN64)
  using hmutex = process::GMutex;
#else
  using hmutex = process::FMutex;
#endif

 private:
  GMutex() = default;

  ~GMutex() { destroy(); }

 public:
  GMutex(const GMutex &) = delete;
  GMutex &operator=(const GMutex &) = delete;
  GMutex(GMutex &&) = delete;
  GMutex &operator=(GMutex &&) = delete;

  /**
   * @throw `std::runtime_error`.
   */
  static GMutex &instance() {
    static GMutex instance;
    static std::once_flag once_flag;
    static bool initialized = false;

    std::string ret_msg;
    std::call_once(once_flag, [&ret_msg]() {
      if (auto ret = instance.create(); !ret.has_value()) {
        ret_msg = ret.error().join_self_all();
        return;
      }
      initialized = true;
    });
    if (!initialized)
      throw std::runtime_error(ret_msg);
    return instance;
  }

  auto lock() -> std::expected<void, UTrace> {
    return mutex_->lock()
      .and_then([&](auto) -> std::expected<void, UTrace> { return {}; })
      .utrace_transform_error_default();
  }

  auto unlock() -> std::expected<void, UTrace> {
    return mutex_->unlock().utrace_transform_error_default();
  }

  class LockGuard {
   public:
    LockGuard(const LockGuard &) = delete;
    LockGuard &operator=(const LockGuard &) = delete;
    LockGuard(LockGuard &&) = delete;
    LockGuard &operator=(LockGuard &&) = delete;

    /**
     * @throw `std::runtime_error`.
     */
    explicit LockGuard() {
      if (auto ret = mutex_.lock(); !ret.has_value())
        throw std::runtime_error(ret.error().join_self_all());
      owns_ = true;
    }

    ~LockGuard() noexcept {
      owns_ = false;
      if (auto ret = mutex_.unlock(); !ret.has_value()) {
        io_ln_error("{0}", ret.error().join_self_all());
      }
    }

   private:
    GMutex &mutex_ = GMutex::instance();
    bool owns_ = false;
  };

 private:
  std::optional<hmutex> mutex_ {};

  auto create() -> std::expected<void, UTrace> {
    return hmutex::create(kMutexProcessKey)
      .and_then([&](auto var) -> std::expected<void, UTrace> {
        mutex_.emplace(std::move(var));
        return {};
      })
      .utrace_transform_error_default();
  }

  void destroy() {
    if (!mutex_.has_value())
      return;
    mutex_->destroy();
    mutex_.reset();
  }
};
} // namespace log
} // namespace utils
} // namespace sirius
