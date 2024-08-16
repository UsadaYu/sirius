#include "internal/internal_log.h"

#include "internal/internal_sys.h"

static int g_fd_err = STDERR_FILENO;
static int g_fd_out = STDOUT_FILENO;
static time_t g_raw_time;
static struct tm g_tm_info;
static int g_size;
static char g_buffer[WRITE_SIZE];

#ifdef _WIN32
#define localtime_r(a, b) localtime_s(b, a)

#define dprintf(fd, ...)                        \
  do {                                          \
    char msg[1024] = {0};                       \
    snprintf(msg, sizeof(msg), ##__VA_ARGS__);  \
    _write(fd, msg, (unsigned int)strlen(msg)); \
  } while (0)
#endif

#define cst_error(fmt, ...)                       \
  dprintf(g_fd_err, "[error %s %s %d] " fmt,      \
          log_module_name, sirius_file, __LINE__, \
          ##__VA_ARGS__)

#ifdef _WIN32
static CRITICAL_SECTION cs;

#define MUTEX_LOCK() EnterCriticalSection(&cs)
#define MUTEX_UNLOCK() LeaveCriticalSection(&cs)

static void _enable_win_ansi() {
#if defined(NTDDI_VERSION) && (NTDDI_VERSION >= 0x0A000002)
  HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  GetConsoleMode(h, &mode);
  mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  SetConsoleMode(h, mode);
#endif
}

bool _internal_log_init() {
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  int cnt = info.dwNumberOfProcessors > 1
                ? info.dwNumberOfProcessors * 100
                : 0;

  if (!InitializeCriticalSectionAndSpinCount(&cs, cnt)) {
    cst_error(
        "InitializeCriticalSectionAndSpinCount: %lu\n",
        GetLastError());
    return false;
  }

  _enable_win_ansi();

  return true;
}

void _internal_log_deinit() { DeleteCriticalSection(&cs); }

#else
static pthread_mutex_t mutex;

#define MUTEX_LOCK() pthread_mutex_lock(&mutex)
#define MUTEX_UNLOCK() pthread_mutex_unlock(&mutex)

bool _internal_log_init() {
  int ret = pthread_mutex_init(&mutex, NULL);
  if (ret) {
    cst_error("pthread_mutex_init: %d\n", ret);
    return false;
  }

  return true;
}
void _internal_log_deinit() {
  int ret = pthread_mutex_destroy(&mutex);
  if (ret) {
    cst_error("pthread_mutex_destroy: %d\n", ret);
  }
}

#endif

/**
 * the validity of the file descriptor is checked by the
 * upper level caller
 */
void internal_log_fd_set(const int *const fd_err,
                         const int *const fd_out) {
  MUTEX_LOCK();

  g_fd_err = fd_err ? *fd_err : g_fd_err;
  g_fd_out = fd_out ? *fd_out : g_fd_out;

  MUTEX_UNLOCK();
}

void internal_log(int level, const char *color,
                  const char *module, const char *file,
                  int line, const char *fmt, ...) {
  internal_init();

  static va_list args;

#ifdef _WIN32
#define W(fd, buf, size) \
  _write((fd), (buf), (unsigned int)(size))
#else
#define W(fd, buf, size) write((fd), (buf), (size))
#endif

#define LOG_WRITE(fd, type)                              \
  color = fd > STDERR_FILENO ? "" : color;               \
  time(&g_raw_time);                                     \
  localtime_r(&g_raw_time, &g_tm_info);                  \
  g_size = snprintf(                                     \
      g_buffer, sizeof(g_buffer),                        \
      "%s[%02d:%02d:%02d " #type " %s %s %d] ", color,   \
      g_tm_info.tm_hour, g_tm_info.tm_min,               \
      g_tm_info.tm_sec, module, file, line);             \
  va_start(args, fmt);                                   \
  g_size +=                                              \
      vsnprintf(g_buffer + g_size,                       \
                sizeof(g_buffer) - g_size, fmt, args);   \
  va_end(args);                                          \
  g_size =                                               \
      fd > STDERR_FILENO                                 \
          ? g_size                                       \
          : g_size + snprintf(g_buffer + g_size,         \
                              sizeof(g_buffer) - g_size, \
                              log_color_none);           \
  W(fd, g_buffer, g_size);

  MUTEX_LOCK();

  g_size = 0;

  switch (level) {
    case sirius_log_level_error:
      LOG_WRITE(g_fd_err, error) break;
    case sirius_log_level_warn:
      LOG_WRITE(g_fd_err, warn) break;
    case sirius_log_level_info:
      LOG_WRITE(g_fd_out, info) break;
    case sirius_log_level_debg:
      LOG_WRITE(g_fd_out, debg) break;
    default:  // sirius_log_level_none
      break;
  }

  MUTEX_UNLOCK();

#undef LOG_WRITE
#undef W
}
