#ifndef SIRIUS_THREAD_H
#define SIRIUS_THREAD_H

#include "sirius/internal/macro.h"
#include "sirius/internal/thread.h"

/**
 * @brief Get the thread id, the result is of type `uint64_t`.
 */
#define sirius_thread_id (sirius_internal_thread_id())

typedef struct sirius_thread_s *sirius_thread_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief On POSIX system, under the `SCHED_OTHER` policy, the priority is
 * always 0, corresponding to `none` here.
 */
#define sirius_thread_priority_none (0)

/**
 * @brief Minimum thread priority.
 */
#define sirius_thread_priority_min (1)

/**
 * @brief Maximum thread priority.
 */
#define sirius_thread_priority_max (99)

typedef enum {
  sirius_thread_detach_none = -1,

  /**
   * @brief The thread can be synchronized using the `sirius_thread_join`
   * function.
   */
  sirius_thread_joinable = 0,

  /**
   * @brief The thread cannot be synchronized using the `sirius_thread_join`
   * function.
   */
  sirius_thread_detached = 1,
} sirius_thread_detach_state_t;

typedef enum {
  sirius_thread_sched_none = -1,

#if defined(_WIN32) || defined(_WIN64)
  sirius_thread_sched_other = sirius_thread_sched_none,
  sirius_thread_sched_fifo = sirius_thread_sched_none,
  sirius_thread_sched_rr = sirius_thread_sched_none,
#else
  /**
   * @brief Not real-time.
   */
  sirius_thread_sched_other = 0,

  /**
   * @brief Real-time, rotational method, system permissions may be required.
   */
  sirius_thread_sched_fifo = 1,

  /**
   * @brief Real-time, first-in, first-out, system permissions may be required.
   */
  sirius_thread_sched_rr = 2,
#endif
} sirius_thread_sched_policy_t;

typedef struct {
  /**
   * @brief Thread rotation policy.
   *
   * @note
   * ------------------------------------------
   * --- Only takes effect on POSIX system. ---
   * ------------------------------------------
   */
  sirius_thread_sched_policy_t sched_policy;

  /**
   * @brief Thread priority.
   *
   * @note This parameters usually range from `sirius_thread_priority_min` (1)
   * to `sirius_thread_priority_max` (99), and it only affects the thread
   * priority, not the process priority.
   *
   * - (1) On POSIX system, like `pthread`, this parameter needs to be used in
   * conjunction with the `sched_policy` parameter. When parameter
   * `sched_policy` is set to `sirius_thread_sched_fifo` or
   * `sirius_thread_sched_rr`, this parameter ranges from 1 to 99; when
   * parameter `sched_policy` is configured to `sirius_thread_sched_other`, this
   * parameter has a value of `sirius_thread_priority_none` (0) always.
   *
   * - (2) On Windows MSVC system, this parameter is valid regardless of the
   * `sched_policy` parameter. any configuration between
   * `sirius_thread_priority_min` and `sirius_thread_priority_max` is converted
   * to a specific priority. In fact, the priority of threads on Windows is also
   * related to the process. The thread priority is configured on the interface
   * according to the current process priority. For details about rules of
   * priorit on Windows, see the official Windows documentation.
   * @ref
   * en:
   * - (2.1)
   * https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadpriority
   *
   * - (2.2)
   * https://learn.microsoft.com/en-us/windows/win32/procthread/scheduling-priorities
   *
   * cn:
   * - (2.3)
   * https://learn.microsoft.com/zh-cn/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadpriority
   *
   * - (2.4)
   * https://learn.microsoft.com/zh-cn/windows/win32/procthread/scheduling-priorities
   */
  int priority;
} sirius_thread_sched_param_t;

typedef enum {
  sirius_thread_inherit_none = -1,

#if defined(_WIN32) || defined(_WIN64)
  sirius_thread_inherit_sched = sirius_thread_inherit_none,
  sirius_thread_explicit_sched = sirius_thread_inherit_none,
#else
  /**
   * @brief Inherits the scheduling policy and scheduling parameters of the
   * caller thread.
   */
  sirius_thread_inherit_sched = 0,

  /**
   * @brief Explicitly specifies the scheduling policy and scheduling parameters
   * for the thread.
   */
  sirius_thread_explicit_sched = 1,
#endif
} sirius_thread_inherit_t;

typedef enum {
  sirius_thread_scope_none = -1,

#if defined(_WIN32) || defined(_WIN64)
  sirius_thread_scope_system = sirius_thread_scope_none,
  sirius_thread_scope_process = sirius_thread_scope_none,
#else
  /**
   * @brief Compete with all threads in the system for the CPU time.
   */
  sirius_thread_scope_system = 0,

  /**
   * @brief Only compete with the thread in current process for the CPU time,
   * system permissions may be required.
   */
  sirius_thread_scope_process = 1,
#endif
} sirius_thread_scope_t;

typedef struct {
  /**
   * @brief Detach state.
   */
  sirius_thread_detach_state_t detach_state;

  /**
   * @note
   * ------------------------------------------
   * --- Only takes effect on POSIX system. ---
   * ------------------------------------------
   */
  sirius_thread_inherit_t inherit_sched;

  /**
   * @note
   * ------------------------------------------
   * --- Only takes effect on POSIX system. ---
   * ------------------------------------------
   */
  sirius_thread_scope_t scope;

  /**
   * @brief Specifies the starting address of the thread stack.
   *
   * @note
   * ------------------------------------------
   * --- Only takes effect on POSIX system. ---
   * ------------------------------------------
   */
  void *stackaddr;

  /**
   * @brief Alert buffer size at the end of the thread stack.
   *
   * @note
   * ------------------------------------------
   * --- Only takes effect on POSIX system. ---
   * ------------------------------------------
   */
  size_t guardsize;

  sirius_thread_sched_param_t sched_param;

  /**
   * @brief The size of the thread stack.
   */
  size_t stacksize;
} sirius_thread_attr_t;

/**
 * @brief Create a thread.
 *
 * @param[out] thread Thread handle.
 * @param[in] attr Thread attributes.
 * @param[in] start_routine Start routine of the thread. According to the POSIX
 * standard, the type of a routine function should be `void *`.
 * @param[in] arg Parameters of routine.
 *
 * @return 0 on success, or an `errno` value on failure.
 *
 * @example
 * void *foo(void *arg) {
 *   (void)arg;
 *   return NULL;
 * }
 *
 * int main() {
 *   sirius_thread_t thread;
 *   sirius_thread_create(&thread, NULL, foo, NULL);
 *   sirius_thread_join(thread, NULL);
 *   return 0;
 * }
 */
sirius_api int sirius_thread_create(sirius_thread_t *thread,
                                    const sirius_thread_attr_t *attr,
                                    void *(*start_routine)(void *), void *arg);

/**
 * @brief Exit the thread.
 *
 * @param[out] retval A pointer to any data.
 *
 * This parameter can be `nullptr`, if it is needed, then this parameter needs
 * to point to a non-temporary memory address.
 */
sirius_api void sirius_thread_exit(void *retval)
#if defined(_WIN32) || defined(_WIN64)
  SIRIUS_INTERNAL_THROW_SPEC
#endif
  ;

/**
 * @brief Reclaim the resources of the thread.
 *
 * @param[in] thread Thread handle.
 * @param[out] retval Data returned by the thread.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_thread_join(sirius_thread_t thread, void **retval);

/**
 * @brief Detach the thread.
 *
 * @param[in] thread Thread handle.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_thread_detach(sirius_thread_t thread);

/**
 * @brief Terminate the thread.
 * Equivalent to the `pthread_cancel` function on POSIX system;
 * Equivalent to the `TerminateThread` function on Windows MSVC environment.
 *
 * @param[in] thread Thread handle.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_thread_cancel(sirius_thread_t thread);

/**
 * @brief Get the thread handle.
 * Equivalent to the `pthread_self` function on POSIX system;
 * Equivalent to the `GetCurrentThread` function on Windows MSVC environment.
 *
 * @return The thread handle.
 */
sirius_api sirius_thread_t sirius_thread_self();

/**
 * @brief Get the maximum priority of the specified thread handle.
 *
 * @param[in] thread Thread handle.
 * @param[out] priority Priority.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_thread_get_priority_max(sirius_thread_t thread,
                                              int *priority);

/**
 * @brief Get the minimum priority of the specified thread handle.
 *
 * @param[in] thread Thread handle.
 * @param[out] priority Priority.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_thread_get_priority_min(sirius_thread_t thread,
                                              int *priority);

/**
 * @brief Set the thread attributes.
 *
 * @param[in] thread Thread handle.
 * @param[in] param Parameters of the thread attributes.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int
sirius_thread_setschedparam(sirius_thread_t thread,
                            const sirius_thread_sched_param_t *param);

/**
 * @brief Get the thread attributes.
 *
 * @param[in] thread Thread handle.
 * @param[in] param Parameters of the thread attributes.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_thread_getschedparam(sirius_thread_t thread,
                                           sirius_thread_sched_param_t *param);

#ifdef __cplusplus
}
#endif

#endif // SIRIUS_THREAD_H
