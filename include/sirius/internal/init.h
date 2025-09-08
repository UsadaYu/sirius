#ifndef SIRIUS_INTERNAL_INIT_H
#define SIRIUS_INTERNAL_INIT_H

#include "decls.h"
#include "sirius_attributes.h"

extern bool sirius_init_state;

bool internal_log_init();
void internal_log_deinit();

bool log_init();
void log_deinit();

void _internal_init();

static inline bool internal_is_init() { return (sirius_init_state == true); }

static inline void internal_init() {
  if (unlikely(!internal_is_init())) {
    _internal_init();
  }
}

#endif  // SIRIUS_INTERNAL_INIT_H
