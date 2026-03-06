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
namespace shm {
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

  process::t_pid pid;
  Buffer buffer;
};
#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

struct AttachedMap {
  process::t_pid pid;
  uint64_t timestamp_ms;
};

struct Header {
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

  /**
   * @note The parameters about `attached` should be guarded by the `mutex_shm`.
   */
  size_t attached_count;
  enum MasterType attached_master_type[kProcessMax];
  AttachedMap attached_maps[kProcessMax];

  std::atomic<uint64_t> write_index;
  std::atomic<uint64_t> read_index;

#if !defined(_WIN32) && !defined(_WIN64)
  pthread_mutex_t mutex_shm;
  pthread_mutex_t mutex_crash;
#endif

  uint64_t reserved[8];
};

class Shm {
#define SUFFIX_ \
  ( \
    std::format("\nPID: {0}" \
                "\n{1}", \
                static_cast<uint64_t>(process::pid()), utils_pretty_func))

 private:
  static constexpr size_t kHeaderOffset =
    (sizeof(Header) + kShmCacheLineSize - 1) & ~(kShmCacheLineSize - 1);
  static constexpr size_t kTotalShmSize =
    kHeaderOffset + (kShmCapacity * sizeof(Slot));

 private:
  class LockGuard {
   public:
    explicit LockGuard(process::mutex::GM &mutex) : mutex_(mutex) {
      if (auto ret = mutex_.lock(); ret.has_value()) {
        lock_state_ = ret.value();
      } else {
        throw std::runtime_error(ret.error());
      }
    }

    ~LockGuard() {
      if (lock_state_ == process::mutex::LockState::kSuccess ||
          lock_state_ == process::mutex::LockState::kOwnerDead) {
        (void)mutex_.unlock();
      }
    }

    LockGuard(const LockGuard &) = delete;
    LockGuard &operator=(const LockGuard &) = delete;

   private:
    process::mutex::GM &mutex_;
    process::mutex::LockState lock_state_ = process::mutex::LockState::kBusy;
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
    bool is_shm_creator() const {
      return parent_.is_shm_creator_;
    }

    // clang-format off
    LockGuard lock_guard() { return LockGuard(parent_.mutex_shm_.value()); }
    auto mutex_crash_lock() { return parent_.mutex_crash_->lock(); }
    auto mutex_crash_trylock() { return parent_.mutex_crash_->trylock(); }
    auto mutex_crash_unlock() { return parent_.mutex_crash_->unlock(); }
    // clang-format on

    Header *&get_shm_header() const {
      return header_;
    }

    Slot *get_shm_slots() const {
      uint8_t *base_ptr = reinterpret_cast<uint8_t *>(header_);

      return reinterpret_cast<Slot *>(base_ptr + kHeaderOffset);
    }

    void slots_free() {
      if (header_) {
        master_free();
      }
    }

    auto slots_alloc() -> std::expected<void, std::string> {
      if (is_shm_creator()) {
        header_->is_daemon_ready.store(false);
        header_->spawner_timestamp = 0;
        header_->attached_count = 0;
        for (auto &mt : header_->attached_master_type) {
          mt = MasterType::kNone;
        }
        header_->write_index.store(0);
        header_->read_index.store(0);

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

      return {};
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
        case process::mutex::LockState::kBusy:
          return DaemonState::kAlive;
        case process::mutex::LockState::kOwnerDead:
          state = DaemonState::kOwnerDead;
          break;
        default:
          state = DaemonState::kPerfectDead;
          break;
        }
        (void)mutex_crash_unlock();
        header_->is_daemon_ready.store(false, std::memory_order_relaxed);
      } else {
        io_errln(ret.error().append(Io::row_gs(utils_pretty_func)));
      }

      {
        lock_guard();

        for (size_t i = 0; i < kProcessMax; ++i) {
          if (header_->attached_master_type[i] == MasterType::kDaemon) {
            io_outln(IO_ISP("Reset the old daemon slot"));
            slot_reset(i);
            break;
          }
        }
      }

      return state;
    }

   private:
    Shm &parent_;
    Header *&header_;
    MasterType master_type_;
    std::jthread thread_guard_ {};
    std::atomic<bool> thread_guard_ready_ = false;

    auto master_alloc() -> std::expected<void, std::string> {
      io_outln(IO_ISP("Master: {0}. Alloc", static_cast<int>(master_type_)));

      switch (master_type_) {
      case MasterType::kDaemon:
        return daemon_master_alloc();
      case MasterType::kNative:
        return native_master_alloc();
      default:
        return std::unexpected(IO_E("Invalid argument{0}", SUFFIX_));
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

      io_outln(IO_ISP("Master: {0}. Free", static_cast<int>(master_type_)));
    }

    bool slot_check(size_t index) const {
      return (header_->attached_master_type[index] == master_type_ &&
              header_->attached_maps[index].pid == process::pid());
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
        io_errln(IO_WSP("No valid pid was matched{0}", SUFFIX_));
      }
    }

    auto slot_alloc() -> std::expected<void, std::string> {
      lock_guard();

      for (size_t i = 0; i < kProcessMax; ++i) {
        if (header_->attached_master_type[i] == MasterType::kNone) {
          ++header_->attached_count;
          header_->attached_master_type[i] = master_type_;
          header_->attached_maps[i].pid = process::pid();
          header_->attached_maps[i].timestamp_ms =
            time::get_monotonic_steady_ms();
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
    }

    auto daemon_master_alloc() -> std::expected<void, std::string> {
      if (daemon_state() == DaemonState::kAlive) {
        return std::unexpected(IO_WSP("Another daemon already exists"));
      }

      if (auto ret = slot_alloc(); !ret.has_value())
        return std::unexpected(ret.error());

      thread_guard_ =
        std::jthread(std::bind_front(&Master::daemon_thread_guard, this));

      return {};
    }

    void daemon_thread_guard(std::stop_token stop_token) {
      constexpr uint64_t kOnceSleepMs = 2000;
      int splits = kProcessGuardTimeoutMilliseconds / kOnceSleepMs;
      splits = UTILS_MAX(1, splits);

      while (!stop_token.stop_requested()) {
        for (int i = 0; i < splits; ++i) {
          std::this_thread::sleep_for(std::chrono::milliseconds(kOnceSleepMs));
          if (stop_token.stop_requested())
            return;
        }

        {
          uint64_t timestamp_ms = time::get_monotonic_steady_ms();
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

      thread_guard_ready_.store(true, std::memory_order_relaxed);

      /**
       * @note Although the probability of occurrence is not high, the
       * initialization function may be called repeatedly. Slightly shortening
       * the duration of a single sleep can help threads exit quickly.
       */
      constexpr int kOnceSleepMs = 1000;
      int splits = kProcessFeedGuardMilliseconds / kOnceSleepMs;
      splits = UTILS_MAX(1, splits);

      while (!stop_token.stop_requested()) {
        {
          uint64_t timestamp_ms = time::get_monotonic_steady_ms();
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
  Shm() = default;

  ~Shm() {
    shm_free();
  }

 public:
  Shm(const Shm &) = delete;
  Shm &operator=(const Shm &) = delete;

  static Shm &instance() {
    static Shm instance;

    return instance;
  }

  auto shm_alloc(enum MasterType master_type)
    -> std::expected<std::unique_ptr<Master>, std::string> {
    if (initialized_.exchange(true, std::memory_order_seq_cst)) {
      return std::unexpected(IO_E("Repeated initialization{0}", SUFFIX_));
    }

    std::string ret_msg;

    if (auto ret = open_shm(); !ret.has_value()) {
      ret_msg = ret.error();
      goto label_free1;
    }
    if (auto ret = memory_map(); !ret.has_value()) {
      ret_msg = ret.error();
      goto label_free2;
    }

    return std::make_unique<Master>(MasterToken {}, *this, master_type);

  label_free2:
    close_shm();
  label_free1:
    initialized_.store(false, std::memory_order_seq_cst);

    return std::unexpected(ret_msg);
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
  Header *header_ = nullptr;
  std::optional<process::mutex::GM> mutex_shm_ {}, mutex_crash_ {};

#if defined(_WIN32) || defined(_WIN64)
  auto open_shm() -> std::expected<void, std::string> {
    DWORD size_high = (DWORD)((uint64_t)kTotalShmSize >> 32);
    DWORD size_low = (DWORD)((uint64_t)kTotalShmSize & 0xFFFFFFFF);

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
      MapViewOfFile(shm_handle_, FILE_MAP_ALL_ACCESS, 0, 0, kTotalShmSize);
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
    {
      LockGuard(mutex_shm_.value());

      if (header_->attached_count == 0) {
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
  auto open_shm() -> std::expected<void, std::string> {
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
    void *ptr = mmap(nullptr, kTotalShmSize, PROT_READ | PROT_WRITE, MAP_SHARED,
                     shm_fd_, 0);
    if (ptr == MAP_FAILED) {
      const int errno_err = errno;
      return std::unexpected(
        IO_E("{}", Io::errno_err(errno_err, "mmap", "{}", SUFFIX_)));
    }
    header_ = static_cast<Header *>(ptr);

    if (auto ret = mutex_create(); !ret.has_value()) {
      munmap(header_, kTotalShmSize);
      header_ = nullptr;
      return std::unexpected(ret.error());
    }

    return {};
  }

  void memory_unmap() {
    if (!header_)
      return;

    {
      LockGuard(mutex_shm_.value());

      if (header_->attached_count == 0) {
        header_->magic = 0;
      }
      destructor_counter_ = header_->attached_count;
    }

    mutex_destroy();

    munmap(header_, kTotalShmSize);
    header_ = nullptr;
  }
#endif

  auto mutex_create() -> std::expected<void, std::string> {
#if defined(_WIN32) || defined(_WIN64)
#  define fn_create() process::mutex::GM::create(T)
#else
#  define fn_create() process::mutex::GM::create(T, is_shm_creator_)
#endif

    auto fn_destroy = [&](std::optional<process::mutex::GM> &mutex) {
      if (is_shm_creator_) {
        mutex->destroy();
        mutex.reset();
      }
    };

#if defined(_WIN32) || defined(_WIN64)
#  define T (kMutexShmKey)
#else
#  define T (&header_->mutex_shm)
#endif
    if (auto ret = fn_create(); !ret.has_value()) {
      return std::unexpected(ret.error().append(Io::row_gs("{0}", SUFFIX_)));
    } else {
      mutex_shm_.emplace(std::move(ret.value()));
    }
#undef T

#if defined(_WIN32) || defined(_WIN64)
#  define T (kMutexCrashKey)
#else
#  define T (&header_->mutex_crash)
#endif
    if (auto ret = fn_create(); !ret.has_value()) {
      if (is_shm_creator_) {
        fn_destroy(mutex_shm_);
      }
      return std::unexpected(ret.error().append(Io::row_gs("{0}", SUFFIX_)));
    } else {
      mutex_crash_.emplace(std::move(ret.value()));
    }
#undef T

    return {};
#undef fn_create
  }

  void mutex_destroy() {
    auto fn_destory = [&](std::optional<process::mutex::GM> &mutex) {
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

class GMutex {
#if defined(_WIN32) || defined(_WIN64)
  using hmutex = process::mutex::GM;
#else
  using hmutex = process::mutex::FM;
#endif

 private:
  GMutex() = default;

  ~GMutex() {
    destroy();
  }

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
        ret_msg = ret.error();
        return;
      }
      initialized = true;
    });

    if (!initialized)
      throw std::runtime_error(ret_msg);

    return instance;
  }

  auto lock() -> std::expected<void, std::string> {
    if (auto ret = mutex_->lock(); !ret.has_value()) {
      return std::unexpected(
        ret.error().append(Io::row_gs("\n{0}", utils_pretty_func)));
    }

    return {};
  }

  auto unlock() -> std::expected<void, std::string> {
    return mutex_->unlock();
  }

 private:
  std::optional<hmutex> mutex_ {};

  auto create() -> std::expected<void, std::string> {
    auto ret = hmutex::create(kMutexProcessKey);

    if (!ret.has_value()) {
      return std::unexpected(
        ret.error().append(Io::row_gs("\n{0}", utils_pretty_func)));
    }

    mutex_.emplace(std::move(ret.value()));

    return {};
  }

  void destroy() {
    if (!mutex_.has_value())
      return;

    mutex_->destroy();
    mutex_.reset();
  }
};
} // namespace shm
} // namespace log
} // namespace utils
} // namespace sirius
