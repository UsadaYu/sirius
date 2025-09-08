#include "test.h"

int main() {
  uint64_t s = 0, e = 0;

  t_dprintf(1, "========================\n");
  s = sirius_get_time_us();
  t_dprintf(1, "Current timestamp (us): %" PRIu64 "\n", s);
  t_dprintf(1, "Sleep 15000 us\n");
  sirius_usleep(15000);
  e = sirius_get_time_us();
  t_dprintf(1, "Current timestamp (us): %" PRIu64 "\n", e);
  t_dprintf(1, "Print time difference: %" PRIu64 "us.\n",
            e - s);
  t_dprintf(1, "========================\n");

  t_dprintf(1, "========================\n");
  s = sirius_get_time_ns();
  t_dprintf(1, "Current timestamp (ns): %" PRIu64 "\n", s);
  t_dprintf(1, "Sleep 2700000 ns\n");
  sirius_nsleep(2700000);
  e = sirius_get_time_ns();
  t_dprintf(1, "Current timestamp (ns): %" PRIu64 "\n", e);
  t_dprintf(1, "Print time difference: %" PRIu64 "ns\n",
            e - s);
  t_dprintf(1, "========================\n");

  return 0;
}
