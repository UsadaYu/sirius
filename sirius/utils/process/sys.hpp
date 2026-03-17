#pragma once
/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

namespace sirius {
namespace utils {
namespace process {
enum class SysInitType {
  kUnknown,

  // --- Windows ---
  kWindows,

  // --- Posix --
  kSystemdOrInit,  // Standard release version, safe.
  kDockerTini,     // Container-specific init, safe.
  kUnreliableInit, // It might be sh or a business process, which is not secure.
};

#if defined(_WIN32) || defined(_WIN64)
inline SysInitType sys_init_type() {
  return SysInitType::kWindows;
}
#else
inline SysInitType sys_init_type() {
  char path[PATH_MAX];
  ssize_t len = readlink("/proc/1/exe", path, sizeof(path) - 1);
  if (len != -1) {
    path[len] = '\0';
    std::string exe_path(path);
    if (exe_path.contains("systemd") || exe_path.contains("sbin/init")) {
      return SysInitType::kSystemdOrInit;
    }
    if (exe_path.contains("tini") || exe_path.contains("docker-init") ||
        exe_path.contains("dumb-init")) {
      return SysInitType::kDockerTini;
    }
  }

  if (access("/.dockerenv", F_OK) == 0)
    return SysInitType::kUnreliableInit;

  return SysInitType::kUnknown;
}
#endif
} // namespace process
} // namespace utils
} // namespace sirius
