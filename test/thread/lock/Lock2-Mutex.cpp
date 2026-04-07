#include <sirius/thread/mutex.h>

#include "inner/utils.h"

namespace {
inline ss_mutex_t g_mutex = SS_MUTEX_INITIALIZER;

inline int main_impl() {
  ss_log_infosp("--- Static initialization test begins ---\n");

  UTILS_ASSERT(!ss_mutex_lock(&g_mutex));
  UTILS_ASSERT(EBUSY == ss_mutex_trylock(&g_mutex));
  UTILS_ASSERT(EBUSY == ss_mutex_trylock(&g_mutex));
  UTILS_ASSERT(!ss_mutex_unlock(&g_mutex));
  UTILS_ASSERT(!ss_mutex_destroy(&g_mutex));

  ss_log_infosp("--- Static initialization test ended ---\n\n");

  return 0;
}
} // namespace

int main() {
  auto init = utils::Init();

  try {
    return main_impl();
  } catch (const std::exception &e) {
    ss_log_error("%s\n", e.what());
    return -1;
  } catch (...) {
    ss_log_error("`exception`: unknow\n");
    return -1;
  }
}
