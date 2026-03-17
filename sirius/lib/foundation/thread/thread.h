#pragma once
/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

#ifndef LIB_FOUNDATION_THREAD_THREAD_H_
#  error "This header file can only be included by the thread module"
#endif

#include "utils/attributes.h"

#if defined(_WIN32) || defined(_WIN64)
#  if defined(__cplusplus)
extern "C" {
#  endif
sirius_api BOOL inner_thread_tls_set_value(LPVOID lpTlsValue,
                                                       DWORD *dw_err);
sirius_api LPVOID inner_thread_tls_get_value(DWORD *dw_err);
#  if defined(__cplusplus)
}
#  endif
#endif
