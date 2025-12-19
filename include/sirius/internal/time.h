#ifndef SIRIUS_INTERNAL_TIME_H
#define SIRIUS_INTERNAL_TIME_H

#include "sirius/sirius_attributes.h"
#include "sirius/sirius_common.h"

#if defined(_WIN32) || defined(_WIN64)
#else
#  include <time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(_WIN64)

#else

sirius_api int sirius_internal_nanosleep(const struct timespec *duration,
                                         struct timespec *rem);

sirius_api int sirius_internal_clock_gettime_monotonic(struct timespec *tp);

#endif

#ifdef __cplusplus
}
#endif

#endif // SIRIUS_INTERNAL_TIME_H
