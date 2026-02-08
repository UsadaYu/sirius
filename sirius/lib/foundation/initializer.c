#include "lib/foundation/initializer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @note This variable only needs to be guaranteed to exist, not a public
 * variable.
 */
int sirius_foundation_link_anchor_ = 0;

#ifdef __cplusplus
}
#endif

static void destructor(void) {
  destructor_foundation_thread();
  destructor_foundation_log();
}

#ifndef _MSC_VER
__attribute__((constructor))
#endif
static void constructor(void) {
  if (!constructor_foundation_log())
    goto label_exit;
  if (!constructor_foundation_thread())
    goto label_exit;

  atexit(destructor);

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

__declspec(allocate(".CRT$XCS")) void(WINAPI *sirius_internal_constructor_ptr)(
  void) = constructor;
#endif
