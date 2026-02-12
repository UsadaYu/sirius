#pragma once

#include "utils/decls.h"

// --- SIRIUS_ENV_EXE_DAEMON_PATH ---
#undef SIRIUS_ENV_EXE_DAEMON_PATH

/**
 * @brief The path of the daemon executables.
 */
#define SIRIUS_ENV_EXE_DAEMON_PATH "SIRIUS_ENV_EXE_DAEMON_PATH"

#ifdef __cplusplus
namespace Utils {
namespace Env {
static inline std::string get_env(const char *name) {
  if (!name)
    return "";

#  if defined(_MSC_VER)
  char *buf = nullptr;
  size_t sz = 0;
  if (_dupenv_s(&buf, &sz, name) == 0 && buf != nullptr) {
    std::string var(buf);
    free(buf);
    return var;
  }
#  else
  const char *var = std::getenv(name);
  if (var)
    return std::string(var);
#  endif

  return "";
}
} // namespace Env
} // namespace Utils
#endif
