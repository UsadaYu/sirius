#ifndef INTERNAL_INITIALIZER_H
#define INTERNAL_INITIALIZER_H

#include "internal/attributes.h"
#include "internal/decls.h"

#ifdef __cplusplus
extern "C" {
#endif

bool internal_init_log();
void internal_deinit_log();

bool internal_init_thread();
void internal_deinit_thread();

#ifdef __cplusplus
}
#endif

#if defined(_MSC_VER)
#  if defined(_M_IX86)
#    pragma comment(linker, "/include:_sirius_internal_link_anchor")
#  else
#    pragma comment(linker, "/include:sirius_internal_link_anchor")
#  endif
#elif defined(__GNUC__) || defined(__clang__)
/**
 * @note `anchor` is not necessary for most non-Windows platforms, but it is
 * still retained here.
 */
extern int sirius_internal_link_anchor;
static const int *__sirius_keep_alive __attribute__((used, section(".data"))) =
  &sirius_internal_link_anchor;
#endif

#endif // INTERNAL_INITIALIZER_H
