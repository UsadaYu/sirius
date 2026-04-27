#pragma once
/* clang-format off */
#include "utils/decls.h"
/* clang-format on */

#include "lib/foundation/attributes.h"

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

FOUNDATION_API void
structor_destructor_register(structor_destructor_register_t *cfg);

#ifdef __cplusplus
}
#endif
