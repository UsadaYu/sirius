#pragma once

#include "sirius/foundation/thread.h"
#include "sirius/inner/macro.h"

/**
 * @brief Get the thread id, the result is of type `uint64_t`.
 */
#define SS_THREAD_ID (ss_thread_id())

typedef struct ss_thread_s *ss_thread_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief On POSIX system, under the `SCHED_OTHER` policy, the priority is
 * always 0, corresponding to `none` here.
 */
#define SS_THREAD_PRIORITY_NONE (0)

/**
 * @brief Minimum thread priority.
 */
#define SS_THREAD_PRIORITY_MIN (1)

/**
 * @brief Maximum thread priority.
 */
#define SS_THREAD_PRIORITY_MAX (99)

enum SsThreadCreate {
  /**
   * @brief The thread can be synchronized using the `ss_thread_join` function.
   */
  kSsThreadCreateJoinable = 0,

  /**
   * @brief The thread cannot be synchronized using the `ss_thread_join`
   * function.
   */
  kSsThreadCreateDetached = 1,
};

enum SsThreadSched {
  /**
   * @brief Not real-time.
   */
  kSsThreadSchedOther = 0,

  /**
   * @brief Real-time, rotational method, system permissions may be required.
   */
  kSsThreadSchedFifo = 1,

  /**
   * @brief Real-time, first-in, first-out, system permissions may be required.
   */
  kSsThreadSchedRR = 2,
};

typedef struct {
  /**
   * @brief Thread scheduling policy.
   *
   * @note
   * ------------------------------------------
   * --- Only takes effect on POSIX system. ---
   * ------------------------------------------
   */
  enum SsThreadSched sched_policy;

  /**
   * @brief Thread priority.
   *
   * @note This parameters usually range from `SS_THREAD_PRIORITY_MIN` (1)
   * to `SS_THREAD_PRIORITY_MAX` (99), and it only affects the thread
   * priority, not the process priority.
   *
   * - (1) On POSIX system, like `pthread`, this parameter needs to be used in
   * conjunction with the `sched_policy` parameter. When parameter
   * `sched_policy` is set to `kSsThreadSchedFifo` or
   * `kSsThreadSchedRR`, this parameter ranges from 1 to 99; when
   * parameter `sched_policy` is configured to `kSsThreadSchedOther`, this
   * parameter has a value of `SS_THREAD_PRIORITY_NONE` (0) always.
   *
   * - (2) On Windows MSVC system, this parameter is valid regardless of the
   * `sched_policy` parameter. any configuration between
   * `SS_THREAD_PRIORITY_MIN` and `SS_THREAD_PRIORITY_MAX` is converted
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
} ss_thread_sched_args_t;

enum SsThreadInherit {
  /**
   * @brief Inherits the scheduling policy and scheduling parameters of the
   * caller thread.
   */
  kSsThreadInheritSched = 0,

  /**
   * @brief Explicitly specifies the scheduling policy and scheduling parameters
   * for the thread.
   */
  kSsThreadExplicitSched = 1,
};

enum SsThreadScope {
  /**
   * @brief Compete with all threads in the system for the CPU time.
   */
  kSsThreadScopeSystem = 0,

  /**
   * @brief Only compete with the thread in current process for the CPU time,
   * system permissions may be required.
   */
  kSsThreadScopeProcess = 1,
};

typedef struct {
  /**
   * @brief Detach state.
   */
  enum SsThreadCreate detach_state;

  /**
   * @note
   * ------------------------------------------
   * --- Only takes effect on POSIX system. ---
   * ------------------------------------------
   */
  enum SsThreadInherit inherit_sched;

  /**
   * @note
   * ------------------------------------------
   * --- Only takes effect on POSIX system. ---
   * ------------------------------------------
   */
  enum SsThreadScope scope;

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

  ss_thread_sched_args_t sched_param;

  /**
   * @brief The size of the thread stack.
   */
  size_t stacksize;
} ss_thread_attr_t;

/**
 * @brief Create a thread.
 *
 * @param[out] thread Thread handle.
 * @param[in] attr Thread attributes.
 * @param[in] start_routine Start routine of the thread. According to the POSIX
 * standard, the type of a routine function should be `void *`.
 * @param[in] arg Parameters of routine.
 *
 * @note Just like `pthread_create` on POSIX, after this function call, there is
 * no guarantee that the thread function (start_routine) will definitely be
 * running.
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
 *   ss_thread_t thread;
 *   ss_thread_create(&thread, NULL, foo, NULL);
 *   ss_thread_join(thread, NULL);
 *
 *   return 0;
 * }
 */
SIRIUS_API int ss_thread_create(ss_thread_t *thread,
                                const ss_thread_attr_t *attr,
                                void *(*start_routine)(void *), void *arg);

/**
 * @brief Exit the thread.
 *
 * @param[out] retval A pointer to any data.
 *
 * This parameter can be `nullptr`, if it is needed, then this parameter needs
 * to point to a non-temporary memory address.
 */
#if defined(_WIN32) || defined(_WIN64)
SIRIUS_API void ss_thread_exit(void *retval) _SS_INNER_THROW_SPEC;
#else
SIRIUS_API void ss_thread_exit(void *retval);
#endif

/**
 * @brief Reclaim the resources of the thread.
 *
 * @param[in] thread Thread handle.
 * @param[out] retval Data returned by the thread.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
SIRIUS_API int ss_thread_join(ss_thread_t thread, void **retval);

/**
 * @brief Detach the thread.
 *
 * @param[in] thread Thread handle.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
SIRIUS_API int ss_thread_detach(ss_thread_t thread);

/**
 * @brief Terminate the thread.
 * Equivalent to the `pthread_cancel` function on POSIX system;
 * Equivalent to the `TerminateThread` function on Windows MSVC environment.
 *
 * @param[in] thread Thread handle.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
SIRIUS_API int ss_thread_cancel(ss_thread_t thread);

/**
 * @brief Get the thread handle.
 * Equivalent to the `pthread_self` function on POSIX system;
 * Equivalent to the `GetCurrentThread` function on Windows MSVC environment.
 *
 * @return The thread handle.
 */
SIRIUS_API ss_thread_t ss_thread_self();

/**
 * @brief Get the maximum priority of the specified thread handle.
 *
 * @param[in] thread Thread handle.
 * @param[out] priority Priority.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
SIRIUS_API int ss_thread_get_priority_max(ss_thread_t thread, int *priority);

/**
 * @brief Get the minimum priority of the specified thread handle.
 *
 * @param[in] thread Thread handle.
 * @param[out] priority Priority.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
SIRIUS_API int ss_thread_get_priority_min(ss_thread_t thread, int *priority);

/**
 * @brief Set the thread attributes.
 *
 * @param[in] thread Thread handle.
 * @param[in] param Parameters of the thread attributes.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
SIRIUS_API int ss_thread_setschedparam(ss_thread_t thread,
                                       const ss_thread_sched_args_t *param);

/**
 * @brief Get the thread attributes.
 *
 * @param[in] thread Thread handle.
 * @param[in] param Parameters of the thread attributes.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
SIRIUS_API int ss_thread_getschedparam(ss_thread_t thread,
                                       ss_thread_sched_args_t *param);

#ifdef __cplusplus
}
#endif
