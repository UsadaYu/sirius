/**
 * @name sirius_errno.h
 *
 * @author UsadaYu
 *
 * @date
 *  Create: 2024-07-30
 *  Update: 2025-01-06
 *
 * @brief Error code.
 */

#ifndef __SIRIUS_ERRNO_H__
#define __SIRIUS_ERRNO_H__

/**
 * @brief Common error.
 */

/* Timeout. */
#define sirius_err_timeout (-10000)

/* Null pointer. */
#define sirius_err_null_pointer (-10001)

/* Invalid arguments. */
#define sirius_err_args (-10002)

/**
 * @brief Function error.
 */

/* Function entry invalid. */
#define sirius_err_entry (-11000)

/* Repeated initialization. */
#define sirius_err_init_repeated (-11001)

/* Uninitialized. */
#define sirius_err_not_init (-11002)

/**
 * @brief Resource error.
 */

/* Fail to alloc memory. */
#define sirius_err_memory_alloc (-12000)

/* Cache overflow. */
#define sirius_err_cache_overflow (-12001)

/* Resource request failure. */
#define sirius_err_resource_alloc (-12002)

/* Resource release failure. */
#define sirius_err_resource_free (-12003)

/**
 * @brief Error in operation on file.
 */

/* File open failure. */
#define sirius_err_file_open (-13000)

/* File write failure. */
#define sirius_err_file_write (-13100)

/* File read failure. */
#define sirius_err_file_read (-13200)

/* The file is read to the end. */
#define sirius_err_file_read_end (-13201)

#endif  // __SIRIUS_ERRNO_H__
