#include "utils/log/log.h"

#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#include "sirius/utils/thread.h"
#include "utils/initializer.h"
#include "utils/macro.h"

#if defined(_WIN32) || defined(_WIN64)

#else
#  include <sys/mman.h>
#endif

constexpr unsigned int SHARED_CAPACITY =
  next_power_of_2(sirius_log_shared_capacity);

#define SHARED_NAME sirius_shared_name_prefix "_utils_kit_log_shmkey"

#define SLOT_RESET_TIMEOUT_MILLIS (10000)

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

struct alignas(hardware_destructive_interference_size) SharedSlot {
  std::atomic<SlotState> state;
  std::atomic<uint64_t> timestamp;

  LogBuffer buffer;
};

struct SharedHeader {
  static constexpr uint32_t MAGIC = 0xDEADBEEF;

  std::atomic<uint32_t> magic;

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
  if (unlikely(size == 0)) {
    UTILS_DPRINTF(STDERR_FILENO, "%s : %lu\n", function, error_code);
  } else {
    UTILS_DPRINTF(STDERR_FILENO, "%s : %lu, %s", function, error_code, e);
  }
}

#endif

// --- Log Manager ---
class LogManager {
 public:
  std::atomic<bool> running = false;
  std::mutex mutex;
  int fd_stdout = STDOUT_FILENO;
  int fd_stderr = STDERR_FILENO;

#if defined(_WIN32) || defined(_WIN64)
  LogManager()
      : shared_handle_(nullptr),
        is_creator_(false),
        mask_(SHARED_CAPACITY - 1) {}
#else
  LogManager()
      : shared_fd_(-1), is_creator_(false), mask_(SHARED_CAPACITY - 1) {}
#endif

  ~LogManager() {}

  bool init() {
    if (!open_shm())
      return false;

    if (!memory_map()) {
      close_shm();
      return false;
    }

    if (!get_slots()) {
      memory_unmap();
      close_shm();
      return false;
    }

    if (is_creator_) {
      running.store(true, std::memory_order_relaxed);
      thread_consumer_ = std::thread([this]() {
        this->thread_consumer();
      });
      thread_monitor_ = std::thread([this]() {
        this->thread_monitor();
      });
    }

    return true;
  }

  void deinit() {
    if (is_creator_) {
      running.store(false, std::memory_order_relaxed);
      thread_monitor_.join();
      thread_consumer_.join();
    }

    memory_unmap();
    close_shm();
  }

  void produce(void *src, size_t size) {
    if (unlikely(!running.load(std::memory_order_relaxed))) {
      auto buffer = (LogBuffer *)src;
      log_write(buffer->level, (void *)buffer->data, buffer->data_size);
    }

    uint64_t cur_write_idx =
      header_->write_index.fetch_add(1, std::memory_order_acq_rel);
    uint32_t slot_idx = cur_write_idx & mask_;
    SharedSlot &slot = slots_[slot_idx];

    int retries = 0;
    while (slot.state.load(std::memory_order_acquire) != SlotState::FREE) {
      if (++retries > 1000) {
        std::this_thread::yield();
      }
      // if (retries > 200000)
      //   return;
    }

    slot.timestamp.store(get_current_time_millis(), std::memory_order_relaxed);
    slot.state.store(SlotState::WRITING, std::memory_order_release);

    std::memcpy(&slot.buffer, src, size);

    slot.state.store(SlotState::READY, std::memory_order_release);
  }

  void log_write(int level, const void *buffer, size_t size) {
    std::lock_guard<std::mutex> lock(mutex);

    int fd = level <= sirius_log_level_warn ? fd_stderr : fd_stdout;

#if defined(_WIN32) || defined(_WIN64)
    _write(fd, buffer, (unsigned int)(size));
#else
    write(fd, buffer, size);
#endif
  }

 private:
#if defined(_WIN32) || defined(_WIN64)
  HANDLE shared_handle_;
#else
  int shared_fd_;
#endif

  SharedHeader *header_ = nullptr;
  SharedSlot *slots_ = nullptr;

  bool is_creator_;
  uint32_t mask_;
  size_t mapped_size_ = 0;

  std::thread thread_consumer_;
  std::thread thread_monitor_;

#if defined(_WIN32) || defined(_WIN64)

  bool open_shm() {
    size_t size_needed = calculate_size();

    DWORD size_high = (DWORD)((uint64_t)size_needed >> 32);
    DWORD size_low = (DWORD)((uint64_t)size_needed & 0xFFFFFFFF);

    shared_handle_ =
      CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                         size_high, size_low, SHARED_NAME);

    if (shared_handle_ == nullptr) {
      win_format_error(GetLastError(), "CreateFileMappingA");
      return false;
    }

    is_creator_ = GetLastError() == ERROR_ALREADY_EXISTS ? false : true;

    mapped_size_ = size_needed;

    return true;
  }

  void close_shm() {
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
    header_ = static_cast<SharedHeader *>(ptr);

    return true;
  }

  void memory_unmap() {
    if (header_) {
      UnmapViewOfFile(header_);
      header_ = nullptr;
    }
  }

#else

  bool open_shm() {
    size_t size_needed = calculate_size();

    shared_fd_ = shm_open(SHARED_NAME, O_CREAT | O_EXCL | O_RDWR, 0644);

    if (shared_fd_ >= 0) {
      is_creator_ = true;
      if (ftruncate(shared_fd_, size_needed) == -1) {
        perror("ftruncate");
        shm_unlink(SHARED_NAME);
        close(shared_fd_);
        return false;
      }
    } else if (errno == EEXIST) {
      is_creator_ = false;
      shared_fd_ = shm_open(SHARED_NAME, O_RDWR, 0);
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

  void close_shm() {
    if (shared_fd_ != -1) {
      close(shared_fd_);
      shared_fd_ = -1;
    }

    if (is_creator_) {
      UTILS_DPRINTF(STDOUT_FILENO, "Unlink the share memory\n");
      shm_unlink(SHARED_NAME);
    }
  }

  bool memory_map() {
    void *ptr = mmap(nullptr, mapped_size_, PROT_READ | PROT_WRITE, MAP_SHARED,
                     shared_fd_, 0);
    if (ptr == MAP_FAILED) {
      perror("mmap");
      return false;
    }
    header_ = static_cast<SharedHeader *>(ptr);

    return true;
  }

  void memory_unmap() {
    if (header_) {
      munmap(header_, mapped_size_);
      header_ = nullptr;
      slots_ = nullptr;
    }
  }

#endif

  void thread_consumer() {
    while (running.load(std::memory_order_relaxed) ||
           header_->read_index.load(std::memory_order_acquire) <
             header_->write_index.load(std::memory_order_acquire)) {
      uint64_t cur_read_idx =
        header_->read_index.load(std::memory_order_acquire);

      if (cur_read_idx >=
          header_->write_index.load(std::memory_order_acquire)) {
        if (!running.load(std::memory_order_relaxed))
          break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }

      uint32_t slot_idx = cur_read_idx & mask_;
      SharedSlot &slot = slots_[slot_idx];

      int retries = 0;
      while (slot.state.load(std::memory_order_acquire) != SlotState::READY) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (++retries > 100) {
          UTILS_DPRINTF(fd_stderr, "Skip corrupted slot index: %" PRIu64 "\n",
                        cur_read_idx);
          goto label_skip;
        }
      }

      {
        LogBuffer &buffer = slot.buffer;
        log_write(buffer.level, (void *)buffer.data, buffer.data_size);
        slot.state.store(SlotState::FREE, std::memory_order_release);
      }

    label_skip:
      header_->read_index.fetch_add(1, std::memory_order_release);
    }
  }

  void thread_monitor() {
    while (running.load(std::memory_order_relaxed)) {
      std::this_thread::sleep_for(std::chrono::seconds(1));

      uint64_t now = get_current_time_millis();

      for (uint32_t i = 0; i < header_->capacity; ++i) {
        SlotState s = slots_[i].state.load(std::memory_order_acquire);

        if (s == SlotState::WRITING) {
          uint64_t ts = slots_[i].timestamp.load(std::memory_order_relaxed);
          if (now - ts > SLOT_RESET_TIMEOUT_MILLIS) {
            {
              std::lock_guard<std::mutex> lock(mutex);
              UTILS_DPRINTF(fd_stdout, "Recovering stuck slot: %u\n", i);
            }

            /**
             * @note Construct a forged log indicating data loss.
             */
            const char *err_msg =
              "[System Error] Slot recovered/skipped due to timeout\n";
            std::memcpy(slots_[i].buffer.data, err_msg, strlen(err_msg));
            slots_[i].buffer.data_size = std::strlen(err_msg);
            slots_[i].buffer.level = sirius_log_level_error;

            /**
             * @note Let it be `READY`, so that consumers can read it, thereby
             * advancing the index.
             */
            slots_[i].state.store(SlotState::READY, std::memory_order_release);
          }
        }
      }
    }
  }

  size_t calculate_size() {
    size_t header_aligned = (sizeof(SharedHeader) + 63) & ~63;
    return header_aligned + (SHARED_CAPACITY * sizeof(SharedSlot));
  }

  bool get_slots() {
    uint8_t *base_ptr = reinterpret_cast<uint8_t *>(header_);
    size_t header_size = sizeof(SharedHeader);

    size_t align_offset =
      (header_size + hardware_destructive_interference_size - 1) &
      ~(hardware_destructive_interference_size - 1);

    slots_ = reinterpret_cast<SharedSlot *>(base_ptr + align_offset);

    if (is_creator_) {
      header_->write_index.store(0);
      header_->read_index.store(0);
      header_->capacity = SHARED_CAPACITY;
      header_->item_size = sizeof(SharedSlot);

      header_->magic.store(SharedHeader::MAGIC, std::memory_order_release);
    } else {
      if (!wait_for_initialization())
        return false;
    }

    return true;
  }

  bool wait_for_initialization() {
    int retries = 0;
    while (header_->magic.load(std::memory_order_acquire) !=
           SharedHeader::MAGIC) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      if (++retries > 100) {
        UTILS_DPRINTF(STDERR_FILENO, "Timeout waiting for SHM initialization");
        return false;
      }
    }

    return true;
  }

  uint64_t get_current_time_millis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
  }
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

#if defined(_WIN32) || defined(_WIN64)
#  define LOCALTIME(timer, result) localtime_s(result, timer)
#else
#  define LOCALTIME(timer, result) localtime_r(timer, result)
#endif

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
  LOCALTIME(&raw_time, &tm_info);

  int written =
    snprintf(buffer.data, sirius_log_buf_size,
             "%s%s [%02d:%02d:%02d %s %" PRIu64 " %s (%s|%d)]%s ", color,
             level_str, tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, module,
             _sirius_thread_id(), file, func, line, log_color_none);

  if (likely(written > 0 && written < sirius_log_buf_size)) {
    len += written;
  } else {
    const char *e =
      "\n"
      "  The LOG module issues an ERROR\n"
      "  Log length exceeds the buffer, log will be lost\n";
    g_log_manager->log_write(sirius_log_level_error, e, strlen(e));
    return;
  }

  va_list args;
  va_start(args, fmt);
  written = vsnprintf(buffer.data + len, sirius_log_buf_size - len, fmt, args);
  va_end(args);

  if (written > 0) {
    len += written;
    if (unlikely(len >= sirius_log_buf_size)) {
      len = sirius_log_buf_size - 1;
      const char *e =
        "\n"
        "  The LOG module issues a WARNING\n"
        "  Log length exceeds the buffer, log will be truncated\n";
      g_log_manager->log_write(sirius_log_level_warn, e, strlen(e));
    }
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
  LOCALTIME(&raw_time, &tm_info);

  int written =
    snprintf(buffer.data, sirius_log_buf_size, "%s%s [%02d:%02d:%02d %s]%s ",
             color, level_str, tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec,
             module, log_color_none);

  if (likely(written > 0 && written < sirius_log_buf_size)) {
    len += written;
  } else {
    const char *e =
      "\n"
      "  The LOG module issues an ERROR\n"
      "  Log length exceeds the buffer, log will be lost\n";
    g_log_manager->log_write(sirius_log_level_error, e, strlen(e));
    return;
  }

  va_list args;
  va_start(args, fmt);
  written = vsnprintf(buffer.data + len, sirius_log_buf_size - len, fmt, args);
  va_end(args);

  if (written > 0) {
    len += written;
    if (unlikely(len >= sirius_log_buf_size)) {
      len = sirius_log_buf_size - 1;
      const char *e =
        "\n"
        "  The LOG module issues a WARNING\n"
        "  Log length exceeds the buffer, log will be truncated\n";
      g_log_manager->log_write(sirius_log_level_warn, e, strlen(e));
    }
  }

  buffer.data_size = len;

  g_log_manager->produce(&buffer,
                         sizeof(LogBuffer) - sirius_log_buf_size + len);
}
