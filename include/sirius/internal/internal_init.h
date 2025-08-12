#ifndef __INTERNAL_INIT_H__
#define __INTERNAL_INIT_H__

#include <stdbool.h>

#include "sirius_attributes.h"

extern bool sirius_init_state;

bool _internal_log_init();
void _internal_log_deinit();

bool _log_init();
void _log_deinit();

void _internal_init();

force_inline bool internal_is_init() {
  return (sirius_init_state == true);
}

force_inline void internal_init() {
  if (unlikely(!internal_is_init())) {
    _internal_init();
  }
}

#endif  // __INTERNAL_INIT_H__
