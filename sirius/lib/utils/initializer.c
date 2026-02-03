#include "lib/utils/initializer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @note This variable only needs to be guaranteed to exist, not a public
 * variable.
 */
int sirius_utils_link_anchor = 0;

#ifdef __cplusplus
}
#endif

static void deinit(void) {
  destructor_utils_thread();
  destructor_utils_log();
}

#ifndef _MSC_VER
__attribute__((constructor))
#endif
static void init(void) {
  if (!constructor_utils_log())
    goto label_exit;
  if (!constructor_utils_thread())
    goto label_exit;

  atexit(deinit);

  return;

label_exit:
#ifdef _WIN32
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

__declspec(allocate(".CRT$XCS")) void(WINAPI *sirius_internal_init_ptr)(void) =
  init;

#endif
