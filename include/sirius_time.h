/**
 * @name sirius_thread.h
 *
 * @author UsadaYu
 *
 * @date create: 2024-11-01
 * @date update: 2024-11-16
 *
 * @brief Time.
 */

#ifndef __SIRIUS_TIME_H__
#define __SIRIUS_TIME_H__

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
void sirius_usleep(unsigned long long usec);

#ifdef __cplusplus
}
#endif

#endif  // __SIRIUS_TIME_H__
