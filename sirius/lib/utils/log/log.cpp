#include "utils/log/log.h"

#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "sirius/utils/thread.h"
#include "utils/initializer.h"

#define UTILS_LOG_LOG_HPP_
#include "utils/log/log.hpp"

#if defined(_WIN32) || defined(_WIN64)

#else
#  include <sys/mman.h>
#endif

#ifdef __cpp_lib_hardware_interference_size
using std::hardware_destructive_interference_size;
#else
constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

enum class SlotState : int {
  FREE = 0,
  WRITING = 1,
  READY = 2,
};

struct LogBuffer {
  int level;

  size_t data_size;
  char data[sirius_log_buf_size];
};

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4324)
#endif
struct alignas(hardware_destructive_interference_size) ShmSlot {
  std::atomic<SlotState> state;
  std::atomic<uint64_t> timestamp_ms;

  LogBuffer buffer;
};
#ifdef _MSC_VER
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

struct ShmHeader {
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
  bool attached_used[LOG_PROCESS_MAX];
  AttachedMap attached_maps[LOG_PROCESS_MAX];

  std::atomic<uint64_t> write_index;
  std::atomic<uint64_t> read_index;
  size_t capacity;
  size_t item_size;

  uint64_t reserved[8];
};

// --- Error ---
#if defined(_WIN32) || defined(_WIN64)
[[maybe_unused]] static inline void win_last_error(const char *function) {
  DWORD error_code = GetLastError();
  char e[sirius_log_buf_size] = {0};

  DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD size = FormatMessage(flags, nullptr, error_code,
                             MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), e,
                             sizeof(e) / sizeof(TCHAR), nullptr);
  if (size == 0) [[unlikely]] {
    UTILS_DPRINTF(STDERR_FILENO, log_level_str_error LOG_COMMON "%s: %lu\n",
                  function, error_code);
  } else {
    UTILS_DPRINTF(STDERR_FILENO, log_level_str_error LOG_COMMON "%s: %lu. %s",
                  function, error_code, e);
  }
}
#endif

[[maybe_unused]] static inline void errno_error(const char *function) {
  int error_code = errno;
  char e[sirius_log_buf_size];

  if (0 == UTILS_STRERROR_R(error_code, e, sizeof(e))) [[likely]] {
    UTILS_DPRINTF(STDERR_FILENO, log_level_str_error LOG_COMMON "%s: %d. %s",
                  function, error_code, e);
  } else {
    UTILS_DPRINTF(STDERR_FILENO, log_level_str_error LOG_COMMON "%s: %d\n",
                  function, error_code);
  }
}

#if defined(_WIN32) || defined(_WIN64)
namespace WinLock {
static inline bool create(HANDLE &handle, const char *name) {
  handle = CreateMutexA(nullptr, FALSE, Utils::Ns::win_lock_name(name).c_str());
  if (!handle) {
    win_last_error("CreateMutexA");
    return false;
  }

  return true;
}

static inline void destory(HANDLE &handle) {
  if (handle != nullptr) {
    CloseHandle(handle);
    handle = nullptr;
  }
}

static inline bool lock(HANDLE &handle) {
  while (true) {
    DWORD dw_err = WaitForSingleObject(handle, INFINITE);
    switch (dw_err) {
    [[likely]] case WAIT_OBJECT_0:
      return true;
    case WAIT_ABANDONED:
      UTILS_DPRINTF(
        STDOUT_FILENO,
        log_level_str_warn LOG_COMMON
        "A process that previously held the lock may have crashed\n");
      continue;
    case WAIT_FAILED:
      win_last_error("WaitForSingleObject");
      return false;
    default:
      return false;
    }
  }
}

static inline bool unlock(HANDLE &handle) {
  if (!ReleaseMutex(handle)) {
    win_last_error("WaitForSingleObject");
    return false;
  }

  return true;
}
} // namespace WinLock

namespace ProcessLock {
static HANDLE process_lock_;

static inline bool create() {
  return WinLock::create(process_lock_, LOG_MUTEX_PROCESS);
}

static inline void destory() {
  return WinLock::destory(process_lock_);
}

static inline bool lock() {
  return WinLock::lock(process_lock_);
}

static inline bool unlock() {
  return WinLock::unlock(process_lock_);
}
} // namespace ProcessLock
#else
namespace ProcessLock {
static int process_lock_ = -1;

static inline bool create() {
  auto path = Utils::Ns::posix_lockfile_path(LOG_MUTEX_PROCESS);
  process_lock_ = open(path.c_str(), O_RDWR | O_CREAT,
                       Utils::File::string_to_mode(sirius_posix_file_mode));
  if (process_lock_ == -1) {
    errno_error("open");
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
  struct flock lock;
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;

  if (fcntl(process_lock_, F_SETLKW, &lock) == -1) {
    errno_error("fcntl lock");
    return false;
  }

  return true;
}

static inline bool unlock() {
  struct flock lock;
  lock.l_type = F_UNLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;

  if (fcntl(process_lock_, F_SETLK, &lock) == -1) {
    errno_error("fcntl unlock");
    return false;
  }

  return true;
}
} // namespace ProcessLock
#endif

class LogShm {
 public:
  bool shm_creator = false;
  ShmHeader *header = nullptr;
  ShmSlot *slots = nullptr;

#if defined(_WIN32) || defined(_WIN64)
  LogShm()
      : shm_name_(Utils::Ns::get_shm_name(LOG_KEY)),
        shm_handle_(nullptr),
        mapped_size_(0) {}
#else
  LogShm()
      : shm_name_(Utils::Ns::get_shm_name(LOG_KEY)),
        shm_fd_(-1),
        mapped_size_(0) {}
#endif

  ~LogShm() {}

// --- Shared Memory ---
#if defined(_WIN32) || defined(_WIN64)
  bool open_shm() {
    size_t size_needed = calculate_size();

    DWORD size_high = (DWORD)((uint64_t)size_needed >> 32);
    DWORD size_low = (DWORD)((uint64_t)size_needed & 0xFFFFFFFF);

    shm_handle_ =
      CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                         size_high, size_low, shm_name_.c_str());

    if (shm_handle_ == nullptr) {
      win_last_error("CreateFileMappingA");
      return false;
    }

    shm_creator = GetLastError() == ERROR_ALREADY_EXISTS ? false : true;

    mapped_size_ = size_needed;

    if (!shm_lock_create()) {
      CloseHandle(shm_handle_);
      shm_handle_ = nullptr;
      return false;
    }

    return true;
  }

  void close_shm(bool shm_destory) {
    (void)shm_destory;

    shm_lock_destory();

    if (shm_handle_ != nullptr) {
      CloseHandle(shm_handle_);
      shm_handle_ = nullptr;
    }
  }

  bool memory_map() {
    void *ptr =
      MapViewOfFile(shm_handle_, FILE_MAP_ALL_ACCESS, 0, 0, mapped_size_);
    if (ptr == nullptr) {
      win_last_error("MapViewOfFile");
      return false;
    }
    header = static_cast<ShmHeader *>(ptr);

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
    size_t size_needed = calculate_size();

    shm_fd_ = shm_open(shm_name_.c_str(), O_CREAT | O_EXCL | O_RDWR,
                       Utils::File::string_to_mode(sirius_posix_file_mode));

    if (shm_fd_ >= 0) {
      shm_creator = true;
      if (ftruncate(shm_fd_, size_needed) == -1) {
        errno_error("ftruncate");
        shm_unlink(shm_name_.c_str());
        close(shm_fd_);
        return false;
      }
    } else if (errno == EEXIST) {
      shm_creator = false;
      shm_fd_ = shm_open(shm_name_.c_str(), O_RDWR, 0);
      if (shm_fd_ < 0) {
        errno_error("shm_open attach");
        return false;
      }
    } else {
      errno_error("shm_open open");
      return false;
    }

    mapped_size_ = size_needed;

    return true;
  }

  void close_shm(bool shm_destory) {
    if (shm_fd_ != -1) {
      close(shm_fd_);
      shm_fd_ = -1;
    }

    if (shm_destory) {
      UTILS_DPRINTF(STDOUT_FILENO,
                    log_level_str_info LOG_COMMON "Unlink the share memory\n");
      shm_unlink(shm_name_.c_str());
    }
  }

  bool memory_map() {
    void *ptr = mmap(nullptr, mapped_size_, PROT_READ | PROT_WRITE, MAP_SHARED,
                     shm_fd_, 0);
    if (ptr == MAP_FAILED) {
      errno_error("mmap");
      return false;
    }
    header = static_cast<ShmHeader *>(ptr);

    return true;
  }

  void memory_unmap() {
    if (header) {
      munmap(header, mapped_size_);
      header = nullptr;
      slots = nullptr;
    }
  }
#endif

// --- Mutex ---
#if defined(_WIN32) || defined(_WIN64)
  bool shm_lock_lock() {
    return WinLock::lock(shm_lock_);
  }

  bool shm_lock_unlock() {
    return WinLock::unlock(shm_lock_);
  }
#else
#endif

  void slots_deinit(bool is_daemon) {
    slots_header_deinit(is_daemon);
  }

  bool slots_init(bool is_daemon) {
    uint8_t *base_ptr = reinterpret_cast<uint8_t *>(header);
    size_t header_size = sizeof(ShmHeader);

    size_t align_offset =
      (header_size + hardware_destructive_interference_size - 1) &
      ~(hardware_destructive_interference_size - 1);

    slots = reinterpret_cast<ShmSlot *>(base_ptr + align_offset);

    return slots_header_init(is_daemon);
  }

 private:
  std::string shm_name_;
#if defined(_WIN32) || defined(_WIN64)
  HANDLE shm_handle_;
  HANDLE shm_lock_;
#else
  int shm_fd_;
#endif
  size_t mapped_size_;
  std::thread thread_attached_;
  std::atomic<bool> thread_attached_running_;
  std::thread thread_daemon_;
  std::atomic<bool> thread_daemon_running_;

  size_t calculate_size() {
    size_t header_aligned = (sizeof(ShmHeader) + 63) & ~63;
    return header_aligned + (LOG_SHM_CAPACITY * sizeof(ShmSlot));
  }

  auto get_process_id() {
#if defined(_WIN32) || defined(_WIN64)
    return GetCurrentProcessId();
#else
    return = getpid();
#endif
  }

  void attached_count_sub1() {
    header->attached_count =
      header->attached_count ? header->attached_count - 1 : 0;
  }

  void thread_attached() {
    size_t index;

    thread_attached_running_.store(true);

    auto pid = get_process_id();

    shm_lock_lock();

    for (index = 0; index < LOG_PROCESS_MAX; ++index) {
      if (header->attached_used[index] &&
          header->attached_maps[index].pid == pid) {
        break;
      }
      if (index == LOG_PROCESS_MAX - 1) {
        UTILS_DPRINTF(STDERR_FILENO,
                      log_level_str_error LOG_NATIVE
                      "No valid pid was matched\n");
        thread_attached_running_.store(false);
      }
    }

    shm_lock_unlock();

    while (thread_attached_running_.load(std::memory_order_relaxed)) {
      uint64_t timestamp_ms = Utils::Time::get_monotonic_steady_ms();

      shm_lock_lock();

      if (header->attached_used[index] &&
          header->attached_maps[index].pid == pid) [[likely]] {
        header->attached_maps[index].timestamp_ms = timestamp_ms;
      } else {
        UTILS_DPRINTF(STDERR_FILENO,
                      log_level_str_warn LOG_NATIVE
                      "Invalid arguments. `attached` thread exit\n");
        thread_attached_running_.store(false, std::memory_order_relaxed);
      }

      shm_lock_unlock();

      std::this_thread::sleep_for(
        std::chrono::milliseconds(LOG_PROCESS_FEED_GUARD_MS));
    }
  }

  void thread_daemon() {
    thread_daemon_running_.store(true);

    const int once = 1000;
    int splits = LOG_PROCESS_GUARD_TIMEOUT_MS / once;
    splits = UTILS_MAX(1, splits);

    while (thread_daemon_running_.load(std::memory_order_relaxed)) {
      for (int i = 0; i < splits; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(once));
        if (thread_daemon_running_.load(std::memory_order_relaxed))
          continue;
        break;
      }

      uint64_t timestamp_ms = Utils::Time::get_monotonic_steady_ms();

      shm_lock_lock();

      if (header->attached_count == 0) {
        shm_lock_unlock();
        break;
      }

      for (size_t i = 0; i < LOG_PROCESS_MAX; ++i) {
        if (!header->attached_used[i])
          continue;

        AttachedMap *am = header->attached_maps + i;
        if (timestamp_ms - am->timestamp_ms < LOG_PROCESS_GUARD_TIMEOUT_MS)
          [[likely]] {
          continue;
        }

        std::memset(am, 0, sizeof(AttachedMap));
        header->attached_used[i] = false;
        attached_count_sub1();
      }

      shm_lock_unlock();
    }
  }

  void slot_attached_free() {
    auto pid = get_process_id();

    thread_attached_running_.store(false, std::memory_order_relaxed);

    shm_lock_lock();

    for (size_t i = 0; i < LOG_PROCESS_MAX; ++i) {
      AttachedMap *am = header->attached_maps + i;
      if (header->attached_used[i] && am->pid == pid) {
        std::memset(am, 0, sizeof(AttachedMap));
        header->attached_used[i] = false;
        attached_count_sub1();
        break;
      }
      if (i == LOG_PROCESS_MAX - 1) {
        UTILS_DPRINTF(STDERR_FILENO,
                      log_level_str_warn LOG_NATIVE
                      "No valid pid was matched\n");
      }
    }

    shm_lock_unlock();

    UTILS_DPRINTF(STDOUT_FILENO,
                  log_level_str_info LOG_NATIVE "PID: %" PRIu64 ", slot free\n",
                  (uint64_t)pid);
  }

  bool slot_attached_alloc() {
    auto pid = get_process_id();

    shm_lock_lock();

    for (size_t i = 0; i < LOG_PROCESS_MAX; ++i) {
      if (!header->attached_used[i]) {
        ++header->attached_count;
        header->attached_used[i] = true;
        header->attached_maps[i].pid = pid;
        header->attached_maps[i].timestamp_ms =
          Utils::Time::get_monotonic_steady_ms();
        shm_lock_unlock();
        break;
      }
      if (i == LOG_PROCESS_MAX - 1) {
        UTILS_DPRINTF(STDERR_FILENO,
                      log_level_str_error LOG_NATIVE
                      "Cache full. Too many processes\n");
        shm_lock_unlock();
        return false;
      }
    }

    thread_attached_ = std::thread([this]() {
      this->thread_attached();
    });
    thread_attached_.detach();

    UTILS_DPRINTF(STDOUT_FILENO,
                  log_level_str_info LOG_NATIVE "PID: %" PRIu64
                                                ", slot alloc\n",
                  (uint64_t)pid);

    return true;
  }

  void slot_daemon_free() {
    auto pid = get_process_id();

    thread_daemon_running_.store(false, std::memory_order_relaxed);

    UTILS_DPRINTF(STDOUT_FILENO,
                  log_level_str_info LOG_DAEMON "PID: %" PRIu64 ", slot free\n",
                  (uint64_t)pid);
  }

  bool slot_daemon_alloc() {
    auto pid = get_process_id();

    thread_daemon_ = std::thread([this]() {
      this->thread_daemon();
    });
    thread_daemon_.detach();

    UTILS_DPRINTF(STDOUT_FILENO,
                  log_level_str_info LOG_DAEMON "PID: %" PRIu64
                                                ", slot alloc\n",
                  (uint64_t)pid);

    return true;
  }

  void slots_header_deinit(bool is_daemon) {
    is_daemon ? slot_daemon_free() : slot_attached_free();
  }

  bool slots_header_init(bool is_daemon) {
    if (!shm_creator) {
      if (header->magic != ShmHeader::MAGIC) {
        UTILS_DPRINTF(STDERR_FILENO,
                      log_level_str_error LOG_COMMON
                      "Invalid argument. `magic`: %u\n",
                      header->magic);
        return false;
      }
      return is_daemon ? slot_daemon_alloc() : slot_attached_alloc();
    }

    header->is_daemon_ready.store(false);
    header->attached_count = 0;
    for (size_t i = 0; i < LOG_PROCESS_MAX; ++i) {
      header->attached_used[i] = false;
    }
    header->write_index.store(0);
    header->read_index.store(0);
    header->capacity = LOG_SHM_CAPACITY;
    header->item_size = sizeof(ShmSlot);

    slot_attached_alloc();

    header->magic = ShmHeader::MAGIC;

    return true;
  }

// --- Mutex ---
#if defined(_WIN32) || defined(_WIN64)
  bool shm_lock_create() {
    return WinLock::create(shm_lock_, LOG_MUTEX_SHM);
  }

  void shm_lock_destory() {
    return WinLock::destory(shm_lock_);
  }
#else
#endif
};

// --- Log Manager ---
class LogManager {
 public:
  std::mutex mutex;
  // ------------------ modify the fd -----!!!!!!!
  int fd_stdout = STDOUT_FILENO;
  int fd_stderr = STDERR_FILENO;

  LogManager(bool is_daemon, std::string daemon_arg)
      : is_creator_(false),
        is_daemon_(is_daemon),
        daemon_arg_(daemon_arg),
        log_shm_(std::make_unique<LogShm>()) {}

  ~LogManager() {}

  bool init() {
    if (!ProcessLock::create())
      return false;
    if (!ProcessLock::lock())
      goto label_free1;
    if (!log_shm_->open_shm())
      goto label_free2;
    is_creator_ = log_shm_->shm_creator;
    if (!log_shm_->memory_map())
      goto label_free3;
    if (!log_shm_->slots_init(is_daemon_))
      goto label_free4;

    ProcessLock::unlock();

    if (!start_daemon())
      goto label_free5;
    if (is_daemon_)
      return true;
    if (!native_wait_for_daemon())
      goto label_free5;

    return true;

  label_free5:
    ProcessLock::lock();
    log_shm_->slots_deinit(is_daemon_);
  label_free4:
    log_shm_->memory_unmap();
  label_free3:
    log_shm_->close_shm(is_creator_);
  label_free2:
    ProcessLock::unlock();
  label_free1:
    ProcessLock::destory();

    return false;
  }

  void deinit() {
    ProcessLock::lock();

    log_shm_->slots_deinit(is_daemon_);
    log_shm_->memory_unmap();
    log_shm_->close_shm(false);

    ProcessLock::unlock();

    ProcessLock::destory();
  }

  void native_produce(void *src, size_t size) {
    if (!log_shm_->header->is_daemon_ready.load(std::memory_order_relaxed))
      [[unlikely]] {
      auto buffer = (LogBuffer *)src;
      log_write(buffer->level, (void *)buffer->data, buffer->data_size);
      return;
    }

    uint64_t cur_write_idx =
      log_shm_->header->write_index.fetch_add(1, std::memory_order_acq_rel);
    size_t slot_idx = cur_write_idx & (LOG_SHM_CAPACITY - 1);
    ShmSlot &slot = log_shm_->slots[slot_idx];

    int retries = 0;
    while (slot.state.load(std::memory_order_acquire) != SlotState::FREE) {
      if (++retries > 1000) {
        std::this_thread::yield();
      }
    }

    slot.timestamp_ms.store(Utils::Time::get_monotonic_steady_ms(),
                            std::memory_order_relaxed);
    slot.state.store(SlotState::WRITING, std::memory_order_release);

    std::memcpy(&slot.buffer, src, size);

    slot.state.store(SlotState::READY, std::memory_order_release);
  }

  void log_write(int level, const void *buffer, size_t size) {
    std::lock_guard<std::mutex> lock(mutex);

    int fd = level <= sirius_log_level_warn ? fd_stderr : fd_stdout;

    UTILS_WRITE(fd, buffer, size);
  }

 private:
  bool is_creator_;
  bool is_daemon_;
  std::string daemon_arg_;
  std::unique_ptr<LogShm> log_shm_;

  std::thread daemon_thread_monitor_;
  std::thread daemon_thread_consumer_;
  std::atomic<bool> daemon_thread_running_;

#if defined(_WIN32) || defined(_WIN64)
  bool restart_self() {
    char module_path[MAX_PATH];
    if (!GetModuleFileNameA(nullptr, module_path, MAX_PATH)) {
      win_last_error("GetModuleFileNameA");
      return false;
    }

    LPSTR sz_cmd_line = GetCommandLineA();
    std::string new_cmd_line = sz_cmd_line;

    if (!new_cmd_line.empty() && new_cmd_line.back() != ' ') {
      new_cmd_line += " ";
    }
    new_cmd_line += daemon_arg_;

    STARTUPINFOA si {};
    si.cb = sizeof(STARTUPINFOA);
    PROCESS_INFORMATION pi {};

    BOOL ret = CreateProcessA(module_path, new_cmd_line.data(), nullptr,
                              nullptr, TRUE, 0, nullptr, nullptr, &si, &pi);
    if (!ret) {
      win_last_error("CreateProcessA");
      return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    UTILS_DPRINTF(STDOUT_FILENO,
                  log_level_str_info LOG_NATIVE "Try to start the daemon\n");

    return true;
  }
#else
#endif

  bool start_daemon() {
    if (is_daemon_)
      return daemon_main();

    return is_creator_ ? restart_self() : true;
  }

  bool daemon_main() {
    auto header = log_shm_->header;

    UTILS_DPRINTF(STDOUT_FILENO,
                  log_level_str_info LOG_DAEMON "The daemon starts\n");

    daemon_thread_running_.store(true, std::memory_order_relaxed);
    // Loader Lock   Deadlock !!!!!!!!!!!!
    UTILS_DPRINTF(STDERR_FILENO,
                  "The thread fail to start under shared libs!\n");
    // Loader Lock   Deadlock !!!!!!!!!!!!
    daemon_thread_consumer_ = std::thread([this]() {
      this->daemon_thread_consumer();
    });
    if (daemon_thread_consumer_.joinable()) {
      UTILS_DPRINTF(STDERR_FILENO, "jjjjjjjjjjjjj\n");
    } else {
      UTILS_DPRINTF(STDERR_FILENO, "qqqqqqqqqqqqq\n");
    }
    daemon_thread_monitor_ = std::thread([this]() {
      this->daemon_thread_monitor();
    });

    header->is_daemon_ready.store(true, std::memory_order_relaxed);

    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      ProcessLock::lock();
      log_shm_->shm_lock_lock();

      if (header->attached_count == 0) [[unlikely]] {
        header->magic = 0;
        header->is_daemon_ready.store(false, std::memory_order_relaxed);
        log_shm_->shm_lock_unlock();
        break;
      }

      log_shm_->shm_lock_unlock();
      ProcessLock::unlock();
    }

    daemon_thread_running_.store(false, std::memory_order_relaxed);
    if (daemon_thread_monitor_.joinable()) {
      daemon_thread_monitor_.join();
    }
    if (daemon_thread_consumer_.joinable()) {
      daemon_thread_consumer_.join();
    }

    log_shm_->slots_deinit(is_daemon_);
    log_shm_->memory_unmap();
    log_shm_->close_shm(false);

    ProcessLock::unlock();
    ProcessLock::destory();

    UTILS_DPRINTF(STDOUT_FILENO,
                  log_level_str_info LOG_DAEMON "The daemon ended\n");

    return true;
  }

  bool native_wait_for_daemon() {
    auto header = log_shm_->header;

    int retries = 0;
    const int retry_times = 600;
    while (!header->is_daemon_ready.load(std::memory_order_relaxed)) {
      if (retries++ > retry_times) {
        UTILS_DPRINTF(STDERR_FILENO,
                      log_level_str_error LOG_NATIVE "No daemon was found\n");
        return false;
      }
      if (retries % 100 == 0) {
        UTILS_DPRINTF(
          STDOUT_FILENO,
          log_level_str_info LOG_NATIVE
          "Trying to acquire daemon. Total attempts: %d; Attempted times: %d\n",
          retry_times, retries);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return true;
  }

  void daemon_thread_consumer() {
    auto header = log_shm_->header;

    while (daemon_thread_running_.load(std::memory_order_relaxed) ||
           header->read_index.load(std::memory_order_acquire) <
             header->write_index.load(std::memory_order_acquire)) {
      uint64_t cur_read_idx =
        header->read_index.load(std::memory_order_acquire);

      if (cur_read_idx >= header->write_index.load(std::memory_order_acquire)) {
        if (!daemon_thread_running_.load(std::memory_order_relaxed))
          break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }

      auto opt_slot = daemon_get_slot(cur_read_idx, 100);
      if (opt_slot) {
        ShmSlot &slot = opt_slot->get();
        LogBuffer &buffer = slot.buffer;
        log_write(buffer.level, (void *)buffer.data, buffer.data_size);
        slot.state.store(SlotState::FREE, std::memory_order_release);
      }

      header->read_index.fetch_add(1, std::memory_order_release);
    }
  }

  void daemon_thread_monitor() {
    auto header = log_shm_->header;
    auto slots = log_shm_->slots;

    while (daemon_thread_running_.load(std::memory_order_relaxed)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));

      uint64_t now = Utils::Time::get_monotonic_steady_ms();

      for (uint32_t i = 0; i < header->capacity; ++i) {
        SlotState s = slots[i].state.load(std::memory_order_acquire);

        if (s == SlotState::WRITING) {
          uint64_t ts = slots[i].timestamp_ms.load(std::memory_order_relaxed);
          if (now - ts > LOG_SLOT_RESET_TIMEOUT_MS) {
            {
              std::lock_guard<std::mutex> lock(mutex);
              UTILS_DPRINTF(
                fd_stdout,
                log_level_str_warn LOG_DAEMON "Recovering stuck slot: %u\n", i);
            }

            /**
             * @note Construct a forged log indicating data loss.
             */
            const char *err_msg =
              "[System Error] Slot recovered/skipped due to timeout\n";
            std::memcpy(slots[i].buffer.data, err_msg, strlen(err_msg));
            slots[i].buffer.data_size = std::strlen(err_msg);
            slots[i].buffer.level = sirius_log_level_error;

            /**
             * @note Let it be `READY`, so that consumers can read it, thereby
             * advancing the index.
             */
            slots[i].state.store(SlotState::READY, std::memory_order_release);
          }
        }
      }
    }
  }

  [[nodiscard]] std::optional<std::reference_wrapper<ShmSlot>>
  daemon_get_slot(const uint64_t read_index, const int retry_times) {
    size_t slot_idx = read_index & (LOG_SHM_CAPACITY - 1);
    ShmSlot &slot = log_shm_->slots[slot_idx];

    int retries = 0;
    while (slot.state.load(std::memory_order_acquire) != SlotState::READY) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      if (++retries > retry_times) {
        UTILS_DPRINTF(fd_stderr,
                      log_level_str_warn LOG_DAEMON
                      "Skip corrupted slot index: %" PRIu64 "\n",
                      read_index);
        return std::nullopt;
      }
    }

    return slot;
  }
};

// --- Global Instance & Hooks ---

static std::string g_daemon_arg;
static bool g_is_daemon = false;
static LogManager *g_log_manager = nullptr;

#if defined(_WIN32) || defined(_WIN64)
static inline std::string daemon_arg() {
  return std::string(sirius_namespace) + "_" +
    Utils::Ns::generate_namespace_prefix(LOG_DAEMON_ARG_KEY);
}

static inline bool daemon_check(std::string daemon_arg) {
  LPSTR sz_cmd_line = GetCommandLineA();
  std::string cmd_line(sz_cmd_line);
  size_t found = cmd_line.find(daemon_arg);

  return found != std::string::npos;
}

static inline void win_enable_ansi() {
#  if defined(NTDDI_VERSION) && (NTDDI_VERSION >= 0x0A000002)
  HANDLE h_out = GetStdHandle(STD_OUTPUT_HANDLE);
  HANDLE h_err = GetStdHandle(STD_ERROR_HANDLE);
  DWORD mode = 0;

  if (GetConsoleMode(h_out, &mode)) {
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(h_out, mode);
  }
  if (GetConsoleMode(h_err, &mode)) {
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(h_err, mode);
  }
#  endif
}

#endif

extern "C" bool constructor_utils_log() {
#if defined(_WIN32) || defined(_WIN64)
  win_enable_ansi();
#endif

  g_daemon_arg = daemon_arg();
  g_is_daemon = daemon_check(g_daemon_arg);
  g_log_manager = new LogManager(g_is_daemon, g_daemon_arg);

  bool ret = g_log_manager->init();

  if (!ret || g_is_daemon) {
    delete g_log_manager;
    g_log_manager = nullptr;
  }

  if (g_is_daemon)
    std::exit(ret ? 0 : -1);

  return ret;
}

extern "C" void destructor_utils_log() {
  if (g_log_manager) {
    g_log_manager->deinit();
    delete g_log_manager;
    g_log_manager = nullptr;
  }
}

extern "C" void sirius_log_configure(sirius_log_config_t cfg) {
  std::lock_guard<std::mutex> lock(g_log_manager->mutex);

  if (cfg.fd_err && *cfg.fd_err >= 0) {
    g_log_manager->fd_stderr = *cfg.fd_err;
  }

  if (cfg.fd_out && *cfg.fd_out >= 0) {
    g_log_manager->fd_stdout = *cfg.fd_out;
  }
}

#define ISSUE_ERROR_STANDARD_FUNCTION(function) \
  do { \
    const char *e = \
      "\n" \
      "  The LOG module issues an ERROR\n" \
      "  " function " error\n"; \
    g_log_manager->log_write(sirius_log_level_error, e, strlen(e)); \
  } while (0)

#define ISSUE_ERROR_LOG_LOST() \
  do { \
    const char *e = \
      "\n" \
      "  The LOG module issues an ERROR\n" \
      "  Log length exceeds the buffer, log will be lost\n"; \
    g_log_manager->log_write(sirius_log_level_error, e, strlen(e)); \
  } while (0)

#define ISSUE_WARNING_LOG_TRUNCATED() \
  do { \
    const char *e = \
      "\n" \
      "  The LOG module issues a WARNING\n" \
      "  Log length exceeds the buffer, log will be truncated\n"; \
    g_log_manager->log_write(sirius_log_level_error, e, strlen(e)); \
  } while (0)

extern "C" void sirius_log_impl(int level, const char *level_str,
                                const char *color, const char *module,
                                const char *file, const char *func, int line,
                                const char *fmt, ...) {
  LogBuffer buffer {};
  int len = 0;

  buffer.level = level;

  time_t raw_time;
  struct tm tm_info;
  time(&raw_time);
  UTILS_LOCALTIME_R(&raw_time, &tm_info);

  /**
   * @ref https://linux.die.net/man/3/snprintf
   */
  int written =
    snprintf(buffer.data, sirius_log_buf_size,
             "%s%s [%02d:%02d:%02d %s %" PRIu64 " %s (%s|%d)]%s ", color,
             level_str, tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, module,
             _sirius_thread_id(), file, func, line, log_color_none);

  if (written < 0) {
    ISSUE_ERROR_STANDARD_FUNCTION("snprintf");
    return;
  }

  len += written;
  if (len >= sirius_log_buf_size) [[unlikely]] {
    ISSUE_ERROR_LOG_LOST();
    return;
  }

  /**
   * @ref https://linux.die.net/man/3/vsnprintf
   */
  va_list args;
  va_start(args, fmt);
  written = vsnprintf(buffer.data + len, sirius_log_buf_size - len, fmt, args);
  va_end(args);

  if (written < 0) {
    ISSUE_ERROR_STANDARD_FUNCTION("vsnprintf");
    return;
  }

  len += written;
  if (len >= sirius_log_buf_size) [[unlikely]] {
    len = sirius_log_buf_size - 1;
    ISSUE_WARNING_LOG_TRUNCATED();
  }

  buffer.data_size = len;

  g_log_manager->native_produce(&buffer,
                                sizeof(LogBuffer) - sirius_log_buf_size + len);
}

extern "C" void sirius_logsp_impl(int level, const char *level_str,
                                  const char *color, const char *module,
                                  const char *fmt, ...) {
  LogBuffer buffer {};
  int len = 0;

  buffer.level = level;

  time_t raw_time;
  struct tm tm_info;
  time(&raw_time);
  UTILS_LOCALTIME_R(&raw_time, &tm_info);

  int written =
    snprintf(buffer.data, sirius_log_buf_size, "%s%s [%02d:%02d:%02d %s]%s ",
             color, level_str, tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec,
             module, log_color_none);

  if (written < 0) {
    ISSUE_ERROR_STANDARD_FUNCTION("snprintf");
    return;
  }

  len += written;
  if (len >= sirius_log_buf_size) [[unlikely]] {
    ISSUE_ERROR_LOG_LOST();
    return;
  }

  va_list args;
  va_start(args, fmt);
  written = vsnprintf(buffer.data + len, sirius_log_buf_size - len, fmt, args);
  va_end(args);

  if (written < 0) {
    ISSUE_ERROR_STANDARD_FUNCTION("vsnprintf");
    return;
  }

  len += written;
  if (len >= sirius_log_buf_size) [[unlikely]] {
    len = sirius_log_buf_size - 1;
    ISSUE_WARNING_LOG_TRUNCATED();
  }

  buffer.data_size = len;

  g_log_manager->native_produce(&buffer,
                                sizeof(LogBuffer) - sirius_log_buf_size + len);
}
