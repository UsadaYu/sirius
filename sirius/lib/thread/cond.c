/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

#include "lib/thread/cond.h"

SIRIUS_API int ss_cond_init(ss_cond_t *__restrict cond,
                            const enum SsThreadProcess *__restrict type) {
  return ss_cond_init_impl(cond, type);
}

SIRIUS_API int ss_cond_destroy(ss_cond_t *cond) {
  return ss_cond_destroy_impl(cond);
}

SIRIUS_API int ss_cond_wait(ss_cond_t *__restrict cond,
                            ss_mutex_t *__restrict mutex) {
  return ss_cond_wait_impl(cond, mutex);
}

SIRIUS_API int ss_cond_timedwait(ss_cond_t *__restrict cond,
                                 ss_mutex_t *__restrict mutex,
                                 uint64_t milliseconds) {
  return ss_cond_timedwait_impl(cond, mutex, milliseconds);
}

SIRIUS_API int ss_cond_signal(ss_cond_t *cond) {
  return ss_cond_signal_impl(cond);
}

SIRIUS_API int ss_cond_broadcast(ss_cond_t *cond) {
  return ss_cond_broadcast_impl(cond);
}
