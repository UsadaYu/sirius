#pragma once

#include "sirius/attributes.h"
#include "sirius/internal/common.h"

#ifdef __cplusplus
extern "C" {
#endif

enum SiriusOFlags {
  // --- Access Modes (mutual exclusion) ---

  kSIRIUS_O_RDONLY = 0x01,
  kSIRIUS_O_WRONLY = 0x02,
  kSIRIUS_O_RDWR = 0x04,

  // --- Modifiers (Combinable) ---

  /**
   * @brief Create if the file does not exist.
   */
  kSIRIUS_O_CREAT = 0x10,

  /**
   * @brief If the file exists and is opened in write mode, then clear the file
   * (Truncate).
   */
  kSIRIUS_O_TRUNC = 0x20,

  /**
   * @brief Appended to the end of the file when writing.
   */
  kSIRIUS_O_APPEND = 0x40,

  /**
   * @brief Issue an error if the file exists (Must be used in conjunction with
   * `creat`).
   */
  kSIRIUS_O_EXCL = 0x80,
};

// --- Permissions ---
#define SIRIUS_PERM_RW 0666 // Default read and write.
#define SIRIUS_PERM_RO 0444 // Read-only.

/**
 * @brief File opening function.
 *
 * @param[in] path File path (UTF-8).
 * @param[in] flags Combined flag.
 * (For example: kSIRIUS_O_WRONLY | kSIRIUS_O_CREAT | kSIRIUS_O_TRUNC).
 * @param[in] mode Permissions, works when `kSIRIUS_O_CREAT` is specified).
 *
 * @return 0 on success, -1 on failure.
 */
sirius_api int sirius_open(const char *path, int flags, int mode);

/**
 * @brief Close the file descriptor.
 */
sirius_api int sirius_close(int fd);

#ifdef __cplusplus
}
#endif
