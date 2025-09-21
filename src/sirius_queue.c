#include "sirius_queue.h"

#include "internal/decls.h"
#include "internal/log.h"
#include "sirius_cond.h"
#include "sirius_errno.h"
#include "sirius_mutex.h"

typedef struct {
  /**
   * @brief Queue elements.
   */
  size_t *elements;
  size_t elem_nr;

  /**
   * @note It must be a power of 2.
   */
  size_t capacity;
  /**
   * @brief Used for bit operations, it is equal to `capacity - 1`.
   *
   * @note This parameter is used for speed optimization calculation.
   */
  size_t capacity_mask;

  size_t front, rear;

  /**
   * @brief Mechanism in the queue.
   *
   * @see `sirius_que_type_t`.
   */
  sirius_que_type_t type;

  /**
   * @brief A mutex which is used when `type` is set to `sirius_que_type_mtx`.
   */
  sirius_mutex_handle mtx;

  sirius_cond_handle cond_non_empty, cond_non_full;
} que_s;

#define que_lock(type, mtx) \
  do { \
    if (sirius_que_type_mtx == type) { \
      sirius_mutex_lock(&mtx); \
    } \
  } while (0)

#define que_unlock(type, mtx) \
  do { \
    if (sirius_que_type_mtx == type) { \
      sirius_mutex_unlock(&mtx); \
    } \
  } while (0)

static inline size_t next_power_of_2(size_t n) {
  if (n == 0)
    return 2;
  n--;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
#if __SIZEOF_SIZE_T__ == 8 // 64-bit
  n |= n >> 32;
#endif
  n++;
  return n;
}

static inline bool is_power_of_2(size_t n) {
  return (n > 0) && ((n & (n - 1)) == 0);
}

sirius_api int sirius_que_alloc(sirius_que_t *cr, sirius_que_handle *handle) {
  if (unlikely(!cr || !handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

  int ret;
  que_s *q = (que_s *)calloc(1, sizeof(que_s));
  if (!q) {
    internal_error("calloc\n");
    return sirius_err_memory_alloc;
  }

  size_t requested_capacity = cr->elem_nr;
  if (!is_power_of_2(requested_capacity)) {
    q->capacity = next_power_of_2(requested_capacity);
    internal_debg("Queue capacity adjusted from `%zu` to `%zu`\n",
                  requested_capacity, q->capacity);
  } else {
    q->capacity = requested_capacity;
  }

  if (q->capacity == 0)
    q->capacity = 2;

  q->capacity_mask = q->capacity - 1;
  q->elem_nr = 0;
  q->front = 0;
  q->rear = 0;
  q->type = cr->que_type;

  q->elements = (size_t *)calloc(q->capacity, sizeof(size_t));
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

  que_s *q = (que_s *)handle;

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

#define q_wait(ret, cond, que, wait_nr, milliseconds) \
  { \
    ret = 0; \
    if (wait_nr == que->elem_nr) { \
      switch (milliseconds) { \
      case sirius_timeout_none: \
        ret = sirius_err_timeout; \
        break; \
      case sirius_timeout_infinite: \
        while (!ret && wait_nr == que->elem_nr) { \
          ret = sirius_cond_wait(&cond, &que->mtx); \
        } \
        break; \
      default: \
        ret = sirius_cond_timedwait(&cond, &que->mtx, milliseconds); \
        ret = !ret && wait_nr == que->elem_nr ? sirius_err_timeout : ret; \
        break; \
      } \
    } \
  }

/**
 * @param[out] ret Return code.
 * @param[in] type Queue type, refer to `sirius_que_type_t`.
 * @param[in] mtx Mutex handle.
 * @param[in] cond Condition handle.
 */
#define q_tpl(ret, type, mtx, cond) \
  do { \
    ret = 0; \
    switch (type) { \
    case sirius_que_type_mtx: \
      sirius_mutex_lock(&mtx); \
      W_B if (!ret) { \
        C sirius_cond_signal(&cond); \
      } \
      sirius_mutex_unlock(&mtx); \
      break; \
    case sirius_que_type_no_mtx: \
      W_NB C break; \
    default: \
      internal_error("Invalid queue type: %d\n", type); \
      ret = sirius_err_args; \
      break; \
    } \
  } while (0)

sirius_api int sirius_que_get(sirius_que_handle handle, size_t *ptr,
                              uint64_t milliseconds) {
  if (unlikely(!handle || !ptr)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

  que_s *q = (que_s *)handle;

  int ret;
#define C \
  *ptr = q->elements[q->front]; \
  q->front = (q->front + 1) & q->capacity_mask; \
  q->elem_nr--;
#define W_NB \
  if (q->elem_nr == 0) { \
    return sirius_err_resource_alloc; \
  }
#define W_B q_wait(ret, q->cond_non_empty, q, 0, milliseconds)

  q_tpl(ret, q->type, q->mtx, q->cond_non_full);
  return ret;

#undef W_B
#undef W_NB
#undef C
}

sirius_api int sirius_que_put(sirius_que_handle handle, size_t ptr,
                              uint64_t milliseconds) {
  if (unlikely(!handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

  que_s *q = (que_s *)handle;

  int ret;
#define C \
  q->elements[q->rear] = ptr; \
  q->rear = (q->rear + 1) & q->capacity_mask; \
  q->elem_nr++;
#define W_NB \
  if (q->elem_nr == q->capacity) { \
    return sirius_err_cache_overflow; \
  }
#define W_B q_wait(ret, q->cond_non_full, q, q->capacity, milliseconds)

  q_tpl(ret, q->type, q->mtx, q->cond_non_empty);
  return ret;

#undef W_B
#undef W_NB
#undef C
}

sirius_api int sirius_que_reset(sirius_que_handle handle) {
  if (unlikely(!handle)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

  que_s *q = (que_s *)handle;

  que_lock(q->type, q->mtx);

  q->front = 0;
  q->rear = 0;
  q->elem_nr = 0;

  que_unlock(q->type, q->mtx);

  return 0;
}

sirius_api int sirius_que_cache_num(sirius_que_handle handle, size_t *num) {
  if (unlikely(!handle || !num)) {
    internal_error("Null pointer\n");
    return sirius_err_entry;
  }

  *num = 0;

  que_s *q = (que_s *)handle;

  que_lock(q->type, q->mtx);

  *num = q->elem_nr;

  que_unlock(q->type, q->mtx);

  return 0;
}
