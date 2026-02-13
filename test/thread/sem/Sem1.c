#include <sirius/thread/sem.h>
#include <sirius/thread/thread.h>

#include "internal/utils.h"

static const int kIndexStart = 1;

static bool g_exit = false;
static char g_string[128];
static sirius_sem_t g_sem_sub;
static sirius_sem_t g_sem_main;

static inline const char *get_nspace(int index) {
  const char *nspace = nullptr;

  if (index < 10) {
    nspace = "   ";
  } else if (index < 100) {
    nspace = "  ";
  } else if (index < 1000) {
    nspace = " ";
  } else {
    sirius_error("Please reduce the index\n");
    nspace = "\n";
  }

  return nspace;
}

static void *foo(void *arg) {
  (void)arg;

  int index = kIndexStart;

  while (!g_exit) {
    UTILS_ASSERT(!sirius_sem_wait(&g_sem_sub));
    if (unlikely(g_exit))
      break;

    utils_dprintf(1, "foo(%d): %s%s|", index, g_string, get_nspace(index));
    if (index % 4 == 0)
      utils_dprintf(1, "\n");

    ++index;
    UTILS_ASSERT(!sirius_sem_post(&g_sem_main));
  }

  return nullptr;
}

int main() {
  utils_init();

  UTILS_ASSERT(!sirius_sem_init(&g_sem_main, 0, 0));
  UTILS_ASSERT(!sirius_sem_init(&g_sem_sub, 0, 0));

  sirius_thread_t thread;
  UTILS_ASSERT(!sirius_thread_create(&thread, nullptr, foo, nullptr));

  for (int i = kIndexStart; i < kIndexStart + 200; i++) {
    memset(g_string, 0, sizeof(g_string));
    snprintf(g_string, sizeof(g_string), "main(%d)%s", i, get_nspace(i));

    UTILS_ASSERT(!sirius_sem_post(&g_sem_sub));
    UTILS_ASSERT(!sirius_sem_wait(&g_sem_main));
  }

  g_exit = true;
  UTILS_ASSERT(!sirius_sem_post(&g_sem_sub));

  UTILS_ASSERT(!sirius_thread_join(thread, nullptr));
  UTILS_ASSERT(!sirius_sem_destroy(&g_sem_sub));
  UTILS_ASSERT(!sirius_sem_destroy(&g_sem_main));

  sirius_warnsp("--------------------------------\n");
  sirius_warnsp("- Try to destroy the sem multiple times\n");
  sirius_warnsp("- sirius_sem_destroy: %d\n", sirius_sem_destroy(&g_sem_sub));
  sirius_warnsp("- sirius_sem_destroy: %d\n", sirius_sem_destroy(&g_sem_sub));
  sirius_warnsp("- sirius_sem_destroy: %d\n", sirius_sem_destroy(&g_sem_main));
  sirius_warnsp("- sirius_sem_destroy: %d\n", sirius_sem_destroy(&g_sem_main));
  sirius_warnsp("--------------------------------\n");

  utils_deinit();

  return 0;
}
