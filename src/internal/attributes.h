#ifndef INTERNAL_ATTRIBUTES_H
#define INTERNAL_ATTRIBUTES_H

#include "sirius/sirius_attributes.h"

// clang-format off

#define internal_check_sizeof(sirius, platform) \
  sirius_static_assert(sizeof(sirius) >= sizeof(platform),\
  "The size of `" #sirius "` is smaller than that of `" #platform "`")

#define internal_check_alignof(sirius, platform) \
  sirius_static_assert(alignof(sirius) >= alignof(platform),\
  "The alignment of `" #sirius "` is weaker than that of `" #platform "`")

// clang-format on

#endif // INTERNAL_ATTRIBUTES_H
