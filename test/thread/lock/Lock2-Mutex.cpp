#include <sirius/thread/mutex.h>

#include "internal/utils.h"

static sirius_mutex_t g_mutex = SIRIUS_MUTEX_INITIALIZER;

int main() {
  auto init = Utils::Init();

  sirius_infosp("--- Static initialization test begins ---\n");

  UTILS_ASSERT(!sirius_mutex_lock(&g_mutex));
  UTILS_ASSERT(EBUSY == sirius_mutex_trylock(&g_mutex));
  UTILS_ASSERT(EBUSY == sirius_mutex_trylock(&g_mutex));
  UTILS_ASSERT(!sirius_mutex_unlock(&g_mutex));
  UTILS_ASSERT(!sirius_mutex_destroy(&g_mutex));

  sirius_infosp("--- Static initialization test ended ---\n\n");

  return 0;
}
