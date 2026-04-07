#include <sirius/c/time.h>

#include "inner/utils.h"

int main() {
  utils_init();

  uint64_t s = 0, e = 0;
  uint64_t duration = 0;

  ss_log_infosp("--------------------------------\n");
  duration = 15000;
  ss_log_infosp("Sleep %" PRIu64 " us\n", duration);
  s = ss_get_clock_monotonic_us();
  ss_log_infosp("Current timestamp (us): %" PRIu64 "\n", s);
  ss_usleep(duration);
  e = ss_get_clock_monotonic_us();
  ss_log_infosp("Current timestamp (us): %" PRIu64 "\n", e);
  ss_log_infosp("Print time difference: %" PRIu64 "us\n", e - s);
  ss_log_infosp("--------------------------------\n");

  ss_log_infosp("--------------------------------\n");
  duration = 2700000;
  ss_log_infosp("Sleep %" PRIu64 " us\n", duration);
  s = ss_get_clock_monotonic_ns();
  ss_log_infosp("Current timestamp (ns): %" PRIu64 "\n", s);
  ss_nsleep(duration);
  e = ss_get_clock_monotonic_ns();
  ss_log_infosp("Current timestamp (ns): %" PRIu64 "\n", e);
  ss_log_infosp("Print time difference: %" PRIu64 "ns\n", e - s);
  ss_log_infosp("--------------------------------\n");

  utils_deinit();

  return 0;
}
