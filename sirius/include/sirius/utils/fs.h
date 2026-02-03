#pragma once

#include "sirius/internal/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  // --- Access Modes (mutual exclusion) ---

  ss_fs_acc_rdonly = 0x01,
  ss_fs_acc_wronly = 0x02,
  ss_fs_acc_rdwr = 0x04,

  // --- Modifiers (Combinable) ---

  /**
   * @brief Create if the file does not exist.
   */
  ss_fs_opt_creat = 0x10,

  /**
   * @brief If the file exists and is opened in write mode, then clear the file
   * (Truncate).
   */
  ss_fs_opt_trunc = 0x20,

  /**
   * @brief Appended to the end of the file when writing.
   */
  ss_fs_opt_append = 0x40,

  /**
   * @brief Issue an error if the file exists (Must be used in conjunction with
   * `creat`).
   */
  ss_fs_opt_excl = 0x80,
} ss_fs_flags_t;

// --- Permissions ---
#define SS_FS_PERM_RW 0666 // Default read and write.
#define SS_FS_PERM_RO 0444 // Read-only.

/**
 * @brief File opening function.
 *
 * @param path File path (UTF-8).
 * @param flags Combined flag.
 * (For example: ss_fs_acc_wronly | ss_fs_opt_creat | ss_fs_opt_trunc)
 * @param mode Permissions, works when `ss_fs_opt_creat` is specified).
 *
 * @return 0 on success, -1 on failure.
 */
int ss_fs_open(const char *path, int flags, int mode);

/**
 * @brief Close the file descriptor.
 */
int ss_fs_close(int fd);

#ifdef __cplusplus
}
#endif
