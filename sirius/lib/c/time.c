#include "sirius/c/time.h"

#include "sirius/thread/cpu.h"
#include "utils/decls.h"

#if defined(_WIN32) || defined(_WIN64)
#  include <mmsystem.h>
#  include <timeapi.h>
#endif

sirius_api void sirius_usleep(uint64_t usec) {
  if (usec == 0)
    return;

#if defined(_WIN32) || defined(_WIN64)
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
      if (end.QuadPart >= target)
        break;

      sirius_cpu_relax();
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

sirius_api void sirius_nsleep(uint64_t nsec) {
  if (nsec == 0)
    return;

#if defined(_WIN32) || defined(_WIN64)
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
      if (end.QuadPart >= target)
        break;

      sirius_cpu_relax();
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

sirius_api uint64_t sirius_get_clock_monotonic_us() {
#if defined(_WIN32) || defined(_WIN64)
  static LARGE_INTEGER qpc_frequency_us = {0};
  LARGE_INTEGER counter;

  if (unlikely(qpc_frequency_us.QuadPart == 0)) {
    if (unlikely(!QueryPerformanceFrequency(&qpc_frequency_us))) {
      return 0;
    }
  }

  if (unlikely(!QueryPerformanceCounter(&counter)))
    return 0;

  uint64_t whole_seconds = counter.QuadPart / qpc_frequency_us.QuadPart;
  uint64_t remainder_ticks = counter.QuadPart % qpc_frequency_us.QuadPart;
  uint64_t microseconds = whole_seconds * 1000000ULL;
  microseconds += (remainder_ticks * 1000000ULL) / qpc_frequency_us.QuadPart;
  return microseconds;
#else
  struct timespec ts;
  if (likely(clock_gettime(CLOCK_MONOTONIC, &ts) == 0)) {
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
  } else {
    return 0;
  }
#endif
}

sirius_api uint64_t sirius_get_clock_monotonic_ns() {
#if defined(_WIN32) || defined(_WIN64)
  static LARGE_INTEGER frequency = {0};
  LARGE_INTEGER counter;

  if (unlikely(frequency.QuadPart == 0)) {
    if (unlikely(!QueryPerformanceFrequency(&frequency)))
      return 0;
  }

  if (unlikely(!QueryPerformanceCounter(&counter)))
    return 0;

  uint64_t whole_seconds = counter.QuadPart / frequency.QuadPart;
  uint64_t remainder_ticks = counter.QuadPart % frequency.QuadPart;
  uint64_t nanoseconds = whole_seconds * 1000000000ULL;
  nanoseconds += (remainder_ticks * 1000000000ULL) / frequency.QuadPart;
  return nanoseconds;
#else
  struct timespec ts;
  if (likely(clock_gettime(CLOCK_MONOTONIC, &ts) == 0)) {
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
  } else {
    return 0;
  }
#endif
}
