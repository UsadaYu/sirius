#include <sirius/sirius_thread.h>
#include <sirius/sirius_time.h>

#include "internal/utils.h"

static constexpr int NB_THREADS = 6;

static const char *g_arg_string = "T~T 1234567890 T~T";
static const char *g_retval = "abcdefghijklmnopqrstuvwxyz";

struct Arg {
  const char *string;
  int index;
};

/**
 * @note Setting the rotation mode and thread priority often requires certain
 * permissions.
 */
#define PERMISSION (0)

static void *thread_join(void *arg) {
  auto *ctx = (Arg *)arg;

  if (ctx->string == g_arg_string) {
    sirius_infosp(
      "Sub-thread (index: %d). The `argument` was successfully verified\n",
      ctx->index);
  } else {
    std::string es;
    es = std::format(log_red
                     "\n"
                     "  Sub-thread (TID: %llu)\n"
                     "  Fail to verifiy the `argument`:\n"
                     "    Actual string:   {}\n"
                     "    Expected string: {}\n" log_color_none,
                     sirius_thread_id, ctx->string, g_arg_string);
    throw std::runtime_error(es);
  }

  sirius_thread_exit((void *)g_retval);

  return nullptr;
}

static std::atomic<bool> g_thread_detach_exit_flag = false;
static void *thread_detach(void *arg) {
  (void)arg;

  while (!g_thread_detach_exit_flag.load()) {
    sirius_usleep(200 * 1000);
  }

  return nullptr;
}

static void start_join() {
  int ret;
  std::string es;
  sirius_thread_attr_t attr {};
  sirius_thread_t threads[NB_THREADS];
  Arg args[NB_THREADS] {};
  void *stackaddr = nullptr;

  // POSIX
  attr.inherit_sched = sirius_thread_explicit_sched;
  attr.scope = sirius_thread_scope_system;
  attr.stackaddr = stackaddr;
  attr.guardsize = 4096;

  for (int i = 0; i < NB_THREADS; ++i) {
    args[i].string = g_arg_string;
    args[i].index = i;
    ret =
      sirius_thread_create(threads + i, &attr, thread_join, (void *)(args + i));
    if (ret) {
      es = std::format(log_red
                       "\n"
                       "  Main-Join (creating the index: %d)\n"
                       "  `sirius_thread_create` error: {}\n" log_color_none,
                       i, ret);
      throw std::runtime_error(es);
    }
  }

  const char *retval = nullptr;
  for (int i = 0; i < NB_THREADS; ++i) {
    ret = sirius_thread_join(threads[i], (void **)&retval);
    if (ret) {
      es = std::format(log_red
                       "\n"
                       "  Main-Join (joining the index: %d)\n"
                       "  `sirius_thread_join` error: {}\n" log_color_none,
                       i, ret);
      throw std::runtime_error(es);
    }

    if (retval == g_retval) {
      sirius_infosp(
        "Main-Join (joining the index: %d). The `retval` was successfully "
        "verified\n",
        i);
    } else {
      es = std::format(log_red
                       "\n"
                       "  Main-Join (joining the index: %d)\n"
                       "  Fail to verifiy the `retval`:\n"
                       "    Actual string:   {}\n"
                       "    Expected string: {}\n" log_color_none,
                       i, retval, g_retval);
      throw std::runtime_error(es);
    }
    retval = nullptr;
  }
}

static void start_detach_get_sched(sirius_thread_t thread) {
  int ret;
  std::string es;
  sirius_thread_sched_param_t sched_param {};

  ret = sirius_thread_getschedparam(thread, &sched_param);
  if (ret) {
    es =
      std::format(log_red
                  "\n"
                  "  Main-Detach\n"
                  "  `sirius_thread_getschedparam` error: {}\n" log_color_none,
                  ret);
    throw std::runtime_error(es);
  }
  sirius_infosp("--------------------------------\n");
  sirius_infosp(
    "- Main-Detach. Successfully get the sub thread scheduling policy\n");
  sirius_infosp("- Scheduling policy: %d; Priority: %d\n",
                sched_param.sched_policy, sched_param.priority);
  sirius_infosp("--------------------------------\n");
}

static void start_detach() {
  int ret;
  std::string es;
  sirius_thread_attr_t attr {};
  sirius_thread_t thread;

  attr.detach_state = sirius_thread_detached;

  // POSIX
#if PERMISSION
  attr.sched_param.sched_policy = sirius_thread_sched_fifo;
#endif

#if defined(_WIN32) || defined(_WIN64) || PERMISSION
  attr.sched_param.priority = 2;
  sirius_infosp("Main-Detach. Thread initial priority: %d\n",
                attr.sched_param.priority);
#endif

  ret = sirius_thread_create(&thread, &attr, thread_detach, nullptr);
  if (ret) {
    es = std::format(log_red
                     "\n"
                     "  Main-Detach\n"
                     "  `sirius_thread_create` error: {}\n" log_color_none,
                     ret);
    throw std::runtime_error(es);
  }

  start_detach_get_sched(thread);

  int priority;
  sirius_thread_get_priority_max(thread, &priority);
  sirius_infosp("Main-Detach. Gets the max priority of the sub thread: %d\n",
                priority);
  sirius_thread_get_priority_min(thread, &priority);
  sirius_infosp("Main-Detach. Gets the min priority of the sub thread: %d\n",
                priority);

#if defined(_WIN32) || defined(_WIN64) || PERMISSION
  sirius_thread_sched_param_t sched_param {};
  sched_param.sched_policy = sirius_thread_sched_other;
  sched_param.priority = 36;
  ret = sirius_thread_setschedparam(thread, &sched_param);
  if (ret) {
    es =
      std::format(log_red
                  "\n"
                  "  Main-Detach\n"
                  "  `sirius_thread_setschedparam` error: {}\n" log_color_none,
                  ret);
    throw std::runtime_error(es);
  } else {
    sirius_infosp("--------------------------------\n");
    sirius_infosp(
      "- Main-Detach. Successfully set the sub thread scheduling policy\n");
    sirius_infosp("- Scheduling policy: %d; Priority: %d\n",
                  sched_param.sched_policy, sched_param.priority);
    sirius_infosp("--------------------------------\n");
  }
#endif

  start_detach_get_sched(thread);

  g_thread_detach_exit_flag.store(true);

  sirius_warnsp("--------------------------------\n");
  sirius_warnsp("- Main-Detach. Try to join the detached thread\n");
  ret = sirius_thread_join(thread, nullptr);
  sirius_warnsp("sirius_thread_join: %d\n", ret);
  sirius_warnsp("--------------------------------\n");
}

int main() {
  auto init = Utils::Init();

  start_join();
  start_detach();

  return 0;
}
