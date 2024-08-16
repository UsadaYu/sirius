#include "sirius_time.h"

#include "internal/internal_sys.h"
#include "sirius_attributes.h"

void sirius_usleep(unsigned long long usec) {
#ifdef _WIN32
  /**
   * In windows, `DWORD` is usually of type
   * `unsigned long`.
   */
  while (usec > 0) {
    DWORD t = (usec > UINT_MAX) ? UINT_MAX : (DWORD)usec;
    Sleep((DWORD)(usec / 1000));
    usec -= t;
  }

#else
  /**
   * In unix, `useconds_t` is usually of type
   * `unsigned int`.
   */
  while (usec > 0) {
    useconds_t t =
        (usec > UINT_MAX) ? UINT_MAX : (useconds_t)usec;
    usleep(t);
    usec -= t;
  }

#endif
}