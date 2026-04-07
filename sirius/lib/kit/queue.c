/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

#include "sirius/kit/queue.h"

#include "lib/foundation/log/log.h"
#include "sirius/thread/cond.h"
#include "sirius/thread/mutex.h"

struct ss_queue_t {
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
   * @see `enum SsQueueType`.
   */
  enum SsQueueType type;

  /**
   * @brief A mutex which is used when `type` is set to `kSsQueueTypeMutex`.
   */
  ss_mutex_t mutex;

  ss_cond_t cond_non_empty, cond_non_full;
};

static ss_force_inline void queue_mutex_lock(enum SsQueueType type,
                                             ss_mutex_t *mutex) {
  if (kSsQueueTypeMutex == type) {
    ss_mutex_lock(mutex);
  }
}

static ss_force_inline void queue_mutex_unlock(enum SsQueueType type,
                                               ss_mutex_t *mutex) {
  if (kSsQueueTypeMutex == type) {
    ss_mutex_unlock(mutex);
  }
}

static inline bool is_power_of_2(size_t n) {
  return n > 0 && (n & (n - 1)) == 0;
}

sirius_api int ss_queue_alloc(ss_queue_t **__restrict queue,
                              const ss_queue_args_t *__restrict args) {
  if (ss_unlikely(!queue || !args)) {
    ss_log_error("Null pointer\n");
    return EINVAL;
  }

  int ret;
  ss_queue_t *q = (ss_queue_t *)calloc(1, sizeof(ss_queue_t));
  if (!q) {
    const int errno_err = errno;
    ss_log_error("calloc\n");
    return errno_err;
  }

  size_t requested_capacity = args->elem_count;
  if (!is_power_of_2(requested_capacity)) {
    q->capacity = utils_next_power_of_2(requested_capacity);
    ss_log_debugsp("Queue capacity adjusted from `%zu` to `%zu`\n",
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
    ss_log_error("calloc\n");
    ret = errno_err;
    goto label_free1;
  }

  if (q->type == kSsQueueTypeMutex) {
    if ((ret = ss_mutex_init(&q->mutex, nullptr)) != 0)
      goto label_free2;
    if ((ret = ss_cond_init(&q->cond_non_empty, nullptr)) != 0)
      goto label_free3;
    if ((ret = ss_cond_init(&q->cond_non_full, nullptr)) != 0)
      goto label_free4;
  }

  *queue = q;
  return 0;

label_free4:
  ss_cond_destroy(&q->cond_non_empty);
label_free3:
  ss_mutex_destroy(&q->mutex);
label_free2:
  free(q->elements);
label_free1:
  free(q);
  return ret;
}

sirius_api int ss_queue_free(ss_queue_t *queue) {
  if (ss_unlikely(!queue)) {
    ss_log_error("Null pointer\n");
    return EINVAL;
  }

  if (queue->type == kSsQueueTypeMutex) {
    ss_cond_destroy(&queue->cond_non_full);
    ss_cond_destroy(&queue->cond_non_empty);
    ss_mutex_destroy(&queue->mutex);
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
      case kSsTimeoutNoWaiting: \
        ret = ETIMEDOUT; \
        break; \
      case kSsTimeoutInfinite: \
        while (!ret && wait_nr == que->elem_count) { \
          ret = ss_cond_wait(&cond, &que->mutex); \
        } \
        break; \
      default: \
        ret = ss_cond_timedwait(&cond, &que->mutex, milliseconds); \
        ret = !ret && wait_nr == que->elem_count ? ETIMEDOUT : ret; \
        break; \
      } \
    } \
  }

/**
 * @param[out] ret Return code.
 * @param[in] type Queue type, refer to `enum SsQueueType`.
 * @param[in] mutex Mutex handle.
 * @param[in] cond Condition handle.
 */
#define T_QUEUE(ret, type, mutex, cond) \
  do { \
    ret = 0; \
    switch (type) { \
    case kSsQueueTypeMutex: \
      ss_mutex_lock(&mutex); \
      G if (!ret) { \
        E ss_cond_signal(&cond); \
      } \
      ss_mutex_unlock(&mutex); \
      break; \
    case kSsQueueTypeNoMutex: \
      F E break; \
    default: \
      ss_log_error("Invalid argument. Queue type: %d\n", (int)type); \
      ret = EINVAL; \
      break; \
    } \
  } while (0)

sirius_api int ss_queue_get(ss_queue_t *queue, size_t *ptr,
                            uint64_t milliseconds) {
  if (ss_unlikely(!queue || !ptr)) {
    ss_log_error("Null pointer\n");
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

  T_QUEUE(ret, queue->type, queue->mutex, queue->cond_non_full);
  return ret;
#undef G
#undef F
#undef E
}

sirius_api int ss_queue_put(ss_queue_t *queue, size_t ptr,
                            uint64_t milliseconds) {
  if (ss_unlikely(!queue)) {
    ss_log_error("Null pointer\n");
    return EINVAL;
  }

  int ret;
#define E \
  queue->elements[queue->rear] = ptr; \
  queue->rear = (queue->rear + 1) & queue->capacity_mask; \
  ++queue->elem_count;
#define F \
  if (queue->elem_count == queue->capacity) { \
    return EAGAIN; \
  }
#define G \
  QUEUE_WAIT(ret, queue->cond_non_full, queue, queue->capacity, milliseconds)

  T_QUEUE(ret, queue->type, queue->mutex, queue->cond_non_empty);
  return ret;
#undef G
#undef F
#undef E
}

sirius_api int ss_queue_reset(ss_queue_t *queue) {
  if (ss_unlikely(!queue)) {
    ss_log_error("Null pointer\n");
    return EINVAL;
  }

  queue_mutex_lock(queue->type, &queue->mutex);
  queue->front = 0;
  queue->rear = 0;
  queue->elem_count = 0;
  queue_mutex_unlock(queue->type, &queue->mutex);

  return 0;
}

sirius_api int ss_queue_nb_cache(ss_queue_t *queue, size_t *num) {
  if (ss_unlikely(!queue || !num)) {
    ss_log_error("Null pointer\n");
    return EINVAL;
  }

  *num = 0;
  queue_mutex_lock(queue->type, &queue->mutex);
  *num = queue->elem_count;
  queue_mutex_unlock(queue->type, &queue->mutex);

  return 0;
}
