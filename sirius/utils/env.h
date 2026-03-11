#pragma once

#include "utils/decls.h"

#ifdef __cplusplus
namespace sirius {
namespace utils {
namespace env {
inline std::string get_env(const char *name) {
  if (!name)
    return {};

#  if defined(_MSC_VER)
  char *buf = nullptr;
  size_t sz = 0;
  if (_dupenv_s(&buf, &sz, name) == 0 && buf != nullptr) {
    std::string var(buf);
    free(buf);
    return var;
  }
#  else
  if (const char *var = std::getenv(name); var != nullptr)
    return std::string(var);
#  endif

  return {};
}
} // namespace env
} // namespace utils
} // namespace sirius
#endif
