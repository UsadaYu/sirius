#include "sirius/internal/init.h"

#include "sirius/internal/decls.h"

bool sirius_init_state = false;

#ifndef _MSC_VER
static __attribute__((constructor))
#else
static
#endif
void _init() {
  if (sirius_init_state)
    return;

  if (!internal_log_init())
    exit(EXIT_FAILURE);
  if (!log_init())
    exit(EXIT_FAILURE);

  sirius_init_state = true;
}

#ifndef _MSC_VER
static __attribute__((destructor))
#else
static
#endif
void _deinit() {
  if (!sirius_init_state)
    return;

  log_deinit();
  internal_log_deinit();

  sirius_init_state = false;
}

#ifdef _MSC_VER
#  pragma section(".CRT$XCU", read)
__declspec(allocate(".CRT$XCU")) void (*init_ptr)() = _init;
#  pragma section(".CRT$XPD", read)
__declspec(allocate(".CRT$XPD")) void (*deinit_ptr)() = _deinit;
#endif

void _internal_init() {
#ifdef _MSC_VER
  (void)init_ptr;
  (void)deinit_ptr;
#endif
}
