#ifndef SIRIUS_INTERNAL_ERRNO_H
#define SIRIUS_INTERNAL_ERRNO_H

#include "sirius/sirius_common.h"

#if defined(_WIN32) || defined(_WIN64)
static inline int sirius_internal_winerr_to_errno(DWORD err) {
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
#else
#  define sirius_internal_winerr_to_errno(err) (err)
#endif

#endif // SIRIUS_INTERNAL_ERRNO_H
