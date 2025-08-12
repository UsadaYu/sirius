/**
 * @name sirius_thread.h
 *
 * @author UsadaYu
 *
 * @date
 *  Create: 2025-01-06
 *  Update: 2025-01-25
 *
 * @brief Thread.
 */

#ifndef __SIRIUS_THREAD_H__
#define __SIRIUS_THREAD_H__

#include "custom/custom_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the thread id, the result is of type
 *  `unsigned long long`.
 */
#define sirius_thread_id (_custom_thread_id())

/* Minimum thread priority. */
#define SIRIUS_THREAD_PRIORITY_MIN (0)

/* Maximum thread priority. */
#define SIRIUS_THREAD_PRIORITY_MAX (31)

#ifdef _WIN32
typedef HANDLE sirius_thread_handle;
#else
typedef pthread_t sirius_thread_handle;
#endif

typedef enum {
  /**
   * @brief The thread can be synchronized using the
   *  `sirius_thread_join` function.
   */
  sirius_thread_joinable = 0,

  /**
   * @brief The thread cannot be synchronized using the
   *  `sirius_thread_join` function.
   */
  sirius_thread_detached = 1,
} sirius_thread_detach_state_t;

typedef enum {
  /**
   * @brief Not real-time.
   */
  SIRIUS_THREAD_SCHED_OTHER = 0,

  /**
   * @brief Real-time, rotational method, system
   *  permissions may be required.
   */
  SIRIUS_THREAD_SCHED_FIFO = 1,

  /**
   * @brief Real-time, first-in, first-out, system
   *  permissions may be required.
   */
  SIRIUS_THREAD_SCHED_RR = 2,
} sirius_thread_sched_policy_t;

typedef struct {
#ifndef _WIN32
  /* Thread rotation policy. */
  sirius_thread_sched_policy_t sched_policy;
#endif

  /**
   * @brief Thread priority.
   *
   * @note This parameters usually range from
   *  `SIRIUS_THREAD_PRIORITY_MIN` to
   *  `SIRIUS_THREAD_PRIORITY_MAX`, and it only affects the
   *  thread priority, not the process priority.
   *
   *  In POSIX system, like pthread, this parameter needs
   *  to be used in conjunction with the `sched_policy`
   *  parameter.
   *  When parameter `sched_policy` is set to
   *  `SIRIUS_THREAD_SCHED_FIFO` or
   *  `SIRIUS_THREAD_SCHED_RR`, this parameter ranges from
   *  1 to 31, corresponding to the priority of pthread
   *  threads from 1 to 99;
   *  when parameter `sched_policy` is configured to
   *  `SIRIUS_THREAD_SCHED_OTHER`, this parameter has a
   *  value of 0.
   *
   *  In Windows MSVC system, this parameter is valid
   *  regardless of the `sched_policy` parameter. any
   *  configuration between `SIRIUS_THREAD_PRIORITY_MIN`
   *  and `SIRIUS_THREAD_PRIORITY_MAX` is converted to a
   *  specific priority.
   *  In fact, the priority of threads in Windows is also
   *  related to the process. the thread priority is
   *  configured on the interface according to the current
   *  process priority.
   *  For details about rules of priorit in Windows, see
   *  the official Windows documentation
   *  @ref en:
   *  https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadpriority
   *  https://learn.microsoft.com/en-us/windows/win32/procthread/scheduling-priorities
   *  @ref cn:
   *  https://learn.microsoft.com/zh-cn/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadpriority
   *  https://learn.microsoft.com/zh-cn/windows/win32/procthread/scheduling-priorities
   */
  int priority;
} sirius_thread_sched_param_t;

typedef enum {
  /**
   * @brief Inherits the scheduling policy and scheduling
   *  parameters of the caller thread.
   */
  SIRIUS_THREAD_INHERIT_SCHED = 0,

  /**
   * @brief Explicitly specifies the scheduling policy and
   *  scheduling parameters for the thread.
   */
  SIRIUS_THREAD_EXPLICIT_SCHED = 1,
} sirius_thread_inherit_t;

typedef enum {
  /**
   * @brief Compete with all threads in the system for the
   *  CPU time.
   */
  SIRIUS_THREAD_SCOPE_SYSTEM = 0,

  /**
   * @brief Only compete with the thread in current process
   *  for the CPU time, system permissions may be required.
   */
  SIRIUS_THREAD_SCOPE_PROCESS = 1,
} sirius_thread_scope_t;

typedef struct {
#ifndef _WIN32
  sirius_thread_detach_state_t detach_state;

  sirius_thread_inherit_t inherit_sched;

  sirius_thread_scope_t scope;

  /**
   * @brief Specifies the starting address of the thread
   *  stack.
   */
  void *stackaddr;

  /**
   * @brief Alert buffer size at the end of the thread
   *  stack.
   */
  size_t guardsize;
#endif

  sirius_thread_sched_param_t sched_param;

  /**
   * @brief The size of the thread stack.
   */
  size_t stacksize;
} sirius_thread_attr_t;

/**
 * @brief Create a thread.
 *  Equivalent to the `pthread_create` function in POSIX
 *  system;
 *  Equivalent to the `CreateThread` function in Windows
 *  MSVC environment.
 *
 * @param[out] handle: Thread handle.
 * @param[in] attr: Thread attributes.
 * @param[in] start_routine: Start routine of the thread.
 * @param[in] arg: Parameters of routine.
 *
 * @return 0 on success, error code otherwise.
 *
 * @note If the thread is created successfully, but
 *  attributes setting fails, the function will still
 * return success.
 */
int sirius_thread_create(sirius_thread_handle *handle,
                         const sirius_thread_attr_t *attr,
                         void *(*start_routine)(void *),
                         void *arg);

/**
 * @brief Reclaim the resources of the thread.
 *  Equivalent to the `pthread_join` function in POSIX
 *  system;
 *  Equivalent to the
 *  `WaitForSingleObject(handle, INFINITE)` function in
 *  Windows MSVC environment.
 *
 * @param[in] handle: Thread handle.
 * @param[out] retval: Data returned by the thread.
 *  this parameter is valid only in POSIX system, but not
 *  in Windows MSVC environment.
 *
 * @return 0 on success, error code otherwise.
 */
int sirius_thread_join(sirius_thread_handle handle,
                       void **retval);

#ifndef _WIN32
/**
 * @brief Detach the thread.
 *  Equivalent to the `pthread_detach` function in POSIX
 *  system.
 *
 * @param[in] handle: Thread handle.
 *
 * @return 0 on success, error code otherwise.
 */
int sirius_thread_detach(sirius_thread_handle handle);
#else
#define sirius_thread_detach(handle) (0)
#endif

/**
 * @brief Exit the thread.
 *  Equivalent to the `pthread_exit` function in POSIX
 *  system;
 *  Equivalent to the `ExitThread` function in Windows MSVC
 *  environment.
 *
 * @param[out] retval: In POSIX system, this parameter can
 *  be a pointer to any data; in Windows MSVC environment,
 *  this parameter can only be a `DWORD` type error code.
 *
 *  This parameter can be null, if it is needed, then:
 *  in POSIX system, this parameter needs to point to a
 *  non-temporary memory address;
 *  In Windows MSVC environment, this parameter can point
 *  to a temporary memory address.
 */
void sirius_thread_exit(
#ifdef _WIN32
    DWORD *
#else
    void *
#endif
        retval);

/**
 * @brief Terminate the thread.
 *  Equivalent to the `pthread_cancel` function in POSIX
 *  system;
 *  Equivalent to the `TerminateThread` function in Windows
 *  MSVC environment.
 *
 * @param[in] handle: Thread handle.
 *
 * @return 0 on success, error code otherwise.
 */
int sirius_thread_cancel(sirius_thread_handle handle);

/**
 * @brief Get the thread handle.
 *  Equivalent to the `pthread_self` function in POSIX
 *  system;
 *  Equivalent to the `GetCurrentThread` function in
 *  Windows MSVC environment.
 *
 * @return The thread handle.
 */
sirius_thread_handle sirius_thread_self();

/**
 * @brief Get the maximum thread priority.
 *
 * @param[in] handle: Thread handle.
 *  This parameter is valid only in POSIX system, but not
 *  in Windows MSVC environment.
 *
 * @return 0 on success, error code otherwise.
 */
int sirius_thread_sched_get_priority_max(
    sirius_thread_handle handle);

/**
 * @brief Get the minimum thread priority.
 *
 * @param[in] handle: Thread handle.
 *  This parameter is valid only in POSIX system, but not
 *  in Windows MSVC environment.
 *
 * @return 0 on success, error code otherwise.
 */
int sirius_thread_sched_get_priority_min(
    sirius_thread_handle handle);

/**
 * @brief Set the thread attributes.
 *
 * @param[in] handle: Thread handle.
 * @param[in] param: Parameters of the thread attributes.
 *
 * @return 0 on success, error code otherwise.
 */
int sirius_thread_setschedparam(
    sirius_thread_handle handle,
    const sirius_thread_sched_param_t *param);

/**
 * @brief Get the thread attributes.
 *
 * @param[in] handle: Thread handle.
 * @param[in] param: Parameters of the thread attributes.
 *
 * @return 0 on success, error code otherwise.
 */
int sirius_thread_getschedparam(
    sirius_thread_handle handle,
    sirius_thread_sched_param_t *param);

#ifdef __cplusplus
}
#endif

#endif  // __SIRIUS_THREAD_H__
