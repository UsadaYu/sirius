#pragma once
/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

#include "lib/foundation/attributes.h"

#if defined(_WIN32) || defined(_WIN64)
#  if defined(__cplusplus)
extern "C" {
#  endif
FOUNDATION_API BOOL inner_thread_tls_set_value(LPVOID lpTlsValue,
                                               DWORD *dw_err);
FOUNDATION_API LPVOID inner_thread_tls_get_value(DWORD *dw_err);
#  if defined(__cplusplus)
}
#  endif
#endif
