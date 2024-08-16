#include "internal/internal_log.h"
#include "internal/internal_sys.h"
#include "sirius_mutex.h"
#include "sirius_thread.h"

static sirius_mutex_handle g_mutex;
static int g_fd_err = STDERR_FILENO;
static int g_fd_out = STDOUT_FILENO;
static time_t g_raw_time;
static struct tm g_tm_info;
static int g_size;
static char g_buf[WRITE_SIZE];

bool _log_init() {
  return (bool)(!sirius_mutex_init(&g_mutex, NULL));
}

void _log_deinit() {
  (void)sirius_mutex_destroy(&g_mutex);
}

void sirius_log_config(sirius_log_config_t cfg) {
  sirius_mutex_lock(&g_mutex);

  if (cfg.fd_err) {
    if (*cfg.fd_err < STDOUT_FILENO) {
      internal_warn("Invalid argument: [fd_err: %lld]\n",
                    *cfg.fd_err);
    } else {
      g_fd_err = *cfg.fd_err;
    }
  }

  if (cfg.fd_out) {
    if (*cfg.fd_out < STDOUT_FILENO) {
      internal_warn("Invalid argument: [fd_out: %lld]\n",
                    *cfg.fd_out);
    } else {
      g_fd_out = *cfg.fd_out;
    }
  }

  sirius_mutex_unlock(&g_mutex);

  internal_log_fd_set(&g_fd_err, &g_fd_out);
}

#ifdef _WIN32
#define localtime_r(a, b) localtime_s(b, a)
#define WRITE(fd, buf, size) \
  _write((fd), (buf), (unsigned int)(size))
#else
#define WRITE(fd, buf, size) write((fd), (buf), (size))
#endif

#define P(fd, type)                                       \
  sirius_mutex_lock(&g_mutex);                            \
  g_size = 0;                                             \
  color = fd > STDERR_FILENO ? "" : color;                \
  time(&g_raw_time);                                      \
  localtime_r(&g_raw_time, &g_tm_info);                   \
  SP(type)                                                \
  va_start(args, fmt);                                    \
  g_size += vsnprintf(g_buf + g_size,                     \
                      sizeof(g_buf) - g_size, fmt, args); \
  va_end(args);                                           \
  g_size =                                                \
      fd > STDERR_FILENO                                  \
          ? g_size                                        \
          : g_size + snprintf(g_buf + g_size,             \
                              sizeof(g_buf) - g_size,     \
                              log_color_none);            \
  WRITE(fd, g_buf, g_size);                               \
  sirius_mutex_unlock(&g_mutex);                          \
  break;

#define LOG_TPL                                  \
  va_list args;                                  \
  switch (log_level) {                           \
    case sirius_log_level_none:                  \
      break;                                     \
    case sirius_log_level_error:                 \
      P(g_fd_err, error)                         \
    case sirius_log_level_warn:                  \
      P(g_fd_err, warn)                          \
    case sirius_log_level_info:                  \
      P(g_fd_out, info)                          \
    case sirius_log_level_debg:                  \
      P(g_fd_out, debg)                          \
    default:                                     \
      internal_warn(                             \
          "Invalid argument: [log level: %d]\n", \
          log_level);                            \
      break;                                     \
  }

void sirius_logsp(int log_level, const char *color,
                  const char *module, const char *fmt,
                  ...) {
#define SP(type)                                          \
  g_size =                                                \
      snprintf(g_buf, sizeof(g_buf),                      \
               "%s[%02d:%02d:%02d " #type " %s] ", color, \
               g_tm_info.tm_hour, g_tm_info.tm_min,       \
               g_tm_info.tm_sec, module);

  LOG_TPL
#undef SP
}

void sirius_log(int log_level, const char *color,
                const char *module, const char *file,
                const char *func, int line,
                const char *fmt, ...) {
#define SP(type)                                          \
  g_size = snprintf(                                      \
      g_buf, sizeof(g_buf),                               \
      "%s[%02d:%02d:%02d " #type " %s %llu %s (%s|%d)] ", \
      color, g_tm_info.tm_hour, g_tm_info.tm_min,         \
      g_tm_info.tm_sec, module, sirius_thread_id, file,   \
      func, line);

  LOG_TPL
#undef SP
}
