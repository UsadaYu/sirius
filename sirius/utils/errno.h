#include "utils/decls.h"
#pragma once

#if defined(_WIN32) || defined(_WIN64)
static inline int utils_winerr_to_errno(const DWORD err_code) {
  const DWORD dw_err = err_code;

  switch (dw_err) {
  case ERROR_SUCCESS:
    return 0;

  case ERROR_INVALID_PARAMETER:
  case ERROR_INVALID_FUNCTION:
    return EINVAL;

  case ERROR_NOT_ENOUGH_MEMORY:
  case ERROR_OUTOFMEMORY:
    return ENOMEM;

  case ERROR_ACCESS_DENIED:
    return EACCES;

  case ERROR_INVALID_HANDLE:
    return EBADF;

  case ERROR_TOO_MANY_SEMAPHORES:
  case ERROR_TOO_MANY_OPEN_FILES:
    return EMFILE;

  case ERROR_BUSY:
    return EBUSY;

  /**
   * @brief Related to the file system.
   */
  case ERROR_FILE_NOT_FOUND:
  case ERROR_PATH_NOT_FOUND:
    return ENOENT;

  case ERROR_ALREADY_EXISTS:
  case ERROR_FILE_EXISTS:
    return EEXIST;

  case ERROR_DISK_FULL:
    return ENOSPC;

  case ERROR_DIR_NOT_EMPTY:
    return ENOTEMPTY;

  case ERROR_READ_FAULT:
  case ERROR_WRITE_FAULT:
    return EIO;

  case ERROR_SHARING_VIOLATION:
    return EACCES;

  case ERROR_LOCK_VIOLATION:
    return EAGAIN;

  case ERROR_NOT_READY:
    return EBUSY;

  case ERROR_BROKEN_PIPE:
    return EPIPE;

  case ERROR_INSUFFICIENT_BUFFER:
    return ENOBUFS;

  /**
   * @brief Related to the network.
   */
  case ERROR_NETNAME_DELETED:
    return ECONNRESET;

  case ERROR_CONNECTION_REFUSED:
    return ECONNREFUSED;

  case ERROR_CONNECTION_ABORTED:
    return ECONNABORTED;

  case ERROR_NETWORK_UNREACHABLE:
    return ENETUNREACH;

  case ERROR_HOST_UNREACHABLE:
    return EHOSTUNREACH;

  case ERROR_SEM_TIMEOUT:
  case WAIT_TIMEOUT:
    return ETIMEDOUT;

  case ERROR_NOT_SUPPORTED:
    return ENOTSUP;

  case ERROR_OPERATION_ABORTED:
    return ECANCELED;

  /**
   * @brief Specific scenarios.
   */
  case ERROR_NO_MORE_FILES:
    return ENOENT;

  case ERROR_INVALID_ACCESS:
    return EACCES;

  default:
    /**
     * @note Best-effort mapping.
     */
    if (dw_err >= WSABASEERR) {
      /**
       * @note If it is a winsock error, special handling may be required.
       */
      return EIO;
    }
    return EINVAL;
  }
}
#else
#  define utils_winerr_to_errno(err_code) (err_code)
#endif
