#include "test.h"

int main() {
  t_dprintf(1, "--------------------------------\n");
  t_dprintf(1, "Sleep 1 s, start\n");
  sirius_usleep(1 * 1000 * 1000);
  t_dprintf(1, "Sleep 1 s, end\n");
  t_dprintf(1, "--------------------------------\n");

  t_dprintf(1, "--------------------------------\n");
  t_dprintf(1, "Sleep 2 s, start\n");
  sirius_nsleep(2 * 1000 * 1000 * 1000);
  t_dprintf(1, "Sleep 2 s, end\n");
  t_dprintf(1, "--------------------------------\n");

  t_dprintf(1, "--------------------------------\n");
  t_dprintf(1, "Sleep 3 s, start\n");
  sirius_nsleep(3ULL * 1000 * 1000 * 1000);
  t_dprintf(1, "Sleep 3 s, end\n");
  t_dprintf(1, "--------------------------------\n");

  return 0;
}
