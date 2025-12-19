#include "sirius/sirius_time.h"

#if defined(_WIN32) || defined(_WIN64)

#else

sirius_api int sirius_internal_nanosleep(const struct timespec *duration,
                                         struct timespec *rem) {
  return nanosleep(duration, rem);
}

sirius_api int sirius_internal_clock_gettime_monotonic(struct timespec *tp) {
  return clock_gettime(CLOCK_MONOTONIC, tp);
}

#endif
