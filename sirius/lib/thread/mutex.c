/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

#include "lib/thread/mutex.h"

SIRIUS_API int ss_mutex_init(ss_mutex_t *mutex, const enum SsMutexType *type) {
  return ss_mutex_init_impl(mutex, type);
}

SIRIUS_API int ss_mutex_destroy(ss_mutex_t *mutex) {
  return ss_mutex_destroy_impl(mutex);
}

SIRIUS_API int ss_mutex_lock(ss_mutex_t *mutex) {
  return ss_mutex_lock_impl(mutex);
}

SIRIUS_API int ss_mutex_unlock(ss_mutex_t *mutex) {
  return ss_mutex_unlock_impl(mutex);
}

SIRIUS_API int ss_mutex_trylock(ss_mutex_t *mutex) {
  return ss_mutex_trylock_impl(mutex);
}
