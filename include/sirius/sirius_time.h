/**
 * @name sirius_time.h
 *
 * @author UsadaYu
 *
 * @date
 *  Create: 2024-11-01
 *  Update: 2025-05-12
 *
 * @brief Time.
 */

#ifndef __SIRIUS_TIME_H__
#define __SIRIUS_TIME_H__

#include "custom/custom_time.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Microsecond hibernation.
 *
 * @example
 *  right:
 *  sirius_usleep(18446744073709551615ULL);
 *
 *  wrong:
 *  sirius_usleep(18446744073709551615);
 *
 *  right:
 *  sirius_usleep(1000ULL * 1000 * 1000 * 1000 * 1000);
 *
 *  wrong:
 *  sirius_usleep(1000 * 1000 * 1000 * 1000 * 1000);
 *
 *  wrong:
 *  sirius_usleep(1000 * 1000 * 1000 * 1000 * 1000ULL);
 */
static inline void sirius_usleep(unsigned long long usec) {
  _custom_usleep(usec);
}

/**
 * @brief Nanosecond hibernation.
 *
 * @example
    Reference the function `sirius_usleep`.
 */
static inline void sirius_nsleep(unsigned long long usec) {
  _custom_nsleep(usec);
}

/**
 * @brief Get high-resolution system timestamp in
 *  microseconds.
 *
 * @return System timestamp in microseconds.
 *  On Windows, this is based on `QueryPerformanceCounter`.
 *  On other systems, this is based on
 * `clock_gettime(CLOCK_MONOTONIC)`.
 *
 *  Returns 0 if the high-resolution timer is not available
 *  or fails.
 */
static inline unsigned long long sirius_get_time_us() {
  return _custom_get_time_us();
}

/**
 * @brief Get high-resolution system timestamp in
 *  nanoseconds.
 *
 * @return System timestamp in nanoseconds.
 *  On Windows, this is based on `QueryPerformanceCounter`.
 *  On other systems, this is based on
 *  `clock_gettime(CLOCK_MONOTONIC)`.
 *
 *  Returns 0 if the high-resolution timer is not available
 *  or fails.
 */
static inline unsigned long long sirius_get_time_ns() {
  return _custom_get_time_ns();
}

#ifdef __cplusplus
}
#endif

#endif  // __SIRIUS_TIME_H__
