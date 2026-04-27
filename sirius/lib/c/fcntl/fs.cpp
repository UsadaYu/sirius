/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

#include "utils/fs.hpp"

using namespace sirius;

extern "C" SIRIUS_API int ss_fs_open(const char *path, int flags, int mode) {
  return utils::fs::fs_open_impl(path, flags, mode);
}

extern "C" SIRIUS_API int ss_fs_close(int fd) {
  return utils::fs::fs_close_impl(fd);
}
