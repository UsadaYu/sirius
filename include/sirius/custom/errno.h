#ifndef SIRIUS_CUSTOM_ERRNO_H
#define SIRIUS_CUSTOM_ERRNO_H

#ifdef _WIN32
#  include <Windows.h>
#endif

#include <errno.h>

#ifdef _WIN32
static inline int sirius_custom_win32err_to_errno(DWORD err) {
  switch (err) {
  case ERROR_SUCCESS:
    return 0;
  case ERROR_INVALID_PARAMETER:
    return EINVAL;
  case ERROR_NOT_ENOUGH_MEMORY:
  case ERROR_OUTOFMEMORY:
    return ENOMEM;
  case ERROR_ACCESS_DENIED:
    return EACCES;
  case ERROR_INVALID_HANDLE:
    return EBADF;
  case ERROR_TOO_MANY_SEMAPHORES:
    return ENOSPC;
  case ERROR_BUSY:
    return EBUSY;
  default:
    /**
     * @note Best-effort mapping.
     */
    return EINVAL;
  }
}
#endif

#endif // SIRIUS_CUSTOM_ERRNO_H
