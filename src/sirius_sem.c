#include "sirius/sirius_sem.h"

#if defined(_WIN32) || defined(_WIN64)

#else

sirius_api int
sirius_internal_sem_timedwait(sem_t *__restrict sem,
                              const struct timespec *__restrict abstime) {
  return sem_timedwait(sem, abstime);
}

#endif
