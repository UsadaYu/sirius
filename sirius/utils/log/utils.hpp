#pragma once
/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

#include "sirius/foundation/log.h"
#include "utils/ns.hpp"

#define UTILS_LOG_KEY "utils_log"

namespace sirius {
namespace utils {
namespace log {
inline constexpr std::string_view kShmKey = UTILS_LOG_KEY "_shm";
inline constexpr std::string_view kMutexProcessKey =
  UTILS_LOG_KEY "_mutex_process";
inline constexpr std::string_view kMutexShmKey = UTILS_LOG_KEY "_mutex_shm";
inline constexpr std::string_view kMutexCrashKey = UTILS_LOG_KEY "_mutex_crash";
sirius_static_assert(kMutexProcessKey != kMutexShmKey &&
                       kMutexProcessKey != kMutexCrashKey &&
                       kMutexShmKey != kMutexCrashKey,
                     "The mutex name must be unique");

inline constexpr size_t kLogBufferSize = 4096;
inline constexpr size_t kLogPathMax = 4096;
inline constexpr size_t kProcessMax = 128;
inline constexpr size_t kProcessNbDaemon = 1;
inline constexpr uint64_t kProcessFeedGuardMs = 2000;
inline constexpr uint64_t kProcessGuardTimeoutMs = 8000;
sirius_static_assert(kProcessFeedGuardMs <= kProcessGuardTimeoutMs);
inline constexpr uint64_t kShmSlotResetTimeoutMs = 5000;
inline constexpr size_t kShmCapacity =
  utils::next_power_of_2(_SIRIUS_LOG_SHM_CAPACITY);

enum class MasterType : int {
  kNone = 0,
  kDaemon = 1,
  kNative = 2,
};
} // namespace log
} // namespace utils
} // namespace sirius

#undef UTILS_LOG_KEY
