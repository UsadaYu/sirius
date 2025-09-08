/**
 * @name time.h
 *
 * @author UsadaYu
 *
 * @date
 * Create: 2025-05-20
 * Update: 2025-05-20
 *
 * @brief Time.
 *
 * @note In this header file, the calculation of time involves the maximum
 * length of the data type. Therefore, the following points need to be
 * clarified:
 * - (1) In Windows environment, `DWORD` is usually of type `unsigned long`.
 */

#ifndef SIRIUS_CUSTOM_TIME_H
#define SIRIUS_CUSTOM_TIME_H

/**
 * The header file `Windows.h` must be included first.
 */
#ifdef _WIN32
#include <Windows.h>
#endif

#ifdef _WIN32
#include <limits.h>
#include <mmsystem.h>
#include <timeapi.h>
#else
#include <errno.h>
#include <time.h>
#endif
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline void _custom_usleep(uint64_t usec) {
  if (usec == 0) return;

#ifdef _WIN32
  uint64_t sleep_ms = usec / 1000ULL;
  uint64_t remaining_usec = usec % 1000ULL;

  TIMECAPS tc;
  UINT period = 1;
  if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) == TIMERR_NOERROR) {
    period = tc.wPeriodMin < 1 ? 1 : tc.wPeriodMin;
    period = period > tc.wPeriodMax ? tc.wPeriodMax : period;
    timeBeginPeriod(period);
  } else {
    period = 0;
  }

  while (sleep_ms > 0) {
    DWORD chunk = sleep_ms > (uint64_t)MAXDWORD ? MAXDWORD : (DWORD)sleep_ms;
    Sleep(chunk);
    sleep_ms -= chunk;
  }

  if (remaining_usec > 0) {
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    double remaining_seconds = (double)remaining_usec / 1e6;
    LONGLONG target =
        start.QuadPart + (LONGLONG)(remaining_seconds * freq.QuadPart);

    do {
      QueryPerformanceCounter(&end);
      if (end.QuadPart >= target) break;

      /**
       * Make way for CPU time slices to reduce the CPU usage during busy
       * waiting.
       */
      Sleep(0);
    } while (1);
  }

  if (period != 0) {
    timeEndPeriod(period);
  }

#else
  struct timespec req, rem;
  req.tv_sec = usec / 1000000ULL;
  req.tv_nsec = (usec % 1000000ULL) * 1000ULL;

  while (nanosleep(&req, &rem) == -1) {
    if (errno == EINTR) {
      req.tv_sec = rem.tv_sec;
      req.tv_nsec = rem.tv_nsec;
    } else {
      break;
    }
  }
#endif
}

static inline void _custom_nsleep(uint64_t nsec) {
  if (nsec == 0) return;

#ifdef _WIN32
  uint64_t sleep_ms = nsec / 1000000ULL;
  uint64_t remaining_nsec = nsec % 1000000ULL;

  TIMECAPS tc;
  UINT period = 1;
  if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) == TIMERR_NOERROR) {
    period = tc.wPeriodMin < 1 ? 1 : tc.wPeriodMin;
    period = period > tc.wPeriodMax ? tc.wPeriodMax : period;
    timeBeginPeriod(period);
  } else {
    period = 0;
  }

  while (sleep_ms > 0) {
    DWORD chunk = sleep_ms > (uint64_t)MAXDWORD ? MAXDWORD : (DWORD)sleep_ms;
    Sleep(chunk);
    sleep_ms -= chunk;
  }

  if (remaining_nsec > 0) {
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    double remaining_seconds = (double)remaining_nsec / 1e9;
    LONGLONG target =
        start.QuadPart + (LONGLONG)(remaining_seconds * freq.QuadPart);

    do {
      QueryPerformanceCounter(&end);
      if (end.QuadPart >= target) break;

      /**
       * Make way for CPU time slices to reduce the CPU usage during busy
       * waiting.
       */
      Sleep(0);
    } while (1);
  }

  if (period != 0) {
    timeEndPeriod(period);
  }

#else
  struct timespec req, rem;
  req.tv_sec = nsec / 1000000000ULL;
  req.tv_nsec = nsec % 1000000000ULL;

  while (nanosleep(&req, &rem) == -1) {
    if (errno == EINTR) {
      req.tv_sec = rem.tv_sec;
      req.tv_nsec = rem.tv_nsec;
    } else {
      break;
    }
  }

#endif
}

static inline uint64_t _custom_get_time_us(void) {
#ifdef _WIN32
  static LARGE_INTEGER qpc_frequency_us = {0};
  LARGE_INTEGER counter;

  if (qpc_frequency_us.QuadPart == 0) {
    if (!QueryPerformanceFrequency(&qpc_frequency_us)) {
      return 0;
    }
  }

  if (!QueryPerformanceCounter(&counter)) {
    return 0;
  }

  uint64_t whole_seconds = counter.QuadPart / qpc_frequency_us.QuadPart;
  uint64_t remainder_ticks = counter.QuadPart % qpc_frequency_us.QuadPart;
  uint64_t microseconds = whole_seconds * 1000000ULL;
  microseconds += (remainder_ticks * 1000000ULL) / qpc_frequency_us.QuadPart;
  return microseconds;

#else
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
  } else {
    return 0;
  }

#endif
}

static inline uint64_t _custom_get_time_ns() {
#ifdef _WIN32
  static LARGE_INTEGER frequency = {0};
  LARGE_INTEGER counter;

  if (frequency.QuadPart == 0) {
    if (!QueryPerformanceFrequency(&frequency)) {
      return 0;
    }
  }

  if (!QueryPerformanceCounter(&counter)) {
    return 0;
  }

  uint64_t whole_seconds = counter.QuadPart / frequency.QuadPart;
  uint64_t remainder_ticks = counter.QuadPart % frequency.QuadPart;
  uint64_t nanoseconds = whole_seconds * 1000000000ULL;
  nanoseconds += (remainder_ticks * 1000000000ULL) / frequency.QuadPart;
  return nanoseconds;

#else
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
  } else {
    return 0;
  }

#endif
}

#ifdef __cplusplus
}
#endif

#endif  // SIRIUS_CUSTOM_TIME_H
