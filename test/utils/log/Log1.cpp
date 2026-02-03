#include <fcntl.h>
#include <sirius/utils/thread.h>
#include <sys/stat.h>

#include <thread>

#include "internal/utils.h"

#if defined(_WIN32) || defined(_WIN64)
#  include <share.h>
#endif

#define NB_THREADS (4)
#ifndef GEN_FILE_NAME
#  define GEN_FILE_NAME "./_gen_log.txt"
#endif

static bool g_exit_flag = false;

static void thread_func() {
  while (!g_exit_flag) {
    sirius_error("--------------------------------\n");
    sirius_debug("[TID: %llu]\n", _sirius_thread_id());
    sirius_info("[TID: %llu]\n", _sirius_thread_id());
    sirius_warn("[TID: %llu]\n", _sirius_thread_id());

    sirius_debugsp("[TID: %llu]\n", _sirius_thread_id());
    sirius_infosp("[TID: %llu]\n", _sirius_thread_id());
    sirius_warnsp("[TID: %llu]\n", _sirius_thread_id());
    sirius_error("--------------------------------\n\n");

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
  }
}

int main() {
  utils_init();

  int fd = 1;
  int std_fd;
  sirius_log_config_t cfg = {};

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
  sirius_log_configure(cfg);

  std::thread threads[NB_THREADS];
  for (int i = 0; i < NB_THREADS; ++i) {
    threads[i] = std::thread(thread_func);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  std_fd = 1;
  cfg.fd_out = &std_fd;
  sirius_log_configure(cfg);

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  std_fd = 2;
  cfg.fd_err = &std_fd;
  sirius_log_configure(cfg);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  cfg.fd_err = &fd;
  cfg.fd_out = &fd;
  sirius_log_configure(cfg);

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  g_exit_flag = true;

  for (int i = 0; i < NB_THREADS; i++) {
    threads[i].join();
  }

#if defined(_WIN32) || defined(_WIN64)
  _close(fd);
#else
  close(fd);
#endif

  /**
   * @note
   * Ultra-long log printing test.
   */
  std_fd = 2;
  cfg.fd_err = &std_fd;
  cfg.fd_out = &std_fd;
  sirius_log_configure(cfg);
  sirius_warnsp("--------------------------------\n");
  sirius_warnsp("- Ultra-long log printing test\n");
  char ultra_long_string[40960] = {0};
  for (size_t i = 0; i < sizeof(ultra_long_string) - 1; ++i) {
    ultra_long_string[i] = 'Q';
  }
  sirius_warnsp("- The length of the string to be printed: %zu\n",
                strlen(ultra_long_string));
  sirius_logsp_impl(sirius_log_level_none, "Ultra", log_purple,
                    sirius_log_module_name, "- %s\n", ultra_long_string);
  sirius_warnsp("--------------------------------\n");

  utils_deinit();

  return 0;
}
