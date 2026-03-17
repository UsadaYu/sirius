#pragma once
/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

namespace sirius {
namespace utils {
namespace process {
inline int64_t pid() {
#if defined(_WIN32) || defined(_WIN64)
  static auto pid = GetCurrentProcessId();
  return static_cast<int64_t>(pid); // DWORD
#else
  /**
   * @note `static` is disabled here because when functions such as `fork`
   * create a process, the data segment will be copied.
   */
  return static_cast<int64_t>(getpid()); // pid_t
#endif
}
} // namespace process
} // namespace utils
} // namespace sirius
