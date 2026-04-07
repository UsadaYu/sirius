/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

#include "lib/foundation/structor.h"

#include <array>
#include <cassert>

#include "sirius/foundation/structor.h"
#include "utils/io.hpp"

/**
 * @note This variable only needs to be guaranteed to exist, not a public
 * variable.
 */
#ifdef __cplusplus
extern "C" {
#endif
int sirius_inner_foundation_link_anchor = 0;
#ifdef __cplusplus
}
#endif

namespace sirius {
namespace {
inline std::atomic<bool> g_has_been_destructed {false};
using destructor_list = std::vector<void (*)()>;
inline std::array<
  destructor_list,
  static_cast<size_t>(
    StructorDestructorPriority::kStructorDestructorPriorityEarliest) +
    1>
  g_destructor_buckets {};
inline std::mutex g_destructor_mutex {};
inline constexpr uint64_t kPriorityLatest = static_cast<uint64_t>(
  StructorDestructorPriority::kStructorDestructorPriorityLatest);
inline constexpr uint64_t kPriorityEarliest = static_cast<uint64_t>(
  StructorDestructorPriority::kStructorDestructorPriorityEarliest);
} // namespace
} // namespace sirius

using namespace sirius;

extern "C" void
structor_destructor_register(structor_destructor_register_t *cfg) {
  if (!cfg || !cfg->fn_destructor) {
#ifdef NDEBUG
    logln_error("Invalid argument. `cfg` or `cfg->fn_destructor`");
    return;
#else
    assert(false && "Invalid argument. `cfg` or `cfg->fn_destructor`");
#endif
  }

  uint64_t prio = cfg->priority;
  if (prio < kPriorityLatest) {
    prio = kPriorityLatest;
  }
  if (prio > kPriorityEarliest) {
    prio = kPriorityEarliest;
  }

  auto lock = std::lock_guard(g_destructor_mutex);
  g_destructor_buckets[prio].push_back(cfg->fn_destructor);
}

/**
 * @note
 */
extern "C" sirius_api void ss_global_destruct() {
  if (g_has_been_destructed.exchange(true, std::memory_order_seq_cst)) {
    return;
  }

  for (uint64_t i = kPriorityEarliest; i >= kPriorityLatest; --i) {
#if 0
    auto &bucket = g_destructor_buckets[i];
#else
    /**
     * @note This ensures the security of processing. However, if the destructor
     * is not called explicitly (called through `atexit` registration), it may
     * be reported as a memory leak by some tools.
     */
    destructor_list bucket {};
    {
      auto lock = std::lock_guard(g_destructor_mutex);
      bucket.swap(g_destructor_buckets[i]);
    }
#endif

    for (auto it = bucket.rbegin(); it != bucket.rend(); ++it) {
      if (*it) {
        (*it)();
      }
    }
    bucket.clear();
    bucket.shrink_to_fit();
  }
}

static void destructor(void) {
  ss_global_destruct();
}

#ifndef _MSC_VER
__attribute__((constructor))
#endif
static void constructor(void) {
  if (!constructor_foundation_log())
    goto label_exit;

  std::atexit(destructor);
  return;

label_exit:
#if defined(_WIN32) || defined(_WIN64)
  exit(EXIT_FAILURE);
#else
  _exit(EXIT_FAILURE);
#endif
}

#if defined(_MSC_VER)
/**
 * @note Ensure to execute before the static construction of C/C++.
 */

#  pragma section(".CRT$XCS", read)

__declspec(allocate(".CRT$XCS")) void(
  WINAPI *sirius_inner_foundation_constructor_ptr)(void) = constructor;
#endif
