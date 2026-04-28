/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

#include "lib/thread/sem.h"

SIRIUS_API int ss_sem_init(ss_sem_t *sem, int pshared, unsigned int value) {
  return ss_sem_init_impl(sem, pshared, value);
}

SIRIUS_API int ss_sem_destroy(ss_sem_t *sem) {
  return ss_sem_destroy_impl(sem);
}

SIRIUS_API int ss_sem_wait(ss_sem_t *sem) {
  return ss_sem_wait_impl(sem);
}

SIRIUS_API int ss_sem_trywait(ss_sem_t *sem) {
  return ss_sem_trywait_impl(sem);
}

SIRIUS_API int ss_sem_timedwait(ss_sem_t *sem, uint64_t milliseconds) {
  return ss_sem_timedwait_impl(sem, milliseconds);
}

SIRIUS_API int ss_sem_post(ss_sem_t *sem) {
  return ss_sem_post_impl(sem);
}

#if !defined(_WIN32) && !defined(_WIN64)
SIRIUS_API int ss_sem_getvalue(ss_sem_t *__restrict sem, int *__restrict sval) {
  return ss_sem_getvalue_impl(sem, sval);
}
#endif
