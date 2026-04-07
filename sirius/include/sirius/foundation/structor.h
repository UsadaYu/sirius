#pragma once

#include "sirius/attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @note This function is best called explicitly, especially on Windows when
 * building dynamic libraries.
 */
sirius_api void ss_global_destruct();

#ifdef __cplusplus
}
#endif
