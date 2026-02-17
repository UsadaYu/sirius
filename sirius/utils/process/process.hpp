#pragma once

#include "utils/decls.h"

namespace Utils {
namespace Process {
static inline auto pid() {
#if defined(_WIN32) || defined(_WIN64)
  static auto pid = GetCurrentProcessId();

  return pid;
#else
  /**
   * @note `static` is disabled here because when functions such as `fork`
   * create a process, the data segment will be copied.
   */
  return getpid();
#endif
}
} // namespace Process
} // namespace Utils
