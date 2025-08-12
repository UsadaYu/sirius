#include "test.h"

int main() {
  unsigned long long s = 0, e = 0;

  t_dprintf(1, "========================\n");
  s = sirius_get_time_us();
  t_dprintf(1, "Current timestamp (us): %llu\n", s);
  t_dprintf(1, "Sleep 15000 us\n");
  sirius_usleep(15000);
  e = sirius_get_time_us();
  t_dprintf(1, "Current timestamp (us): %llu\n", e);
  t_dprintf(1, "Print time difference: %llu us.\n", e - s);
  t_dprintf(1, "========================\n");

  t_dprintf(1, "========================\n");
  s = sirius_get_time_ns();
  t_dprintf(1, "Current timestamp (ns): %llu\n", s);
  t_dprintf(1, "Sleep 2700000 ns\n");
  sirius_nsleep(2700000);
  e = sirius_get_time_ns();
  t_dprintf(1, "Current timestamp (ns): %llu\n", e);
  t_dprintf(1, "Print time difference: %llu ns\n", e - s);
  t_dprintf(1, "========================\n");

  return 0;
}
