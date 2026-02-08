#pragma once

#include "sirius/foundation/thread.h"
#include "sirius/internal/macro.h"

/**
 * @brief Get the thread id, the result is of type `uint64_t`.
 */
#define SIRIUS_THREAD_ID (sirius_thread_id())

typedef struct sirius_thread_s *sirius_thread_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief On POSIX system, under the `SCHED_OTHER` policy, the priority is
 * always 0, corresponding to `none` here.
 */
#define SIRIUS_THREAD_PRIORITY_NONE (0)

/**
 * @brief Minimum thread priority.
 */
#define SIRIUS_THREAD_PRIORITY_MIN (1)

/**
 * @brief Maximum thread priority.
 */
#define SIRIUS_THREAD_PRIORITY_MAX (99)

enum SiriusThreadCreate {
  kSiriusThreadCreateNone = -1,

  /**
   * @brief The thread can be synchronized using the `sirius_thread_join`
   * function.
   */
  kSiriusThreadCreateJoinable = 0,

  /**
   * @brief The thread cannot be synchronized using the `sirius_thread_join`
   * function.
   */
  kSiriusThreadCreateDetached = 1,
};

enum SiriusThreadSched {
  kSiriusThreadSchedNone = -1,

#if defined(_WIN32) || defined(_WIN64)
  kSiriusThreadSchedOther = kSiriusThreadSchedNone,
  kSiriusThreadSchedFifo = kSiriusThreadSchedNone,
  kSiriusThreadSchedRR = kSiriusThreadSchedNone,
#else
  /**
   * @brief Not real-time.
   */
  kSiriusThreadSchedOther = 0,

  /**
   * @brief Real-time, rotational method, system permissions may be required.
   */
  kSiriusThreadSchedFifo = 1,

  /**
   * @brief Real-time, first-in, first-out, system permissions may be required.
   */
  kSiriusThreadSchedRR = 2,
#endif
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
  enum SiriusThreadSched sched_policy;

  /**
   * @brief Thread priority.
   *
   * @note This parameters usually range from `SIRIUS_THREAD_PRIORITY_MIN` (1)
   * to `SIRIUS_THREAD_PRIORITY_MAX` (99), and it only affects the thread
   * priority, not the process priority.
   *
   * - (1) On POSIX system, like `pthread`, this parameter needs to be used in
   * conjunction with the `sched_policy` parameter. When parameter
   * `sched_policy` is set to `kSiriusThreadSchedFifo` or
   * `kSiriusThreadSchedRR`, this parameter ranges from 1 to 99; when
   * parameter `sched_policy` is configured to `kSiriusThreadSchedOther`, this
   * parameter has a value of `SIRIUS_THREAD_PRIORITY_NONE` (0) always.
   *
   * - (2) On Windows MSVC system, this parameter is valid regardless of the
   * `sched_policy` parameter. any configuration between
   * `SIRIUS_THREAD_PRIORITY_MIN` and `SIRIUS_THREAD_PRIORITY_MAX` is converted
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
} sirius_thread_sched_args_t;

enum SiriusThreadInherit {
  kSiriusThreadInheritNone = -1,

#if defined(_WIN32) || defined(_WIN64)
  kSiriusThreadInheritSched = kSiriusThreadInheritNone,
  kSiriusThreadExplicitSched = kSiriusThreadInheritNone,
#else
  /**
   * @brief Inherits the scheduling policy and scheduling parameters of the
   * caller thread.
   */
  kSiriusThreadInheritSched = 0,

  /**
   * @brief Explicitly specifies the scheduling policy and scheduling parameters
   * for the thread.
   */
  kSiriusThreadExplicitSched = 1,
#endif
};

enum SiriusThreadScope {
  kSiriusThreadScopeNone = -1,

#if defined(_WIN32) || defined(_WIN64)
  kSiriusThreadScopeSystem = kSiriusThreadScopeNone,
  kSiriusThreadScopeProcess = kSiriusThreadScopeNone,
#else
  /**
   * @brief Compete with all threads in the system for the CPU time.
   */
  kSiriusThreadScopeSystem = 0,

  /**
   * @brief Only compete with the thread in current process for the CPU time,
   * system permissions may be required.
   */
  kSiriusThreadScopeProcess = 1,
#endif
};

typedef struct {
  /**
   * @brief Detach state.
   */
  enum SiriusThreadCreate detach_state;

  /**
   * @note
   * ------------------------------------------
   * --- Only takes effect on POSIX system. ---
   * ------------------------------------------
   */
  enum SiriusThreadInherit inherit_sched;

  /**
   * @note
   * ------------------------------------------
   * --- Only takes effect on POSIX system. ---
   * ------------------------------------------
   */
  enum SiriusThreadScope scope;

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

  sirius_thread_sched_args_t sched_param;

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
 *
 *   return NULL;
 * }
 *
 * int main() {
 *   sirius_thread_t thread;
 *   sirius_thread_create(&thread, NULL, foo, NULL);
 *   sirius_thread_join(thread, NULL);
 *
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
  _sirius_throw_spec
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
                            const sirius_thread_sched_args_t *param);

/**
 * @brief Get the thread attributes.
 *
 * @param[in] thread Thread handle.
 * @param[in] param Parameters of the thread attributes.
 *
 * @return 0 on success, or an `errno` value on failure.
 */
sirius_api int sirius_thread_getschedparam(sirius_thread_t thread,
                                           sirius_thread_sched_args_t *param);

#ifdef __cplusplus
}
#endif
