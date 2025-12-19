#ifndef SIRIUS_INTERNAL_SEM_H
#define SIRIUS_INTERNAL_SEM_H

#include "sirius/sirius_attributes.h"
#include "sirius/sirius_common.h"

#if defined(_WIN32) || defined(_WIN64)
#  include <windows.h>
#else
#  include <semaphore.h>
#  include <time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(_WIN64)

#else

sirius_api int
sirius_internal_sem_timedwait(sem_t *__restrict sem,
                              const struct timespec *__restrict abstime);

#endif

#ifdef __cplusplus
}
#endif

#endif // SIRIUS_INTERNAL_SEM_H
