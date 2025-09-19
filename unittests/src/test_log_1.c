#include <fcntl.h>
#include <sys/stat.h>

#include "sirius/sirius_log.h"
#include "sirius/sirius_mutex.h"
#include "sirius/sirius_thread.h"
#include "test.h"

#ifdef _WIN32
#  include <share.h>
#endif

#define THREAD_CNT (16)

static const char *g_file_name = "./test_log_2.txt";
static bool g_exit_flag = false;

static void thread_func() {
  while (!g_exit_flag) {
    unsigned int idx = 1;
    sirius_debg("[thread: %llu] %u: HelloWorld\n", sirius_thread_id, idx++);
    sirius_debg("[thread: %llu] %u: %s\n", sirius_thread_id, idx++,
                "HelloWorld");
    sirius_debg("[thread: %llu] %u: %d\n", sirius_thread_id, idx++,
                (2 + 3) * 3);

    sirius_debgsp("[thread: %llu] %u: HelloWorld\n", sirius_thread_id, idx++);
    sirius_debgsp("[thread: %llu] %u: %s\n", sirius_thread_id, idx++,
                  "HelloWorld");
    sirius_debgsp("[thread: %llu] %u: %d\n", sirius_thread_id, idx++,
                  (2 + 3) * 3);

    sirius_info("[thread: %llu] %u: HelloWorld\n", sirius_thread_id, idx++);
    sirius_info("[thread: %llu] %u: %s\n", sirius_thread_id, idx++,
                "HelloWorld");
    sirius_info("[thread: %llu] %u: %d\n", sirius_thread_id, idx++,
                (2 + 3) * 3);

    sirius_infosp("[thread: %llu] %u: HelloWorld\n", sirius_thread_id, idx++);
    sirius_infosp("[thread: %llu] %u: %s\n", sirius_thread_id, idx++,
                  "HelloWorld");
    sirius_infosp("[thread: %llu] %u: %d\n", sirius_thread_id, idx++,
                  (2 + 3) * 3);

    sirius_warn("[thread: %llu] %u: HelloWorld\n", sirius_thread_id, idx++);
    sirius_warn("[thread: %llu] %u: %s\n", sirius_thread_id, idx++,
                "HelloWorld");
    sirius_warn("[thread: %llu] %u: %d\n", sirius_thread_id, idx++,
                (2 + 3) * 3);

    sirius_warnsp("[thread: %llu] %u: HelloWorld\n", sirius_thread_id, idx++);
    sirius_warnsp("[thread: %llu] %u: %s\n", sirius_thread_id, idx++,
                  "HelloWorld");
    sirius_warnsp("[thread: %llu] %u: %d\n", sirius_thread_id, idx++,
                  (2 + 3) * 3);

    sirius_error("[thread: %llu] %u: HelloWorld\n", sirius_thread_id, idx++);
    sirius_error("[thread: %llu] %u: %s\n", sirius_thread_id, idx++,
                 "HelloWorld");
    sirius_error("[thread: %llu] %u: %d\n", sirius_thread_id, idx++,
                 (2 + 3) * 3);

    sirius_usleep(10 * 1000);
  }
}

static void *thread_func_wrapper(void *args) {
  if (args)
    ;

  thread_func();
  return NULL;
}

int main() {
  int fd;
  int std_fd;
  sirius_log_config_t cfg = {0};

#ifdef _WIN32
  errno_t err = _sopen_s(&fd, g_file_name, _O_RDWR | _O_CREAT | _O_TRUNC,
                         _SH_DENYNO, _S_IREAD | _S_IWRITE);
  t_assert(err == 0);
#else
  fd = open(g_file_name, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  t_assert(-1 != fd);
#endif

  cfg.fd_err = &fd;
  cfg.fd_out = &fd;
  sirius_log_config(cfg);

  sirius_thread_handle thread[THREAD_CNT];
  for (int i = 0; i < THREAD_CNT; i++) {
    t_assert(
      !sirius_thread_create(&thread[i], NULL, thread_func_wrapper, NULL));
  }

  sirius_usleep(2 * 1000 * 1000);
  std_fd = 1;
  cfg.fd_out = &std_fd;
  sirius_log_config(cfg);

  sirius_usleep(2 * 1000 * 1000);
  std_fd = 2;
  cfg.fd_err = &std_fd;
  sirius_log_config(cfg);

  sirius_usleep(2 * 1000 * 1000);
  cfg.fd_err = &fd;
  cfg.fd_out = &fd;
  sirius_log_config(cfg);

  sirius_usleep(2 * 1000 * 1000);
  g_exit_flag = true;

  for (int i = 0; i < THREAD_CNT; i++) {
    t_assert(!sirius_thread_join(thread[i], NULL));
  }

#ifdef _WIN32
  _close(fd);
#else
  close(fd);
#endif

  return 0;
}
