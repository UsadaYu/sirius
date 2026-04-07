#pragma once
/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

#include "utils/attributes.h"

enum StructorDestructorPriority {
  kStructorDestructorPriorityLatest = 1,
  kStructorDestructorPriorityEarliest = 99,
};

typedef struct {
  /**
   * @ref `enum StructorDestructorPriority`.
   */
  uint64_t priority;

  void (*fn_destructor)();
} structor_destructor_register_t;

#ifdef __cplusplus
extern "C" {
#endif

void structor_destructor_register(structor_destructor_register_t *cfg);

bool constructor_foundation_log();

#ifdef __cplusplus
}
#endif

#if defined(_MSC_VER)
#  if defined(_M_IX86)
#    pragma comment(linker, "/include:_sirius_inner_foundation_link_anchor")
#  else
#    pragma comment(linker, "/include:sirius_inner_foundation_link_anchor")
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  ifdef __cplusplus
extern "C" {
#  endif
/**
 * @note `anchor` is not necessary for most non-Windows platforms, but it is
 * still retained here.
 * This variable only needs to be guaranteed to exist, not a public variable.
 */
extern int sirius_inner_foundation_link_anchor;
#  ifdef __cplusplus
}
#  endif

static const int *sirius_inner_foundation_keep_alive
  __attribute__((used, section(".data"))) =
    &sirius_inner_foundation_link_anchor;
#endif
