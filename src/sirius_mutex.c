#include "sirius/sirius_mutex.h"

#if defined(_WIN32) || defined(_WIN64)

#else

sirius_api int sirius_mutex_init(sirius_mutex_t *mutex,
                                 const sirius_mutex_type_t *type) {
  if (unlikely(!mutex))
    return EINVAL;

  int ret;
  pthread_mutexattr_t attr;

  /**
   * @note Default to a normal mutex if type is not specified or if `attr_init`
   * fails.
   */
  if (!type || pthread_mutexattr_init(&attr) != 0)
    return pthread_mutex_init(mutex, NULL);

  int native_type = *type == sirius_mutex_recursive ? PTHREAD_MUTEX_RECURSIVE
                                                    : PTHREAD_MUTEX_NORMAL;

  ret = pthread_mutexattr_settype(&attr, native_type);
  if (ret != 0) {
    pthread_mutexattr_destroy(&attr);
    return ret;
  }

  ret = pthread_mutex_init(mutex, &attr);
  pthread_mutexattr_destroy(&attr);
  return ret;
}

#endif
