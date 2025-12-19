#include <fcntl.h>
#include <sirius/sirius_thread.h>
#include <sys/stat.h>

#include "internal/utils.h"

#if defined(_WIN32) || defined(_WIN64)
#  include <share.h>
#endif

#define NB_THREADS (4)
#ifndef GEN_FILE_NAME
#  define GEN_FILE_NAME "./_gen_log.txt"
#endif

static bool g_exit_flag = false;

static void *thread_func(void *args) {
  (void)args;

  while (!g_exit_flag) {
    sirius_error("--------------------------------\n");
    sirius_debg("[TID: %llu]\n", sirius_thread_id);
    sirius_info("[TID: %llu]\n", sirius_thread_id);
    sirius_warn("[TID: %llu]\n", sirius_thread_id);

    sirius_debgsp("[TID: %llu]\n", sirius_thread_id);
    sirius_infosp("[TID: %llu]\n", sirius_thread_id);
    sirius_warnsp("[TID: %llu]\n", sirius_thread_id);
    sirius_error("--------------------------------\n\n");

    sirius_usleep(150 * 1000);
  }

  return nullptr;
}

int main() {
  int fd;
  int std_fd;
  sirius_log_config_t cfg = {0};

  utils_init();

#if defined(_WIN32) || defined(_WIN64)
  errno_t err = _sopen_s(&fd, GEN_FILE_NAME, _O_RDWR | _O_CREAT | _O_TRUNC,
                         _SH_DENYNO, _S_IREAD | _S_IWRITE);
  utils_assert(err == 0);
#else
  fd = open(GEN_FILE_NAME, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  utils_assert(-1 != fd);
#endif

  cfg.fd_err = &fd;
  cfg.fd_out = &fd;
  sirius_log_config(cfg);

  sirius_thread_t threads[NB_THREADS];
  for (int i = 0; i < NB_THREADS; i++) {
    utils_assert(
      !sirius_thread_create(threads + i, nullptr, thread_func, nullptr));
  }

  sirius_usleep(500 * 1000);
  std_fd = 1;
  cfg.fd_out = &std_fd;
  sirius_log_config(cfg);

  sirius_usleep(500 * 1000);
  std_fd = 2;
  cfg.fd_err = &std_fd;
  sirius_log_config(cfg);

  sirius_usleep(500 * 1000);
  cfg.fd_err = &fd;
  cfg.fd_out = &fd;
  sirius_log_config(cfg);

  sirius_usleep(500 * 1000);
  g_exit_flag = true;

  for (int i = 0; i < NB_THREADS; i++) {
    utils_assert(!sirius_thread_join(threads[i], nullptr));
  }

#if defined(_WIN32) || defined(_WIN64)
  _close(fd);
#else
  close(fd);
#endif

  utils_deinit();

  return 0;
}
