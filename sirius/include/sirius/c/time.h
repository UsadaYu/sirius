#pragma once

#include "sirius/attributes.h"
#include "sirius/inner/common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Microsecond hibernation.
 *
 * @example
 * - (1) right: ss_usleep(18446744073709551615ULL);
 *
 * - (2) wrong: ss_usleep(18446744073709551615);
 *
 * - (3) right: ss_usleep(1000ULL * 1000 * 1000 * 1000 * 1000);
 *
 * - (4) wrong: ss_usleep(1000 * 1000 * 1000 * 1000 * 1000);
 *
 * - (5) wrong: ss_usleep(1000 * 1000 * 1000 * 1000 * 1000ULL);
 */
SIRIUS_API void ss_usleep(uint64_t usec);

/**
 * @brief Nanosecond hibernation.
 *
 * @example
 * @see `ss_usleep`.
 */
SIRIUS_API void ss_nsleep(uint64_t usec);

/**
 * @brief Get a monotonic clock in microseconds.
 *
 * @return The monotonic cloc in microseconds.
 * On Windows, this is based on `QueryPerformanceCounter`.
 * On other systems, this is based on `clock_gettime(CLOCK_MONOTONIC)`.
 *
 * Returns 0 if the high-resolution timer is not available or fails.
 */
SIRIUS_API uint64_t ss_get_clock_monotonic_us();

/**
 * @brief Get a monotonic clock in nanoseconds.
 *
 * @return The monotonic cloc in nanoseconds.
 * On Windows, this is based on `QueryPerformanceCounter`.
 * On other systems, this is based on `clock_gettime(CLOCK_MONOTONIC)`.
 *
 * Returns 0 if the high-resolution timer is not available or fails.
 */
SIRIUS_API uint64_t ss_get_clock_monotonic_ns();

#ifdef __cplusplus
}
#endif
