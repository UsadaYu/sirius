#pragma once

#ifdef UTILS_LOG_LOG_HPP_

#  include "utils/namespace.hpp"

#  define LOG_KEY "utils_log"
#  define LOG_COMMON " [Common " sirius_namespace " " LOG_KEY "] "
#  define LOG_NATIVE " [Native " sirius_namespace " " LOG_KEY "] "
#  define LOG_DAEMON " [Daemon " sirius_namespace " " LOG_KEY "] "
#  define LOG_DAEMON_ARG_KEY LOG_KEY "_daemon_arg"
#  define LOG_MUTEX_PROCESS LOG_KEY "_process"
#  define LOG_MUTEX_SHM LOG_KEY "_shm"
#  define LOG_PROCESS_FEED_GUARD_MS (2000)
#  define LOG_PROCESS_GUARD_TIMEOUT_MS (10000)
#  define LOG_SLOT_RESET_TIMEOUT_MS (10000)
#  define LOG_PROCESS_MAX (128)

static constexpr size_t LOG_SHM_CAPACITY =
  Utils::Utils::next_power_of_2(sirius_log_shm_capacity);

#endif
