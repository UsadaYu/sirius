#pragma once

#include "sirius/attributes.h"
#include "sirius/inner/common.h"

#ifdef __cplusplus
extern "C" {
#endif

enum SsOFlags {
  // --- Access Modes (mutual exclusion) ---

  kSS_O_RDONLY = 0x01,
  kSS_O_WRONLY = 0x02,
  kSS_O_RDWR = 0x04,

  // --- Modifiers (Combinable) ---

  /**
   * @brief Create if the file does not exist.
   */
  kSS_O_CREAT = 0x10,

  /**
   * @brief If the file exists and is opened in write mode, then clear the file
   * (Truncate).
   */
  kSS_O_TRUNC = 0x20,

  /**
   * @brief Appended to the end of the file when writing.
   */
  kSS_O_APPEND = 0x40,

  /**
   * @brief Issue an error if the file exists (Must be used in conjunction with
   * `creat`).
   */
  kSS_O_EXCL = 0x80,
};

// --- Permissions ---
#define SS_FS_PERM_RW 0666 // Default read and write.
#define SS_FS_PERM_RO 0444 // Read-only.

/**
 * @brief File opening function.
 *
 * @param[in] path File path (UTF-8).
 * @param[in] flags Combined flag.
 * (For example: kSS_O_WRONLY | kSS_O_CREAT | kSS_O_TRUNC).
 * @param[in] mode Permissions, works when `kSS_O_CREAT` is specified).
 *
 * @return 0 on success, -1 on failure.
 */
SIRIUS_API int ss_fs_open(const char *path, int flags, int mode);

/**
 * @brief Close the file descriptor.
 */
SIRIUS_API int ss_fs_close(int fd);

#ifdef __cplusplus
}
#endif
