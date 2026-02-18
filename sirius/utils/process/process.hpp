#pragma once

#include "utils/decls.h"

namespace Utils {
namespace Process {
#if defined(_WIN32) || defined(_WIN64)
using Pid = DWORD;
#else
using Pid = pid_t;
#endif

inline Pid pid() {
#if defined(_WIN32) || defined(_WIN64)
  static auto pid = GetCurrentProcessId();

  return static_cast<Pid>(pid);
#else
  /**
   * @note `static` is disabled here because when functions such as `fork`
   * create a process, the data segment will be copied.
   */
  return static_cast<Pid>(getpid());
#endif
}
} // namespace Process
} // namespace Utils
