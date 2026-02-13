#pragma once

#include "utils/decls.h"

namespace Utils {
namespace Process {
static inline auto pid() {
#if defined(_WIN32) || defined(_WIN64)
  static auto pid = GetCurrentProcessId();
#else
  static auto pid = getpid();
#endif

  return pid;
}
} // namespace Process
} // namespace Utils
