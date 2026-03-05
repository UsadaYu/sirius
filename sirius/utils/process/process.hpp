#pragma once

#include "utils/decls.h"

namespace sirius {
namespace utils {
namespace process {
#if defined(_WIN32) || defined(_WIN64)
using t_pid = DWORD;
#else
using t_pid = pid_t;
#endif

inline t_pid pid() {
#if defined(_WIN32) || defined(_WIN64)
  static auto pid = GetCurrentProcessId();

  return static_cast<t_pid>(pid);
#else
  /**
   * @note `static` is disabled here because when functions such as `fork`
   * create a process, the data segment will be copied.
   */
  return static_cast<t_pid>(getpid());
#endif
}
} // namespace process
} // namespace utils
} // namespace sirius
