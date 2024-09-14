#include "sirius_queue.h"
#include "sirius_log.h"
#include "sirius_errno.h"

#include "./internal/sirius_internal_sys.h"

typedef struct {
    /* queue elements */
    size_t *elements;
    /* the number of elements in the queue */
    unsigned short elem_nr;
    /* queue capacity */
    unsigned short capacity;

    /* queue header */
    unsigned short front;
    /* queue tail */
    unsigned short rear;

    /* mechanism in the queue, refer `sirius_que_type_t` */
    sirius_que_type_t type;

    /**
     * a mutex which is used when
     * `type` is set to `SIRIUS_QUE_TYPE_MTX`
     */
    pthread_mutex_t mutex;

    /* condition variable for non-empty queue */
    pthread_cond_t cond_non_empty;
    /* condition variable for non-full queue */
    pthread_cond_t cond_non_full;
} i_queue_t;

int
sirius_que_cr(sirius_que_cr_t *p_cr,
    sirius_que_handle *p_handle)
{
    if (!(p_cr) || !(p_handle)) {
        SIRIUS_ERROR("null pointer\n");
        return SIRIUS_ERR_NULL_POINTER;
    }

    i_queue_t *q = (i_queue_t *)calloc(1, sizeof(i_queue_t));
    if (!(q)) {
        SIRIUS_ERROR("calloc\n");
        return SIRIUS_ERR_MEMORY_ALLOC;
    }

    q->elem_nr = 0;
    q->capacity = p_cr->elem_nr;
    q->front = 0;
    q->rear = 0;
    q->type = p_cr->que_type;

    q->elements = (size_t *)calloc(q->capacity, sizeof(size_t));
    if (!q->elements) {
        free(q);
        SIRIUS_ERROR("calloc\n");
        return SIRIUS_ERR_MEMORY_ALLOC;
    }

    if (q->type == SIRIUS_QUE_TYPE_MTX) {
        pthread_mutex_init(&(q->mutex), NULL);
        pthread_cond_init(&(q->cond_non_empty), NULL);
        pthread_cond_init(&(q->cond_non_full), NULL);
    }

    *p_handle = (sirius_que_handle)q;
    return SIRIUS_OK;
}

int
sirius_que_del(sirius_que_handle handle)
{
    if (!(handle)) {
        SIRIUS_ERROR("null pointer\n");
        return SIRIUS_ERR_INVALID_ENTRY;
    }

    i_queue_t *q = (i_queue_t *)handle;
    
    if (q->type == SIRIUS_QUE_TYPE_MTX) {
        pthread_cond_destroy(&(q->cond_non_full));
        pthread_cond_destroy(&(q->cond_non_empty));
        pthread_mutex_destroy(&(q->mutex));
    }

    free(q->elements);
    q->elements = NULL;
    free(q);

    return SIRIUS_OK;
}

static inline int
i_que_wait(i_queue_t *q, unsigned int timeout,
    pthread_cond_t *p_cond, unsigned short wait_nr)
{
    if (q->elem_nr != wait_nr) {
        return SIRIUS_OK;
    }

    struct timespec ts;
    if (timeout != SIRIUS_QUE_TIMEOUT_NONE) {
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout / 1000;
        ts.tv_nsec += (timeout % 1000) * 1000000;
    }

    int ret = SIRIUS_OK;
    switch (timeout) {
        case SIRIUS_QUE_TIMEOUT_NONE:
            ret = SIRIUS_ERR;
            break;
        case SIRIUS_QUE_TIMEOUT_INFINITE:
            pthread_cond_wait(
                p_cond, &(q->mutex));
        default:
            if (pthread_cond_timedwait(
                    p_cond, &(q->mutex), &ts) == ETIMEDOUT) {
                SIRIUS_DEBG("timeout\n");
                ret = SIRIUS_ERR_TIMEOUT;
                break;
            }
    }

    return ret;
}

/**
 * @param ret: return code
 * @param type: queue type, refer to `sirius_que_type_t`
 * @param que_mtx: queue mutex
 * @param cond_sig: semaphore notified
 */
#define I_QUE_VAR(ret, type, que_mtx, cond_sig) \
do { \
    ret = SIRIUS_OK; \
    switch (type) { \
        case SIRIUS_QUE_TYPE_MTX: \
            pthread_mutex_lock(&(que_mtx)); \
            ret = W; \
            if (ret == SIRIUS_OK) { \
                V \
                pthread_cond_signal(&(cond_sig)); \
            } \
            pthread_mutex_unlock(&(que_mtx)); \
            break; \
        case SIRIUS_QUE_TYPE_NO_MTX: \
            V \
            break; \
        default: \
            SIRIUS_WARN("queue type: %d\n", type); \
            ret = SIRIUS_ERR_INVALID_PARAMETER; \
            break; \
    } \
} while(0)

int
sirius_que_get(sirius_que_handle handle,
    size_t *p_value, unsigned int timeout)
{
    if (!(handle) || !(p_value)) {
        SIRIUS_ERROR("null pointer\n");
        return SIRIUS_ERR_INVALID_ENTRY;
    }

    i_queue_t *q = (i_queue_t *)handle;

    int ret;
#define V \
    *p_value = q->elements[q->front]; \
    q->front = (q->front + 1) % q->capacity; \
    (q->elem_nr)--;
#define W \
    i_que_wait(q, timeout, &(q->cond_non_empty), 0)

    I_QUE_VAR(ret, q->type, q->mutex, q->cond_non_full);
#undef W
#undef V
    return ret;
}

int
sirius_que_put(sirius_que_handle handle,
    size_t p_value, unsigned int timeout)
{
    if (!(handle)) {
        SIRIUS_ERROR("null pointer\n");
        return SIRIUS_ERR_INVALID_ENTRY;
    }

    i_queue_t *q = (i_queue_t *)handle;

    int ret;
#define V \
    q->elements[q->rear] = p_value; \
    q->rear = (q->rear + 1) % q->capacity; \
    (q->elem_nr)++;
#define W \
    i_que_wait(q, timeout, &(q->cond_non_full), q->capacity)

    I_QUE_VAR(ret, q->type, q->mutex, q->cond_non_empty);
#undef W
#undef V
    return ret;
}

int
sirius_que_reset(sirius_que_handle handle)
{
    if (!(handle)) {
        SIRIUS_ERROR("null pointer\n");
        return SIRIUS_ERR_INVALID_ENTRY;
    }

    i_queue_t *q = (i_queue_t *)handle;
    
    if (q->type == SIRIUS_QUE_TYPE_MTX) {
        pthread_mutex_lock(&(q->mutex));
    }

    q->front = 0;
    q->rear = 0;
    q->elem_nr = 0;

    if (q->type == SIRIUS_QUE_TYPE_MTX) {
        pthread_mutex_unlock(&(q->mutex));
    }

    return SIRIUS_OK;
}
