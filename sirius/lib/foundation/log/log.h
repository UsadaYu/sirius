#pragma once

#include "sirius/foundation/log.h"
#include "utils/io.h"

#if defined(_WIN32) || defined(_WIN64)
static inline void foundation_win_last_error(DWORD error_code,
                                             const char *msg) {
  char es[512] = {0};
  char e[_SIRIUS_LOG_BUF_SIZE] = {0};
  char schrodinger_space = (likely(msg) && msg[0] == ' ') ? '\0' : ' ';

  snprintf(es, sizeof(es), "%c%s", schrodinger_space, msg);

  DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD size = FormatMessage(flags, nullptr, error_code,
                             MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), e,
                             sizeof(e) / sizeof(TCHAR), nullptr);
  if (unlikely(size == 0)) {
    sirius_errorsp("%s: %lu\n", es, error_code);
  } else {
    sirius_errorsp("%s: %lu. %s", es, error_code, e);
  }
}
#endif

static inline void foundation_errno_error(int error_code, const char *msg) {
  char es[512] = {0};
  char e[_SIRIUS_LOG_BUF_SIZE];
  char schrodinger_space = (likely(msg) && msg[0] == ' ') ? '\0' : ' ';

  snprintf(es, sizeof(es), "%c%s", schrodinger_space, msg);

  if (likely(0 == UTILS_STRERROR_R(error_code, e, sizeof(e)))) {
    sirius_errorsp("%s: %lu. %s", es, error_code, e);
  } else {
    sirius_errorsp("%s: %lu\n", es, error_code);
  }
}
