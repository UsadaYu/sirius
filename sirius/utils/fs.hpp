#pragma once
/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

#include "sirius/c/fs.h"

#if defined(_WIN32) || defined(_WIN64)
#  include <io.h>
#  include <share.h>
#endif

namespace sirius {
namespace utils {
namespace fs {
#if defined(_WIN32) || defined(_WIN64)
inline int win_map_flags(int flags) {
  int os_flags = 0;

  os_flags |= _O_BINARY;
  if (flags & kSIRIUS_O_RDONLY) {
    os_flags |= _O_RDONLY;
  }
  if (flags & kSIRIUS_O_WRONLY) {
    os_flags |= _O_WRONLY;
  }
  if (flags & kSIRIUS_O_RDWR) {
    os_flags |= _O_RDWR;
  }

  if (flags & kSIRIUS_O_CREAT) {
    os_flags |= _O_CREAT;
  }
  if (flags & kSIRIUS_O_TRUNC) {
    os_flags |= _O_TRUNC;
  }
  if (flags & kSIRIUS_O_APPEND) {
    os_flags |= _O_APPEND;
  }
  if (flags & kSIRIUS_O_EXCL) {
    os_flags |= _O_EXCL;
  }

  return os_flags;
}

inline int win_map_mode(int mode) {
  int os_mode = 0;

  if (mode & 0200) {
    os_mode |= _S_IWRITE;
  }
  if (mode & 0400) {
    os_mode |= _S_IREAD;
  }

  return os_mode;
}

inline int fs_open_impl(const char *path, int flags, int mode) {
  int fd = -1;
  int os_flags = win_map_flags(flags);
  int os_mode = win_map_mode(mode);

  /**
   * @note _SH_DENYNO: Allow other processes to read and write.
   */
  errno_t err = _sopen_s(&fd, path, os_flags, _SH_DENYNO, os_mode);
  if (err != 0) {
    errno = err;
    return -1;
  }
  return fd;
}

inline int fs_close_impl(int fd) {
  return _close(fd);
}
#else
inline int posix_map_flags(int flags) {
  int os_flags = 0;

  if (flags & kSIRIUS_O_RDONLY) {
    os_flags |= O_RDONLY;
  }
  if (flags & kSIRIUS_O_WRONLY) {
    os_flags |= O_WRONLY;
  }
  if (flags & kSIRIUS_O_RDWR) {
    os_flags |= O_RDWR;
  }

  if (flags & kSIRIUS_O_CREAT) {
    os_flags |= O_CREAT;
  }
  if (flags & kSIRIUS_O_TRUNC) {
    os_flags |= O_TRUNC;
  }
  if (flags & kSIRIUS_O_APPEND) {
    os_flags |= O_APPEND;
  }
  if (flags & kSIRIUS_O_EXCL) {
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

inline int fs_open_impl(const char *path, int flags, int mode) {
  return open(path, posix_map_flags(flags), (mode_t)mode);
}

inline int fs_close_impl(int fd) {
  return close(fd);
}
#endif
} // namespace fs
} // namespace utils
} // namespace sirius
