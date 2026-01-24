#include <sirius/thread/mutex.h>

#include "internal/utils.h"

static sirius_mutex_t g_mutex = SIRIUS_MUTEX_INITIALIZER;

int main() {
  auto init = Utils::Init();

  sirius_infosp("--- Static initialization test begins ---\n");

  utils_assert(!sirius_mutex_lock(&g_mutex));
  utils_assert(EBUSY == sirius_mutex_trylock(&g_mutex));
  utils_assert(EBUSY == sirius_mutex_trylock(&g_mutex));
  utils_assert(!sirius_mutex_unlock(&g_mutex));
  utils_assert(!sirius_mutex_destroy(&g_mutex));

  sirius_infosp("--- Static initialization test ended ---\n\n");

  return 0;
}
