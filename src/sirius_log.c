#include "internal/initializer.h"
#include "internal/log.h"
#include "sirius/sirius_thread.h"

#ifdef LOW_WRITE
#  undef LOW_WRITE
#endif
#ifdef LOCALTIME
#  undef LOCALTIME
#endif
#ifdef DPRINTF
#  undef DPRINTF
#endif
#ifdef LOG_PRINT
#  undef LOG_PRINT
#endif
#ifdef LOCK
#  undef LOCK
#endif
#ifdef UNLOCK
#  undef UNLOCK
#endif

#if defined(_WIN32) || defined(_WIN64)
#  define LOW_WRITE(fd, buf, len) _write(fd, buf, (unsigned int)(len))
#  define LOCALTIME(timer, result) localtime_s(result, timer)
#  define DPRINTF(fd, ...) \
    do { \
      char msg[1024] = {0}; \
      snprintf(msg, sizeof(msg), ##__VA_ARGS__); \
      _write(fd, msg, (unsigned int)strlen(msg)); \
    } while (0)
#else
#  define LOW_WRITE(fd, buf, len) write(fd, buf, len)
#  define LOCALTIME(timer, result) localtime_r(timer, result)
#  define DPRINTF dprintf
#endif

static int g_fd_err = STDERR_FILENO;
static int g_fd_out = STDOUT_FILENO;

#define LOG_PRINT(fmt, ...) \
  DPRINTF(g_fd_err, "[%s %s %d] " fmt, sirius_log_module_name, \
          sirius_file_name, __LINE__, ##__VA_ARGS__)

#if defined(_WIN32) || defined(_WIN64)
static SRWLOCK g_srw_lock = SRWLOCK_INIT;
#  define LOCK() AcquireSRWLockExclusive(&g_srw_lock)
#  define UNLOCK() ReleaseSRWLockExclusive(&g_srw_lock)

static void enable_win_ansi() {
#  if defined(NTDDI_VERSION) && (NTDDI_VERSION >= 0x0A000002)
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
  DWORD mode = 0;

  if (GetConsoleMode(hOut, &mode)) {
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, mode);
  }
  if (GetConsoleMode(hErr, &mode)) {
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hErr, mode);
  }
#  endif
}

#else

static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
#  define LOCK() pthread_mutex_lock(&g_mutex)
#  define UNLOCK() pthread_mutex_unlock(&g_mutex)

#endif

bool internal_init_log() {
#if defined(_WIN32) || defined(_WIN64)
  enable_win_ansi();
#endif
  return true;
}

void internal_deinit_log() {
#if !defined(_WIN32) && !defined(_WIN64)
  pthread_mutex_destroy(&g_mutex);
#endif
}

sirius_api void sirius_log_config(sirius_log_config_t cfg) {
  LOCK();
  if (cfg.fd_err && *cfg.fd_err >= 0) {
    g_fd_err = *cfg.fd_err;
  }
  if (cfg.fd_out && *cfg.fd_out >= 0) {
    g_fd_out = *cfg.fd_out;
  }
  UNLOCK();
}

static inline void write_log(int level, const char *buf, int len) {
  LOCK();
  int fd = (level <= sirius_log_level_warn) ? g_fd_err : g_fd_out;
  LOW_WRITE(fd, buf, len);
  UNLOCK();
}

sirius_api void sirius_log_impl(int level, const char *level_str,
                                const char *color, const char *module,
                                const char *file, const char *func, int line,
                                const char *fmt, ...) {
  char buf[LOG_BUF_SIZE];
  int len = 0;

  time_t raw_time;
  struct tm tm_info;
  time(&raw_time);
  LOCALTIME(&raw_time, &tm_info);

  int written = snprintf(
    buf, LOG_BUF_SIZE, "%s%s [%02d:%02d:%02d %s %" PRIu64 " %s (%s|%d)]%s ",
    color, level_str, tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, module,
    sirius_thread_id, file, func, line, log_color_none);

  if (likely(written > 0 && written < LOG_BUF_SIZE)) {
    len += written;
  } else {
    LOG_PRINT(
      "\n"
      "  The LOG module issues an ERROR\n"
      "  Log length exceeds the buffer, log will be lost\n");
    return;
  }

  va_list args;
  va_start(args, fmt);
  written = vsnprintf(buf + len, LOG_BUF_SIZE - len, fmt, args);
  va_end(args);

  if (written > 0) {
    len += written;
    if (unlikely(len >= LOG_BUF_SIZE)) {
      len = LOG_BUF_SIZE - 1;
      LOG_PRINT(
        "\n"
        "  The LOG module issues a WARNING\n"
        "  Log length exceeds the buffer, log will be truncated\n");
    }
  }

  write_log(level, buf, len);
}

sirius_api void sirius_logsp_impl(int level, const char *level_str,
                                  const char *color, const char *module,
                                  const char *fmt, ...) {
  char buf[LOG_BUF_SIZE];
  int len = 0;

  time_t raw_time;
  struct tm tm_info;
  time(&raw_time);
  LOCALTIME(&raw_time, &tm_info);

  int written = snprintf(buf, LOG_BUF_SIZE, "%s%s [%02d:%02d:%02d %s]%s ",
                         color, level_str, tm_info.tm_hour, tm_info.tm_min,
                         tm_info.tm_sec, module, log_color_none);

  if (likely(written > 0 && written < LOG_BUF_SIZE)) {
    len += written;
  } else {
    LOG_PRINT(
      "\n"
      "  The LOG module issues an ERROR\n"
      "  Log length exceeds the buffer, log will be lost\n");
    return;
  }

  va_list args;
  va_start(args, fmt);
  written = vsnprintf(buf + len, LOG_BUF_SIZE - len, fmt, args);
  va_end(args);

  if (written > 0) {
    len += written;
    if (unlikely(len >= LOG_BUF_SIZE)) {
      len = LOG_BUF_SIZE - 1;
      LOG_PRINT(
        "\n"
        "  The LOG module issues a WARNING\n"
        "  Log length exceeds the buffer, log will be truncated\n");
    }
  }

  write_log(level, buf, len);
}
