#include "utils/log/log.h"

#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "sirius/utils/thread.h"
#include "utils/initializer.h"
#include "utils/namespace.hpp"

#if defined(_WIN32) || defined(_WIN64)

#else
#  include <sys/mman.h>
#endif

static constexpr unsigned int SHARED_CAPACITY =
  (unsigned int)Utils::Utils::next_power_of_2(sirius_log_shared_capacity);

#define SLOT_RESET_TIMEOUT_MS (10000)

#define UTILS_LOG "utils_log"

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
struct alignas(hardware_destructive_interference_size) SharedSlot {
  std::atomic<SlotState> state;
  std::atomic<uint64_t> timestamp;

  LogBuffer buffer;
};
#ifdef _MSC_VER
#  pragma warning(pop)
#endif

struct SharedHeader {
  static constexpr uint32_t MAGIC = 0xDEADBEEF;

  /**
   * @note This variable should be guarded by the process lock.
   */
  uint32_t magic;

  /**
   * @note This variable should be guarded by the process lock.
   */
  uint64_t attached_count;

  // std::thread thread_consumer;
  // std::thread thread_monitor;

  std::atomic<uint64_t> write_index;
  std::atomic<uint64_t> read_index;
  uint32_t capacity;
  uint32_t item_size;

  uint64_t reserved[8];
};

#if defined(_WIN32) || defined(_WIN64)
static inline void win_format_error(DWORD error_code, const char *function) {
  char e[sirius_log_buf_size] = {0};
  DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD size = FormatMessage(flags, nullptr, error_code,
                             MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), e,
                             sizeof(e) / sizeof(TCHAR), nullptr);
  if (size == 0) [[unlikely]] {
    UTILS_DPRINTF(STDERR_FILENO, "%s : %lu\n", function, error_code);
  } else {
    UTILS_DPRINTF(STDERR_FILENO, "%s : %lu, %s", function, error_code, e);
  }
}

static HANDLE global_lock_;

static inline bool process_lock_create() {
  global_lock_ =
    CreateMutexA(nullptr, FALSE, Utils::Ns::win_lock_name(UTILS_LOG).c_str());
  if (!global_lock_) {
    win_format_error(GetLastError(), "CreateMutexA");
    return false;
  }

  return true;
}

static inline void process_lock_destory() {
  CloseHandle(global_lock_);
}

static inline bool process_lock_lock() {
  while (true) {
    DWORD dw_err = WaitForSingleObject(global_lock_, INFINITE);
    switch (dw_err) {
    [[likely]] case WAIT_OBJECT_0:
      return true;
    case WAIT_ABANDONED:
      UTILS_DPRINTF(
        STDOUT_FILENO,
        "A process that previously held the lock may have crashed\n");
      continue;
    case WAIT_FAILED:
      win_format_error(dw_err, "WaitForSingleObject");
      return false;
    default:
      return false;
    }
  }
}

static inline bool process_lock_unlock() {
  if (!ReleaseMutex(global_lock_)) {
    win_format_error(GetLastError(), "WaitForSingleObject");
    return false;
  }

  return true;
}

#else
static int global_lock_fd_;

static inline bool process_lock_create() {
  auto path = Utils::Ns::posix_lockfile_path(UTILS_LOG);
  global_lock_fd_ = open(path.c_str(), O_RDWR | O_CREAT,
                         Utils::File::string_to_mode(sirius_posix_file_mode));
  if (global_lock_fd_ == -1) {
    perror("open");
    return false;
  }

  return true;
}

static inline void process_lock_destory() {
  close(global_lock_fd_);
}

static inline bool process_lock_lock() {
  struct flock lock;
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;

  if (fcntl(global_lock_fd_, F_SETLKW, &lock) == -1) {
    perror("fcntl lock");
    return false;
  }

  return true;
}

static inline bool process_lock_unlock() {
  struct flock lock;
  lock.l_type = F_UNLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;

  if (fcntl(global_lock_fd_, F_SETLK, &lock) == -1) {
    perror("fcntl unlock");
    return false;
  }

  return true;
}
#endif

class LogShared {
 public:
  bool shared_creator = false;
  SharedHeader *header = nullptr;
  SharedSlot *slots = nullptr;

#if defined(_WIN32) || defined(_WIN64)
  LogShared()
      : shared_name_(Utils::Ns::get_shm_name(UTILS_LOG)),
        shared_handle_(nullptr) {}
#else
  LogShared()
      : shared_name_(Utils::Ns::get_shm_name(UTILS_LOG)), shared_fd_(-1) {}
#endif

  ~LogShared() {}

#if defined(_WIN32) || defined(_WIN64)
  bool open_shm() {
    size_t size_needed = calculate_size();

    DWORD size_high = (DWORD)((uint64_t)size_needed >> 32);
    DWORD size_low = (DWORD)((uint64_t)size_needed & 0xFFFFFFFF);

    shared_handle_ =
      CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                         size_high, size_low, shared_name_.c_str());

    if (shared_handle_ == nullptr) {
      win_format_error(GetLastError(), "CreateFileMappingA");
      return false;
    }

    shared_creator = GetLastError() == ERROR_ALREADY_EXISTS ? false : true;

    mapped_size_ = size_needed;

    return true;
  }

  void close_shm(bool should_unlink) {
    (void)should_unlink;

    if (shared_handle_ != nullptr) {
      CloseHandle(shared_handle_);
      shared_handle_ = nullptr;
    }
  }

  bool memory_map() {
    void *ptr =
      MapViewOfFile(shared_handle_, FILE_MAP_ALL_ACCESS, 0, 0, mapped_size_);
    if (ptr == nullptr) {
      win_format_error(GetLastError(), "MapViewOfFile");
      return false;
    }
    header = static_cast<SharedHeader *>(ptr);

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

    shared_fd_ = shm_open(shared_name_.c_str(), O_CREAT | O_EXCL | O_RDWR,
                          Utils::File::string_to_mode(sirius_posix_file_mode));

    if (shared_fd_ >= 0) {
      shared_creator = true;
      if (ftruncate(shared_fd_, size_needed) == -1) {
        perror("ftruncate");
        shm_unlink(shared_name_.c_str());
        close(shared_fd_);
        return false;
      }
    } else if (errno == EEXIST) {
      shared_creator = false;
      shared_fd_ = shm_open(shared_name_.c_str(), O_RDWR, 0);
      if (shared_fd_ < 0) {
        perror("shm_open attach");
        return false;
      }
    } else {
      perror("shm_open error");
      return false;
    }

    mapped_size_ = size_needed;

    return true;
  }

  void close_shm(bool should_unlink) {
    if (shared_fd_ != -1) {
      close(shared_fd_);
      shared_fd_ = -1;
    }

    if (should_unlink) {
      UTILS_DPRINTF(STDOUT_FILENO, "Unlink the share memory\n");
      shm_unlink(shared_name_.c_str());
    }
  }

  bool memory_map() {
    void *ptr = mmap(nullptr, mapped_size_, PROT_READ | PROT_WRITE, MAP_SHARED,
                     shared_fd_, 0);
    if (ptr == MAP_FAILED) {
      perror("mmap");
      return false;
    }
    header = static_cast<SharedHeader *>(ptr);

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

  bool get_slots() {
    uint8_t *base_ptr = reinterpret_cast<uint8_t *>(header);
    size_t header_size = sizeof(SharedHeader);

    size_t align_offset =
      (header_size + hardware_destructive_interference_size - 1) &
      ~(hardware_destructive_interference_size - 1);

    slots = reinterpret_cast<SharedSlot *>(base_ptr + align_offset);

    if (shared_creator) {
      header->attached_count = 0;
      header->write_index.store(0);
      header->read_index.store(0);
      header->capacity = SHARED_CAPACITY;
      header->item_size = sizeof(SharedSlot);

      header->magic = SharedHeader::MAGIC;
    } else {
      if (header->magic != SharedHeader::MAGIC) {
        UTILS_DPRINTF(STDERR_FILENO, "Invalid argument. `magic`: %u\n",
                      header->magic);
        return false;
      }
    }

    ++header->attached_count;

    return true;
  }

 private:
  std::string shared_name_;
#if defined(_WIN32) || defined(_WIN64)
  HANDLE shared_handle_;
#else
  int shared_fd_;
#endif
  size_t mapped_size_ = 0;

  size_t calculate_size() {
    size_t header_aligned = (sizeof(SharedHeader) + 63) & ~63;
    return header_aligned + (SHARED_CAPACITY * sizeof(SharedSlot));
  }
};

// ---------------------------------------------------
/**
 * @note
 * -----------------------
 * A daemon process needs to be used here.
 * -----------------------
 */
static std::thread g_thread_consumer;
static std::thread g_thread_monitor;
// ---------------------------------------------------
// --- Log Manager ---
class LogManager {
 public:
  std::atomic<bool> running = false;
  std::mutex mutex;
  int fd_stdout = STDOUT_FILENO;
  int fd_stderr = STDERR_FILENO;

  LogManager() : is_creator_(false), is_destoryer_(false) {
    log_shared_ = new LogShared();
  }

  ~LogManager() {
    if (log_shared_) {
      delete log_shared_;
      log_shared_ = nullptr;
    }
  }

  bool init() {
    if (!process_lock_create())
      return false;
    if (!process_lock_lock())
      goto label_free1;
    if (!log_shared_->open_shm())
      goto label_free2;
    is_creator_ = log_shared_->shared_creator;
    if (!log_shared_->memory_map())
      goto label_free3;
    if (!log_shared_->get_slots())
      goto label_free4;

    if (is_creator_) {
      running.store(true, std::memory_order_relaxed);
      g_thread_consumer = std::thread([this]() {
        this->thread_consumer();
      });
      g_thread_monitor = std::thread([this]() {
        this->thread_monitor();
      });
    }

    process_lock_unlock();

    return true;

  label_free4:
    log_shared_->memory_unmap();
  label_free3:
    log_shared_->close_shm(is_creator_);
  label_free2:
    process_lock_unlock();
  label_free1:
    process_lock_destory();

    return false;
  }

  void deinit() {
    process_lock_lock();

    is_destoryer_ = --log_shared_->header->attached_count <= 0;

    if (is_destoryer_) {
      running.store(false, std::memory_order_relaxed);

#ifdef _SIRIUS_WIN_DLL
      try_best_to_flush();
#else
      if (g_thread_monitor.joinable()) {
        g_thread_monitor.join();
      }
      if (g_thread_consumer.joinable()) {
        g_thread_consumer.join();
      }
#endif
    }

    log_shared_->memory_unmap();
    log_shared_->close_shm(is_destoryer_);

    process_lock_unlock();
    process_lock_destory();
  }

  void produce(void *src, size_t size) {
    if (!running.load(std::memory_order_relaxed)) [[unlikely]] {
      auto buffer = (LogBuffer *)src;
      log_write(buffer->level, (void *)buffer->data, buffer->data_size);
    }

    uint64_t cur_write_idx =
      log_shared_->header->write_index.fetch_add(1, std::memory_order_acq_rel);
    uint32_t slot_idx = cur_write_idx & (SHARED_CAPACITY - 1);
    SharedSlot &slot = log_shared_->slots[slot_idx];

    int retries = 0;
    while (slot.state.load(std::memory_order_acquire) != SlotState::FREE) {
      if (++retries > 1000) {
        std::this_thread::yield();
      }
    }

    slot.timestamp.store(Utils::Time::get_sys_clock_ms(),
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
  bool is_destoryer_;
  LogShared *log_shared_;

  void thread_consumer() {
    auto header = log_shared_->header;

    while (running.load(std::memory_order_relaxed) ||
           header->read_index.load(std::memory_order_acquire) <
             header->write_index.load(std::memory_order_acquire)) {
      uint64_t cur_read_idx =
        header->read_index.load(std::memory_order_acquire);

      if (cur_read_idx >= header->write_index.load(std::memory_order_acquire)) {
        if (!running.load(std::memory_order_relaxed))
          break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }

      auto opt_slot = get_slot(cur_read_idx, 100);
      if (opt_slot) {
        SharedSlot &slot = opt_slot->get();
        LogBuffer &buffer = slot.buffer;
        log_write(buffer.level, (void *)buffer.data, buffer.data_size);
        slot.state.store(SlotState::FREE, std::memory_order_release);
      }

      header->read_index.fetch_add(1, std::memory_order_release);
    }
  }

  void thread_monitor() {
    auto header = log_shared_->header;
    auto slots = log_shared_->slots;

    while (running.load(std::memory_order_relaxed)) {
      std::this_thread::sleep_for(std::chrono::seconds(1));

      uint64_t now = Utils::Time::get_sys_clock_ms();

      for (uint32_t i = 0; i < header->capacity; ++i) {
        SlotState s = slots[i].state.load(std::memory_order_acquire);

        if (s == SlotState::WRITING) {
          uint64_t ts = slots[i].timestamp.load(std::memory_order_relaxed);
          if (now - ts > SLOT_RESET_TIMEOUT_MS) {
            {
              std::lock_guard<std::mutex> lock(mutex);
              UTILS_DPRINTF(fd_stdout, "Recovering stuck slot: %u\n", i);
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

  [[nodiscard]] std::optional<std::reference_wrapper<SharedSlot>>
  get_slot(const uint64_t read_index, const int retry_times) {
    uint32_t slot_idx = read_index & (SHARED_CAPACITY - 1);
    SharedSlot &slot = log_shared_->slots[slot_idx];

    int retries = 0;
    while (slot.state.load(std::memory_order_acquire) != SlotState::READY) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      if (++retries > retry_times) {
        UTILS_DPRINTF(fd_stderr, "Skip corrupted slot index: %" PRIu64 "\n",
                      read_index);
        return std::nullopt;
      }
    }

    return slot;
  }

#ifdef _SIRIUS_WIN_DLL

  /**
   * @note On Windows, after the `main` function is completed, the `ExitProcess`
   * function is called. This function is destructive, especially for dynamic
   * libraries, for instance, it will directly destroy all threads except the
   * main thread.
   */
  void try_best_to_flush() {
    auto header = log_shared_->header;
    uint64_t read_index = header->read_index.load(std::memory_order_acquire);
    uint64_t write_index = header->write_index.load(std::memory_order_acquire);

    for (; read_index < write_index; ++read_index) {
      auto opt_slot = get_slot(read_index, 10);
      if (opt_slot) {
        SharedSlot &slot = opt_slot->get();
        LogBuffer &buffer = slot.buffer;
        int fd = buffer.level <= sirius_log_level_warn ? fd_stderr : fd_stdout;
        UTILS_WRITE(fd, (void *)buffer.data, buffer.data_size);
      }
    }
  }

#endif
};

// --- Global Instance & Hooks ---

LogManager *g_log_manager = nullptr;

#if defined(_WIN32) || defined(_WIN64)

static inline void enable_win_ansi() {
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
  g_log_manager = new LogManager();
  if (!g_log_manager->init()) {
    delete g_log_manager;
    g_log_manager = nullptr;
    return false;
  }

#if defined(_WIN32) || defined(_WIN64)
  enable_win_ansi();
#endif

  return true;
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

  g_log_manager->produce(&buffer,
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

  g_log_manager->produce(&buffer,
                         sizeof(LogBuffer) - sirius_log_buf_size + len);
}
