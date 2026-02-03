#pragma once

#ifndef LIB_UTILS_THREAD_THREAD_H_
#  error "This header file can only be included by the thread module"
#endif

#include "utils/attributes.h"
#include "utils/decls.h"

#if defined(_WIN32) || defined(_WIN64)

#  if defined(__cplusplus)
extern "C" {
#  endif

sirius_api BOOL sirius_utils_thread_tls_set_value(LPVOID lpTlsValue);

sirius_api LPVOID sirius_utils_thread_tls_get_value();

#  if defined(__cplusplus)
}
#  endif

#endif
