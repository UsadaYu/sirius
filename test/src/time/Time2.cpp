#include "internal/utils.h"

int main() {
  auto init = Utils::Init();

  sirius_infosp("--------------------------------\n");
  sirius_infosp("Sleep 1 s, start\n");
  sirius_usleep(1 * 1000 * 1000);
  sirius_infosp("Sleep 1 s, end\n");
  sirius_infosp("--------------------------------\n");

  sirius_infosp("--------------------------------\n");
  sirius_infosp("Sleep 2 s, start\n");
  sirius_nsleep(2 * 1000 * 1000 * 1000);
  sirius_infosp("Sleep 2 s, end\n");
  sirius_infosp("--------------------------------\n");

  sirius_infosp("--------------------------------\n");
  sirius_infosp("Sleep 1 s, start\n");
  sirius_nsleep(1ULL * 1000 * 1000 * 1000);
  sirius_infosp("Sleep 1 s, end\n");
  sirius_infosp("--------------------------------\n");

  return 0;
}
