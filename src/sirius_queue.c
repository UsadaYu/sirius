#include "sirius_queue.h"

#include "internal/internal_log.h"
#include "internal/internal_sys.h"
#include "sirius_cond.h"
#include "sirius_errno.h"
#include "sirius_mutex.h"

typedef struct {
  /* Queue elements. */
  size_t *elements;
  /* The number of elements in the queue. */
  size_t elem_nr;
  /* Queue capacity. */
  size_t capacity;

  /* Queue header. */
  size_t front;
  /* Queue tail. */
  size_t rear;

  /* Mechanism in the queue, refer `sirius_que_type_t`. */
  sirius_que_type_t type;

  /**
   * A mutex which is used when `type` is set to
   * `sirius_que_type_mtx`.
   */
  sirius_mutex_handle mtx;

  /* Condition variable for non-empty queue. */
  sirius_cond_handle cond_non_empty;
  /* Condition variable for non-full queue. */
  sirius_cond_handle cond_non_full;
} que_t;

#define que_lock(type, mtx)            \
  do {                                 \
    if (sirius_que_type_mtx == type) { \
      sirius_mutex_lock(&mtx);         \
    }                                  \
  } while (0)

#define que_unlock(type, mtx)          \
  do {                                 \
    if (sirius_que_type_mtx == type) { \
      sirius_mutex_unlock(&mtx);       \
    }                                  \
  } while (0)

sirius_api int sirius_que_alloc(
    sirius_que_t *cr, sirius_que_handle *handle) {
  if (unlikely(!cr || !handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

  int ret;
  que_t *q = (que_t *)calloc(1, sizeof(que_t));
  if (!q) {
    internal_error("calloc\n");
    return sirius_err_memory_alloc;
  }

  q->elem_nr = 0;
  q->capacity = cr->elem_nr;
  q->front = 0;
  q->rear = 0;
  q->type = cr->que_type;

  q->elements =
      (size_t *)calloc(q->capacity, sizeof(size_t));
  if (!q->elements) {
    internal_error("calloc\n");
    ret = sirius_err_memory_alloc;
    goto label_free1;
  }

  if (q->type == sirius_que_type_mtx) {
    ret = sirius_err_resource_alloc;
    if (sirius_mutex_init(&q->mtx, nullptr))
      goto label_free2;
    if (sirius_cond_init(&q->cond_non_empty, nullptr))
      goto label_free3;
    if (sirius_cond_init(&q->cond_non_full, nullptr))
      goto label_free4;
  }

  *handle = (sirius_que_handle)q;
  return 0;

label_free4:
  sirius_cond_destroy(&q->cond_non_empty);

label_free3:
  sirius_mutex_destroy(&q->mtx);

label_free2:
  free(q->elements);

label_free1:
  free(q);

  return ret;
}

sirius_api int sirius_que_free(sirius_que_handle handle) {
  if (unlikely(!handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

  que_t *q = (que_t *)handle;

  if (q->type == sirius_que_type_mtx) {
    sirius_cond_destroy(&q->cond_non_full);
    sirius_cond_destroy(&q->cond_non_empty);
    sirius_mutex_destroy(&q->mtx);
  }

  if (q->elements) {
    free(q->elements);
    q->elements = nullptr;
  }
  free(q);

  return 0;
}

#define q_wait(ret, cond, que, w_nr, milliseconds)      \
  {                                                     \
    ret = 0;                                            \
    if (w_nr == que->elem_nr) {                         \
      switch (milliseconds) {                           \
        case sirius_timeout_none:                       \
          ret = sirius_err_timeout;                     \
          break;                                        \
        case sirius_timeout_infinite:                   \
          while (!ret && w_nr == que->elem_nr) {        \
            ret = sirius_cond_wait(&cond, &que->mtx);   \
          }                                             \
          break;                                        \
        default:                                        \
          ret = sirius_cond_timedwait(&cond, &que->mtx, \
                                      milliseconds);    \
          ret = !ret && w_nr == que->elem_nr            \
                    ? sirius_err_timeout                \
                    : ret;                              \
          break;                                        \
      }                                                 \
    }                                                   \
  }

/**
 * @param[out] ret: Return code.
 * @param[in] type: Queue type, refer to
 *  `sirius_que_type_t`.
 * @param[in] mtx: Mutex handle.
 * @param[in] cond: Condition handle.
 */
#define q_tpl(ret, type, mtx, cond)                       \
  do {                                                    \
    ret = 0;                                              \
    switch (type) {                                       \
      case sirius_que_type_mtx:                           \
        sirius_mutex_lock(&mtx);                          \
        W if (!ret) { V sirius_cond_signal(&cond); }      \
        sirius_mutex_unlock(&mtx);                        \
        break;                                            \
      case sirius_que_type_no_mtx:                        \
        V break;                                          \
      default:                                            \
        internal_error("Invalid queue type: %d\n", type); \
        ret = sirius_err_args;                            \
        break;                                            \
    }                                                     \
  } while (0)

sirius_api int sirius_que_get(
    sirius_que_handle handle, size_t *ptr,
    unsigned long int milliseconds) {
  if (unlikely(!handle || !ptr)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

  que_t *q = (que_t *)handle;

  int ret;
#define V                                  \
  *ptr = q->elements[q->front];            \
  q->front = (q->front + 1) % q->capacity; \
  (q->elem_nr)--;

#define W \
  q_wait(ret, q->cond_non_empty, q, 0, milliseconds)

  q_tpl(ret, q->type, q->mtx, q->cond_non_full);
  return ret;

#undef W
#undef V
}

sirius_api int sirius_que_put(
    sirius_que_handle handle, size_t ptr,
    unsigned long int milliseconds) {
  if (unlikely(!handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

  que_t *q = (que_t *)handle;

  int ret;
#define V                                \
  q->elements[q->rear] = ptr;            \
  q->rear = (q->rear + 1) % q->capacity; \
  (q->elem_nr)++;

#define W                                       \
  q_wait(ret, q->cond_non_full, q, q->capacity, \
         milliseconds)

  q_tpl(ret, q->type, q->mtx, q->cond_non_empty);
  return ret;

#undef W
#undef V
}

sirius_api int sirius_que_reset(sirius_que_handle handle) {
  if (unlikely(!handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

  que_t *q = (que_t *)handle;

  que_lock(q->type, q->mtx);

  q->front = 0;
  q->rear = 0;
  q->elem_nr = 0;

  que_unlock(q->type, q->mtx);

  return 0;
}

sirius_api int sirius_que_cache_num(
    sirius_que_handle handle, size_t *num) {
  *num = 0;

  if (unlikely(!handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

  que_t *q = (que_t *)handle;

  que_lock(q->type, q->mtx);

  *num = q->elem_nr;

  que_unlock(q->type, q->mtx);

  return 0;
}
