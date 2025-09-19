/**
 * @name sirius_errno.h
 *
 * @author UsadaYu
 *
 * @date
 * Create: 2024-07-30
 * Update: 2025-08-14
 *
 * @brief Error code.
 */

#ifndef SIRIUS_ERRNO_H
#define SIRIUS_ERRNO_H

typedef enum {
  sirius_err_ok = 0, // Ok.

  /**
   * @brief Common error.
   */
  sirius_err_timeout = -10000,      // Timeout.
  sirius_err_null_pointer = -10001, // Null pointer.
  sirius_err_args = -10002,         // Invalid arguments.

  /**
   * @brief Function error.
   */
  sirius_err_entry = -11000,         // Function entry invalid.
  sirius_err_init_repeated = -11001, // Repeated initialization.
  sirius_err_not_init = -11002,      // Uninitialized.

  /**
   * @brief Resource error.
   */
  sirius_err_memory_alloc = -12000,   // Fail to alloc memory.
  sirius_err_cache_overflow = -12001, // Cache overflow.
  sirius_err_resource_alloc = -12002, // Resource request failure.
  sirius_err_resource_free = -12003,  // Resource release failure.
  sirius_err_resource_busy = -12004,  // Resource is busy.

  /**
   * @brief Error in operation on file.
   */
  sirius_err_file_open = -13000,  // File open failure.
  sirius_err_file_write = -13001, // File write failure.
  sirius_err_file_read = -13002,  // File read failure.

  /**
   * @brief Error in operation on streaming data.
   */
  sirius_err_url_end = -14000, // The url is read to the end.
} sirius_err_t;

#endif // SIRIUS_ERRNO_H
