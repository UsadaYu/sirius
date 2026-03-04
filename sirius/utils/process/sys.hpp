#pragma once

#include "utils/decls.h"

namespace Utils {
namespace Process {
enum class InitType {
  // --- Windows ---
  kWindows,

  // --- Posix --
  kSystemdOrInit,  // Standard release version, safe.
  kDockerTini,     // Container-specific init, safe.
  kUnreliableInit, // It might be sh or a business process, which is not secure.

  kUnknown,
};

#if defined(_WIN32) || defined(_WIN64)
inline InitType check_init_type() {
  return InitType::kWindows;
}
#else
inline InitType check_init_type() {
  char path[PATH_MAX];
  ssize_t len = readlink("/proc/1/exe", path, sizeof(path) - 1);

  if (len != -1) {
    path[len] = '\0';
    std::string exe_path(path);

    if (exe_path.contains("systemd") || exe_path.contains("sbin/init")) {
      return InitType::kSystemdOrInit;
    }

    if (exe_path.contains("tini") || exe_path.contains("docker-init") ||
        exe_path.contains("dumb-init")) {
      return InitType::kDockerTini;
    }
  }

  if (access("/.dockerenv", F_OK) == 0)
    return InitType::kUnreliableInit;

  return InitType::kUnknown;
}
#endif
} // namespace Process
} // namespace Utils
