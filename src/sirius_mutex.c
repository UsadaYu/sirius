#include "sirius_mutex.h"

#include "internal/internal_log.h"
#include "internal/internal_sys.h"
#include "sirius_errno.h"

sirius_api int sirius_mutex_init(
    sirius_mutex_handle *handle,
    const sirius_mutex_attr_t *attr) {
  if (unlikely(!handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

#ifdef _WIN32
  if (attr) {
    switch (*attr) {
      case sirius_mutex_normal:
        break;
      default:
        internal_warn("Invalid mutex attr: %d\n", *attr);
    }
  }

  InitializeCriticalSection(handle);

#else
  int ret = -1;
  pthread_mutexattr_t mutex_attr;
  if (attr) {
    ret = pthread_mutexattr_init(&mutex_attr);
    if (ret) {
      internal_str_warn(ret, "pthread_mutexattr_init");
    } else {
      int attr_type;
      switch (*attr) {
        case sirius_mutex_recursive:
          attr_type = PTHREAD_MUTEX_RECURSIVE;
          break;
        case sirius_mutex_errorcheck:
          attr_type = PTHREAD_MUTEX_ERRORCHECK;
          break;
        default:
          attr_type = PTHREAD_MUTEX_NORMAL;
          break;
      }

      ret = pthread_mutexattr_settype(&mutex_attr,
                                      attr_type);
      if (ret) {
        internal_str_warn(ret,
                          "pthread_mutexattr_settype");
        pthread_mutexattr_destroy(&mutex_attr);
      }
    }
  }

  pthread_mutexattr_t *mutex_attr_ptr =
      ret ? nullptr : &mutex_attr;
  ret = pthread_mutex_init(handle, mutex_attr_ptr);
  if (mutex_attr_ptr) {
    pthread_mutexattr_destroy(&mutex_attr);
  }
  if (ret) {
    internal_str_error(ret, "pthread_mutex_init");
    return sirius_err_resource_alloc;
  }

#endif

  return 0;
}

sirius_api int sirius_mutex_destroy(
    sirius_mutex_handle *handle) {
  if (unlikely(!handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

#ifdef _WIN32
  DeleteCriticalSection(handle);

#else
  int ret = pthread_mutex_destroy(handle);
  if (ret) {
    internal_str_error(ret, "pthread_mutex_destroy");
    return sirius_err_resource_free;
  }

#endif

  return 0;
}

sirius_api int sirius_mutex_lock(
    sirius_mutex_handle *handle) {
  if (unlikely(!handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

#ifdef _WIN32
  EnterCriticalSection(handle);

#else
  int ret = pthread_mutex_lock(handle);
  if (unlikely(ret)) {
    internal_str_error(ret, "pthread_mutex_lock");
    return -1;
  }

#endif

  return 0;
}

sirius_api int sirius_mutex_unlock(
    sirius_mutex_handle *handle) {
  if (unlikely(!handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

#ifdef _WIN32
  LeaveCriticalSection(handle);

#else
  int ret = pthread_mutex_unlock(handle);
  if (unlikely(ret)) {
    internal_str_error(ret, "pthread_mutex_unlock");
    return -1;
  }

#endif

  return 0;
}

sirius_api int sirius_mutex_trylock(
    sirius_mutex_handle *handle) {
  if (unlikely(!handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

#ifdef _WIN32
  return TryEnterCriticalSection(handle)
             ? 0
             : sirius_err_resource_busy;

#else
  int ret = pthread_mutex_trylock(handle);
  if (ret) {
    if (EBUSY == ret) {
      return sirius_err_resource_busy;
    } else {
      internal_str_error(ret, "pthread_mutex_trylock");
      return -1;
    }
  }

  return 0;

#endif
}
