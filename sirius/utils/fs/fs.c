#include "sirius/utils/fs.h"

#include "utils/decls.h"

#if defined(_WIN32) || defined(_WIN64)
#  include <io.h>
#  include <share.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
static int win_map_flags(int flags) {
  int os_flags = 0;

  os_flags |= _O_BINARY;

  if (flags & ss_fs_acc_rdonly) {
    os_flags |= _O_RDONLY;
  }
  if (flags & ss_fs_acc_wronly) {
    os_flags |= _O_WRONLY;
  }
  if (flags & ss_fs_acc_rdwr) {
    os_flags |= _O_RDWR;
  }

  if (flags & ss_fs_opt_creat) {
    os_flags |= _O_CREAT;
  }
  if (flags & ss_fs_opt_trunc) {
    os_flags |= _O_TRUNC;
  }
  if (flags & ss_fs_opt_append) {
    os_flags |= _O_APPEND;
  }
  if (flags & ss_fs_opt_excl) {
    os_flags |= _O_EXCL;
  }

  return os_flags;
}

static int win_map_mode(int mode) {
  int os_mode = 0;

  if (mode & 0200) {
    os_mode |= _S_IWRITE;
  }
  if (mode & 0400) {
    os_mode |= _S_IREAD;
  }

  return os_mode;
}

int ss_fs_open(const char *path, int flags, int mode) {
  int fd = -1;
  int os_flags = win_map_flags(flags);
  int os_mode = win_map_mode(mode);

  /**
   * @note _SH_DENYNO: Allow other processes to read and write.
   */
  errno_t err = _sopen_s(&fd, path, os_flags, _SH_DENYNO, os_mode);

  if (err != 0) {
    /**
     * @note When `_sopen_s` fails, it will set `errno`, but also return an
     * error code. Here it make sure `errno` is set correctly for external
     * inspection.
     */
    errno = err;
    return -1;
  }

  return fd;
}

int ss_fs_close(int fd) {
  return _close(fd);
}
#else
static int posix_map_flags(int flags) {
  int os_flags = 0;

  if (flags & ss_fs_acc_rdonly) {
    os_flags |= O_RDONLY;
  }
  if (flags & ss_fs_acc_wronly) {
    os_flags |= O_WRONLY;
  }
  if (flags & ss_fs_acc_rdwr) {
    os_flags |= O_RDWR;
  }

  if (flags & ss_fs_opt_creat) {
    os_flags |= O_CREAT;
  }
  if (flags & ss_fs_opt_trunc) {
    os_flags |= O_TRUNC;
  }
  if (flags & ss_fs_opt_append) {
    os_flags |= O_APPEND;
  }
  if (flags & ss_fs_opt_excl) {
    os_flags |= O_EXCL;
  }

  /**
   * @note Add `O_CLOEXEC` to prevent fd from being leaked to child processes
   * during exec.
   * On Windows, it needs to be set up through the dedicated API.
   */
  os_flags |= O_CLOEXEC;

  return os_flags;
}

int ss_fs_open(const char *path, int flags, int mode) {
  return open(path, posix_map_flags(flags), (mode_t)mode);
}

int ss_fs_close(int fd) {
  return close(fd);
}
#endif
