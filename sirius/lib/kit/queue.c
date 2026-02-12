#include "sirius/kit/queue.h"

#include "lib/foundation/log/log.h"
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
   * @see `enum SiriusQueueType`.
   */
  enum SiriusQueueType type;

  /**
   * @brief A mutex which is used when `type` is set to `kSiriusQueueTypeMutex`.
   */
  sirius_mutex_t mutex;

  sirius_cond_t cond_non_empty, cond_non_full;
};

static force_inline void queue_mutex_lock(enum SiriusQueueType type,
                                          sirius_mutex_t *mutex) {
  if (kSiriusQueueTypeMutex == type) {
    sirius_mutex_lock(mutex);
  }
}

static force_inline void queue_mutex_unlock(enum SiriusQueueType type,
                                            sirius_mutex_t *mutex) {
  if (kSiriusQueueTypeMutex == type) {
    sirius_mutex_unlock(mutex);
  }
}

static inline bool is_power_of_2(size_t n) {
  return (n > 0) && ((n & (n - 1)) == 0);
}

sirius_api int sirius_queue_alloc(sirius_queue_t **__restrict queue,
                                  const sirius_queue_args_t *__restrict args) {
  if (unlikely(!queue || !args)) {
    sirius_error("Null pointer\n");
    return EINVAL;
  }

  int ret;
  sirius_queue_t *q = (sirius_queue_t *)calloc(1, sizeof(sirius_queue_t));
  if (!q) {
    const int errno_err = errno;
    sirius_error("calloc\n");
    return errno_err;
  }

  size_t requested_capacity = args->elem_count;
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
  q->type = args->queue_type;

  q->elements = (size_t *)calloc(q->capacity, sizeof(size_t));
  if (!q->elements) {
    const int errno_err = errno;
    sirius_error("calloc\n");
    ret = errno_err;
    goto label_free1;
  }

  if (q->type == kSiriusQueueTypeMutex) {
    if ((ret = sirius_mutex_init(&q->mutex, nullptr)) != 0)
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
  sirius_mutex_destroy(&q->mutex);
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

  if (queue->type == kSiriusQueueTypeMutex) {
    sirius_cond_destroy(&queue->cond_non_full);
    sirius_cond_destroy(&queue->cond_non_empty);
    sirius_mutex_destroy(&queue->mutex);
  }

  if (queue->elements) {
    free(queue->elements);
    queue->elements = nullptr;
  }
  free(queue);

  return 0;
}

#define QUEUE_WAIT(ret, cond, que, wait_nr, milliseconds) \
  { \
    ret = 0; \
    if (wait_nr == que->elem_count) { \
      switch (milliseconds) { \
      case SIRIUS_TIMEOUT_NO_WAITING: \
        ret = ETIMEDOUT; \
        break; \
      case SIRIUS_TIMEOUT_INFINITE: \
        while (!ret && wait_nr == que->elem_count) { \
          ret = sirius_cond_wait(&cond, &que->mutex); \
        } \
        break; \
      default: \
        ret = sirius_cond_timedwait(&cond, &que->mutex, milliseconds); \
        ret = !ret && wait_nr == que->elem_count ? ETIMEDOUT : ret; \
        break; \
      } \
    } \
  }

/**
 * @param[out] ret Return code.
 * @param[in] type Queue type, refer to `enum SiriusQueueType`.
 * @param[in] mutex Mutex handle.
 * @param[in] cond Condition handle.
 */
#define QUEUE_T(ret, type, mutex, cond) \
  do { \
    ret = 0; \
    switch (type) { \
    case kSiriusQueueTypeMutex: \
      sirius_mutex_lock(&mutex); \
      G if (!ret) { \
        E sirius_cond_signal(&cond); \
      } \
      sirius_mutex_unlock(&mutex); \
      break; \
    case kSiriusQueueTypeNoMutex: \
      F E break; \
    default: \
      sirius_error("Invalid argument. Queue type: %d\n", (int)type); \
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
#define E \
  *ptr = queue->elements[queue->front]; \
  queue->front = (queue->front + 1) & queue->capacity_mask; \
  queue->elem_count--;
#define F \
  if (queue->elem_count == 0) { \
    return EAGAIN; \
  }
#define G QUEUE_WAIT(ret, queue->cond_non_empty, queue, 0, milliseconds)

  QUEUE_T(ret, queue->type, queue->mutex, queue->cond_non_full);

  return ret;

#undef G
#undef F
#undef E
}

sirius_api int sirius_queue_put(sirius_queue_t *queue, size_t ptr,
                                uint64_t milliseconds) {
  if (unlikely(!queue)) {
    sirius_error("Null pointer\n");
    return EINVAL;
  }

  int ret;
#define E \
  queue->elements[queue->rear] = ptr; \
  queue->rear = (queue->rear + 1) & queue->capacity_mask; \
  queue->elem_count++;
#define F \
  if (queue->elem_count == queue->capacity) { \
    return EAGAIN; \
  }
#define G \
  QUEUE_WAIT(ret, queue->cond_non_full, queue, queue->capacity, milliseconds)

  QUEUE_T(ret, queue->type, queue->mutex, queue->cond_non_empty);

  return ret;

#undef G
#undef F
#undef E
}

sirius_api int sirius_queue_reset(sirius_queue_t *queue) {
  if (unlikely(!queue)) {
    sirius_error("Null pointer\n");
    return EINVAL;
  }

  queue_mutex_lock(queue->type, &queue->mutex);

  queue->front = 0;
  queue->rear = 0;
  queue->elem_count = 0;

  queue_mutex_unlock(queue->type, &queue->mutex);

  return 0;
}

sirius_api int sirius_queue_nb_cache(sirius_queue_t *queue, size_t *num) {
  if (unlikely(!queue || !num)) {
    sirius_error("Null pointer\n");
    return EINVAL;
  }

  *num = 0;

  queue_mutex_lock(queue->type, &queue->mutex);

  *num = queue->elem_count;

  queue_mutex_unlock(queue->type, &queue->mutex);

  return 0;
}
