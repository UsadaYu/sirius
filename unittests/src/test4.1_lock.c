#include "sirius/sirius_mutex.h"
#include "sirius/sirius_spinlock.h"
#include "sirius/sirius_thread.h"
#include "test.h"

/**
 * @note When using the spinlock, the number of threads should not be too large.
 */
#define CNT (4)

static uint64_t g_count;
static bool g_exit_flag;
static bool g_exit[CNT];
static char g_buf[40];

#define G_CLEAR() \
  do { \
    g_count = 0; \
    g_exit_flag = false; \
    memset(g_exit, 0, sizeof(g_exit)); \
    memset(g_buf, 0, sizeof(g_buf)); \
  } while (0)

static sirius_mutex_t g_mutex;
static sirius_spinlock_t g_spin;

typedef enum {
  lock_type_mutex = 0,
  lock_type_spin = 1,
} lock_type_e;

#define ll_tpl(type) \
  do { \
    switch (type) { \
    case lock_type_mutex: \
      ll_mutex(); \
      break; \
    case lock_type_spin: \
      ll_spin(); \
      break; \
    default: \
      t_dprintf(2, "[error: invalid argument] [lock type: %d]\n", type); \
      break; \
    } \
  } while (0)

static void ll_init(lock_type_e type) {
#define ll_mutex() sirius_mutex_init(&g_mutex, nullptr)
#define ll_spin() sirius_spin_init(&g_spin, SIRIUS_THREAD_PROCESS_PRIVATE)
  ll_tpl(type);
#undef ll_spin
#undef ll_mutex
}

static void ll_destroy(lock_type_e type) {
#define ll_mutex() sirius_mutex_destroy(&g_mutex)
#define ll_spin() sirius_spin_destroy(&g_spin)
  ll_tpl(type);
#undef ll_spin
#undef ll_mutex
}

static force_inline void ll_lock(lock_type_e type) {
#define ll_mutex() sirius_mutex_lock(&g_mutex)
#define ll_spin() sirius_spin_lock(&g_spin)
  ll_tpl(type);
#undef ll_spin
#undef ll_mutex
}

static force_inline void ll_unlock(lock_type_e type) {
#define ll_mutex() sirius_mutex_unlock(&g_mutex)
#define ll_spin() sirius_spin_unlock(&g_spin)
  ll_tpl(type);
#undef ll_spin
#undef ll_mutex
}

void *foo(void *args) {
  int type = *((lock_type_e *)args);

  for (int i = 0; i < 1024 * 10; i++) {
    ll_lock(type);

    memset(g_buf, 0, sizeof(g_buf));
    snprintf(g_buf, sizeof(g_buf),
             "[count: %" PRIu64 "] [thread id: %" PRIu64 "]", ++g_count,
             sirius_thread_id);

    ll_unlock(type);
  }

  ll_lock(type);
  for (int i = 0; i < CNT; i++) {
    if (likely(g_exit[i])) {
      continue;
    }

    g_exit[i] = true;
    if (i == CNT - 1) {
      g_exit_flag = true;
    }
    break;
  }
  ll_unlock(type);

  sirius_thread_exit(nullptr);
  return nullptr;
}

int test(lock_type_e type) {
  G_CLEAR();

  int t_cnt = 0;
  sirius_thread_t thread_handle[CNT];

  ll_init(type);

  int ret = 0;
  for (int i = 0; i < CNT; i++) {
    if (sirius_thread_create(&thread_handle[i], nullptr, foo, (void *)&type)) {
      ret = -1;
      goto label_exit;
    }

    t_cnt++;
  }

  for (int i = 0; !g_exit_flag; i++) {
    ll_lock(type);
    t_dprintf(1, "[idx: %d] %s\n", i, g_buf);
    ll_unlock(type);
  }

label_exit:
  for (int j = 0; j < t_cnt; j++) {
    sirius_thread_join(thread_handle[j], nullptr);
  }

  ll_destroy(type);

  return ret;
}

int main() {
#define ARR_CNT (2)
  int ret[ARR_CNT] = {0};

  ret[0] = test(lock_type_mutex);

  sirius_usleep(1000 * 1000 * 2);
  ret[1] = test(lock_type_spin);

  t_dprintf(1, "\n--------------------------------\n");
  for (int i = 0; i < ARR_CNT; i++) {
    if (ret[i]) {
      t_dprintf(2, "[test index: %d] fail\n", i);
    } else {
      t_dprintf(1, "[test index: %d] succeed\n", i);
    }
  }
  t_dprintf(1, "--------------------------------\n");

  return 0;
#undef ARR_CNT
}
