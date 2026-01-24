#pragma once

#include "utils/attributes.h"
#include "utils/decls.h"

#ifdef __cplusplus
extern "C" {
#endif

bool constructor_utils_log();
bool constructor_utils_thread();

void destructor_utils_log();
void destructor_utils_thread();

#ifdef __cplusplus
}
#endif

#if defined(_MSC_VER)
#  if defined(_M_IX86)
#    pragma comment(linker, "/include:_sirius_internal_link_anchor")
#  else
#    pragma comment(linker, "/include:sirius_utils_link_anchor")
#  endif
#elif defined(__GNUC__) || defined(__clang__)
/**
 * @note `anchor` is not necessary for most non-Windows platforms, but it is
 * still retained here.
 */
extern int sirius_utils_link_anchor;
static const int *__sirius_keep_alive __attribute__((used, section(".data"))) =
  &sirius_utils_link_anchor;
#endif
