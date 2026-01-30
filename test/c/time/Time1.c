#include <sirius/c/time.h>

#include "internal/utils.h"

int main() {
  utils_init();

  uint64_t s = 0, e = 0;
  uint64_t duration = 0;

  sirius_infosp("--------------------------------\n");
  duration = 15000;
  sirius_infosp("Sleep %" PRIu64 " us\n", duration);
  s = sirius_get_clock_monotonic_us();
  sirius_infosp("Current timestamp (us): %" PRIu64 "\n", s);
  sirius_usleep(duration);
  e = sirius_get_clock_monotonic_us();
  sirius_infosp("Current timestamp (us): %" PRIu64 "\n", e);
  sirius_infosp("Print time difference: %" PRIu64 "us\n", e - s);
  sirius_infosp("--------------------------------\n");

  sirius_infosp("--------------------------------\n");
  duration = 2700000;
  sirius_infosp("Sleep %" PRIu64 " us\n", duration);
  s = sirius_get_clock_monotonic_ns();
  sirius_infosp("Current timestamp (ns): %" PRIu64 "\n", s);
  sirius_nsleep(duration);
  e = sirius_get_clock_monotonic_ns();
  sirius_infosp("Current timestamp (ns): %" PRIu64 "\n", e);
  sirius_infosp("Print time difference: %" PRIu64 "ns\n", e - s);
  sirius_infosp("--------------------------------\n");

  utils_deinit();

  return 0;
}
