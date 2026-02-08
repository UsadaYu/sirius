#pragma once

#include "utils/attributes.h"
#include "utils/decls.h"

#ifdef __cplusplus
extern "C" {
#endif

bool constructor_foundation_log();
bool constructor_foundation_thread();

void destructor_foundation_log();
void destructor_foundation_thread();

#ifdef __cplusplus
}
#endif

#if defined(_MSC_VER)
#  if defined(_M_IX86)
#    pragma comment(linker, "/include:_sirius_internal_link_anchor")
#  else
#    pragma comment(linker, "/include:sirius_foundation_link_anchor_")
#  endif
#elif defined(__GNUC__) || defined(__clang__)
/**
 * @note `anchor` is not necessary for most non-Windows platforms, but it is
 * still retained here.
 */
extern int sirius_foundation_link_anchor_;
static const int *__sirius_keep_alive __attribute__((used, section(".data"))) =
  &sirius_foundation_link_anchor_;
#endif
