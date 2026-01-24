#pragma once

#include "sirius/attributes.h"
#include "sirius/internal/common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Microsecond hibernation.
 *
 * @example
 * - (1) right: sirius_usleep(18446744073709551615ULL);
 *
 * - (2) wrong: sirius_usleep(18446744073709551615);
 *
 * - (3) right: sirius_usleep(1000ULL * 1000 * 1000 * 1000 * 1000);
 *
 * - (4) wrong: sirius_usleep(1000 * 1000 * 1000 * 1000 * 1000);
 *
 * - (5) wrong: sirius_usleep(1000 * 1000 * 1000 * 1000 * 1000ULL);
 */
sirius_api void sirius_usleep(uint64_t usec);

/**
 * @brief Nanosecond hibernation.
 *
 * @example
 * @see `sirius_usleep`.
 */
sirius_api void sirius_nsleep(uint64_t usec);

/**
 * @brief Get high-resolution system timestamp in microseconds.
 *
 * @return System timestamp in microseconds.
 * On Windows, this is based on `QueryPerformanceCounter`.
 * On other systems, this is based on `clock_gettime(CLOCK_MONOTONIC)`.
 *
 * Returns 0 if the high-resolution timer is not available or fails.
 */
sirius_api uint64_t sirius_get_time_us();

/**
 * @brief Get high-resolution system timestamp in nanoseconds.
 *
 * @return System timestamp in nanoseconds.
 * On Windows, this is based on `QueryPerformanceCounter`.
 * On other systems, this is based on `clock_gettime(CLOCK_MONOTONIC)`.
 *
 * Returns 0 if the high-resolution timer is not available or fails.
 */
sirius_api uint64_t sirius_get_time_ns();

#ifdef __cplusplus
}
#endif
