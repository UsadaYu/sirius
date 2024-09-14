/**
 * @name sirius_queue.h
 * 
 * @author 胡益华
 * 
 * @date 2024-09-12
 * 
 * @brief 队列接口
 */

#ifndef __SIRIUS_QUEUE_H__
#define __SIRIUS_QUEUE_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SIRIUS_QUE_TIMEOUT_NONE
/* timeout, no waiting, return immediately */
#define SIRIUS_QUE_TIMEOUT_NONE (0)
#else
#undef SIRIUS_QUE_TIMEOUT_NONE
#define SIRIUS_QUE_TIMEOUT_NONE (0)
#endif

#ifndef SIRIUS_QUE_TIMEOUT_INFINITE
/* timeout, infinite wait */
#define SIRIUS_QUE_TIMEOUT_INFINITE (~0U)
#else
#undef SIRIUS_QUE_TIMEOUT_INFINITE
#define SIRIUS_QUE_TIMEOUT_INFINITE (~0U)
#endif

typedef void* sirius_que_handle;

typedef enum {
    /* queue with mutex, default */
    SIRIUS_QUE_TYPE_MTX = 0,

    /* queue without mutex */
    SIRIUS_QUE_TYPE_NO_MTX = 1,

    SIRIUS_QUE_TYPE_MAX,
} sirius_que_type_t;

typedef struct {
    /* number of queue members */
    unsigned short elem_nr;

    /* mechanism in the queue, refer `sirius_que_type_t` */
    sirius_que_type_t que_type;
} sirius_que_cr_t;

/**
 * @brief create a queue,
 *  the resulting handle must be deleted using `sirius_que_del`
 * 
 * @param[in] p_cr: queue creation parameters
 * @param[out] p_handle: queue handle
 * 
 * @return 0 on success, error code otherwise
 */
int
sirius_que_cr(sirius_que_cr_t *p_cr,
    sirius_que_handle *p_handle);

/**
 * @brief delete the queue
 * 
 * @param[in] p_handle: queue handle
 * 
 * @return 0 on success, error code otherwise
 */
int
sirius_que_del(sirius_que_handle handle);

/**
 * @brief get an element from the queue
 * 
 * @param[in] p_handle: queue handle
 * @param[out] p_value: an obtained queue element
 * @param[in] timeout: timeout period, unit: ms.
 *  takes effect only when the queue with mutex.
 *  setting the value to `SIRIUS_QUE_TIMEOUT_NONE` means no wait,
 *  and setting it to `SIRIUS_QUE_TIMEOUT_INFINITE` means infinite wait.
 * 
 * @return 0 on success, error code otherwise
 */
int
sirius_que_get(sirius_que_handle handle,
    size_t *p_value, unsigned int timeout);

/**
 * @brief put an element into the queue
 * 
 * @param[in] p_handle: queue handle
 * @param[out] value: the element which will be added to the queue
 * @param[in] timeout: timeout period, unit: ms.
 *  takes effect only when the queue with mutex.
 *  setting the value to `SIRIUS_QUE_TIMEOUT_NONE` means no wait,
 *  and setting it to `SIRIUS_QUE_TIMEOUT_INFINITE` means infinite wait.
 * 
 * @return 0 on success, `SIRIUS_ERR_TIMEOUT` on timeout,
 *  error code otherwise
 */
int
sirius_que_put(sirius_que_handle handle,
    size_t p_value, unsigned int timeout);

/**
 * @brief reset the queue, empty the cached elements
 * 
 * @param[in] p_handle: queue handle
 * 
 * @return 0 on success, `SIRIUS_ERR_TIMEOUT` on timeout, 
 *  error code otherwise
 */
int
sirius_que_reset(sirius_que_handle handle);

#ifdef __cplusplus
}
#endif

#endif // __SIRIUS_QUEUE_H__
