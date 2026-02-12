#pragma once

#include "sirius/foundation/log.h"
#include "utils/namespace.hpp"

#define UTILS_LOG_KEY "utils_log"

namespace Utils {
namespace Log {
inline constexpr const char *kKey = UTILS_LOG_KEY;
inline constexpr const char *kCommon =
  " [Common " _SIRIUS_NAMESPACE " " UTILS_LOG_KEY "] ";
inline constexpr const char *kNative =
  " [Native " _SIRIUS_NAMESPACE " " UTILS_LOG_KEY "] ";
inline constexpr const char *kDaemon =
  " [Daemon " _SIRIUS_NAMESPACE " " UTILS_LOG_KEY "] ";
inline constexpr const char *kDaemonArgKey = UTILS_LOG_KEY "_daemon_arg";
inline constexpr const char *kMutexProcessKey = UTILS_LOG_KEY "_process";
inline constexpr const char *kMutexShmKey = UTILS_LOG_KEY "_shm";

inline constexpr size_t kLogBufferSize = 4096;
inline constexpr size_t kLogPathMax = 4096;
inline constexpr size_t kProcessMax = 128;
inline constexpr uint64_t kProcessFeedGuardMilliseconds = 2000;
inline constexpr uint64_t kProcessGuardTimeoutMilliseconds = 10000;
inline constexpr uint64_t kShmSlotResetTimeoutMilliseconds = 10000;
inline constexpr size_t kShmCapacity =
  Utils::next_power_of_2(_SIRIUS_LOG_SHM_CAPACITY);
} // namespace Log
} // namespace Utils

#undef UTILS_LOG_KEY
