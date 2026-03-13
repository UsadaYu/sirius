#include "utils/decls.h"
#pragma once

namespace sirius {
namespace utils {
namespace time {
inline uint64_t get_process_cputime_ns() noexcept {
#if defined(_WIN32) || defined(_WIN64)
  FILETIME ft_kernel {}, ft_user {};
  if (!GetProcessTimes(GetCurrentProcess(), nullptr, nullptr, &ft_kernel,
                       &ft_user)) {
    return 0;
  }
  auto to_u64 = [](const FILETIME &ft) -> uint64_t {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
  };
  uint64_t kernel100ns = to_u64(ft_kernel);
  uint64_t user100ns = to_u64(ft_user);
  uint64_t total100ns = kernel100ns + user100ns;
  return total100ns * 100ULL;
#else
  struct timespec ts {};
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) != 0)
    return 0;
  return (static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL +
          static_cast<uint64_t>(ts.tv_nsec));
#endif
}

inline uint64_t get_monotonic_steady_ms() {
  auto now = std::chrono::steady_clock::now();
  auto duration = now.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(duration)
    .count();
}
} // namespace time
} // namespace utils
} // namespace sirius
