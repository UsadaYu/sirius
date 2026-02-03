#include "sirius/kit/queue.h"

#include "lib/utils/log/log.h"
#include "sirius/thread/cond.h"
#include "sirius/thread/mutex.h"

struct sirius_queue_t {
  /**
   * @brief Queue elements.
   */
  size_t *elements;
  size_t elem_count;

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
   * @see `sirius_queue_type_t`.
   */
  sirius_queue_type_t type;

  /**
   * @brief A mutex which is used when `type` is set to `sirius_queue_type_mtx`.
   */
  sirius_mutex_t mtx;

  sirius_cond_t cond_non_empty, cond_non_full;
};

#define que_lock(type, mtx) \
  do { \
    if (sirius_queue_type_mtx == type) { \
      sirius_mutex_lock(&mtx); \
    } \
  } while (0)

#define que_unlock(type, mtx) \
  do { \
    if (sirius_queue_type_mtx == type) { \
      sirius_mutex_unlock(&mtx); \
    } \
  } while (0)

static inline bool is_power_of_2(size_t n) {
  return (n > 0) && ((n & (n - 1)) == 0);
}

sirius_api int sirius_queue_alloc(sirius_queue_t **__restrict queue,
                                  const sirius_queue_arg_t *__restrict arg) {
  if (unlikely(!queue || !arg)) {
    sirius_error("Null pointer\n");
    return EINVAL;
  }

  int ret;
  sirius_queue_t *q = (sirius_queue_t *)calloc(1, sizeof(sirius_queue_t));
  if (!q) {
    sirius_error("calloc\n");
    return errno;
  }

  size_t requested_capacity = arg->elem_count;
  if (!is_power_of_2(requested_capacity)) {
    q->capacity = utils_next_power_of_2(requested_capacity);
    sirius_debugsp("Queue capacity adjusted from `%zu` to `%zu`\n",
                   requested_capacity, q->capacity);
  } else {
    q->capacity = requested_capacity;
  }

  if (q->capacity == 0)
    q->capacity = 2;

  q->capacity_mask = q->capacity - 1;
  q->elem_count = 0;
  q->front = 0;
  q->rear = 0;
  q->type = arg->queue_type;

  q->elements = (size_t *)calloc(q->capacity, sizeof(size_t));
  if (!q->elements) {
    sirius_error("calloc\n");
    ret = errno;
    goto label_free1;
  }

  if (q->type == sirius_queue_type_mtx) {
    if ((ret = sirius_mutex_init(&q->mtx, nullptr)) != 0)
      goto label_free2;
    if ((ret = sirius_cond_init(&q->cond_non_empty, nullptr)) != 0)
      goto label_free3;
    if ((ret = sirius_cond_init(&q->cond_non_full, nullptr)) != 0)
      goto label_free4;
  }

  *queue = q;
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

sirius_api int sirius_queue_free(sirius_queue_t *queue) {
  if (unlikely(!queue)) {
    sirius_error("Null pointer\n");
    return EINVAL;
  }

  if (queue->type == sirius_queue_type_mtx) {
    sirius_cond_destroy(&queue->cond_non_full);
    sirius_cond_destroy(&queue->cond_non_empty);
    sirius_mutex_destroy(&queue->mtx);
  }

  if (queue->elements) {
    free(queue->elements);
    queue->elements = nullptr;
  }
  free(queue);

  return 0;
}

#define q_wait(ret, cond, que, wait_nr, milliseconds) \
  { \
    ret = 0; \
    if (wait_nr == que->elem_count) { \
      switch (milliseconds) { \
      case sirius_timeout_none: \
        ret = ETIMEDOUT; \
        break; \
      case sirius_timeout_infinite: \
        while (!ret && wait_nr == que->elem_count) { \
          ret = sirius_cond_wait(&cond, &que->mtx); \
        } \
        break; \
      default: \
        ret = sirius_cond_timedwait(&cond, &que->mtx, milliseconds); \
        ret = !ret && wait_nr == que->elem_count ? ETIMEDOUT : ret; \
        break; \
      } \
    } \
  }

/**
 * @param[out] ret Return code.
 * @param[in] type Queue type, refer to `sirius_queue_type_t`.
 * @param[in] mtx Mutex handle.
 * @param[in] cond Condition handle.
 */
#define que_tpl(ret, type, mtx, cond) \
  do { \
    ret = 0; \
    switch (type) { \
    case sirius_queue_type_mtx: \
      sirius_mutex_lock(&mtx); \
      W_B if (!ret) { \
        C sirius_cond_signal(&cond); \
      } \
      sirius_mutex_unlock(&mtx); \
      break; \
    case sirius_queue_type_no_mtx: \
      W_NB C break; \
    default: \
      sirius_error("Invalid queue type: %d\n", type); \
      ret = EINVAL; \
      break; \
    } \
  } while (0)

sirius_api int sirius_queue_get(sirius_queue_t *queue, size_t *ptr,
                                uint64_t milliseconds) {
  if (unlikely(!queue || !ptr)) {
    sirius_error("Null pointer\n");
    return EINVAL;
  }

  int ret;
#define C \
  *ptr = queue->elements[queue->front]; \
  queue->front = (queue->front + 1) & queue->capacity_mask; \
  queue->elem_count--;
#define W_NB \
  if (queue->elem_count == 0) { \
    return EAGAIN; \
  }
#define W_B q_wait(ret, queue->cond_non_empty, queue, 0, milliseconds)

  que_tpl(ret, queue->type, queue->mtx, queue->cond_non_full);
  return ret;

#undef W_B
#undef W_NB
#undef C
}

sirius_api int sirius_queue_put(sirius_queue_t *queue, size_t ptr,
                                uint64_t milliseconds) {
  if (unlikely(!queue)) {
    sirius_error("Null pointer\n");
    return EINVAL;
  }

  int ret;
#define C \
  queue->elements[queue->rear] = ptr; \
  queue->rear = (queue->rear + 1) & queue->capacity_mask; \
  queue->elem_count++;
#define W_NB \
  if (queue->elem_count == queue->capacity) { \
    return EAGAIN; \
  }
#define W_B \
  q_wait(ret, queue->cond_non_full, queue, queue->capacity, milliseconds)

  que_tpl(ret, queue->type, queue->mtx, queue->cond_non_empty);
  return ret;

#undef W_B
#undef W_NB
#undef C
}

sirius_api int sirius_queue_reset(sirius_queue_t *queue) {
  if (unlikely(!queue)) {
    sirius_error("Null pointer\n");
    return EINVAL;
  }

  que_lock(queue->type, queue->mtx);

  queue->front = 0;
  queue->rear = 0;
  queue->elem_count = 0;

  que_unlock(queue->type, queue->mtx);

  return 0;
}

sirius_api int sirius_queue_cache_num(sirius_queue_t *queue, size_t *num) {
  if (unlikely(!queue || !num)) {
    sirius_error("Null pointer\n");
    return EINVAL;
  }

  *num = 0;

  que_lock(queue->type, queue->mtx);

  *num = queue->elem_count;

  que_unlock(queue->type, queue->mtx);

  return 0;
}
