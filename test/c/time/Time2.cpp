#include <sirius/c/time.h>

#include "inner/utils.h"

namespace {
inline int main_impl() {
  ss_log_infosp("--------------------------------\n");
  ss_log_infosp("Sleep 1 s, start\n");
  ss_usleep(1 * 1000 * 1000);
  ss_log_infosp("Sleep 1 s, end\n");
  ss_log_infosp("--------------------------------\n");

  ss_log_infosp("--------------------------------\n");
  ss_log_infosp("Sleep 2 s, start\n");
  ss_nsleep(2 * 1000 * 1000 * 1000);
  ss_log_infosp("Sleep 2 s, end\n");
  ss_log_infosp("--------------------------------\n");

  ss_log_infosp("--------------------------------\n");
  ss_log_infosp("Sleep 1 s, start\n");
  ss_nsleep(1ULL * 1000 * 1000 * 1000);
  ss_log_infosp("Sleep 1 s, end\n");
  ss_log_infosp("--------------------------------\n");

  return 0;
}
} // namespace

int main() {
  auto init = utils::Init();

  try {
    return main_impl();
  } catch (const std::exception &e) {
    ss_log_error("%s\n", e.what());
    return -1;
  } catch (...) {
    ss_log_error("`exception`: unknow\n");
    return -1;
  }
}
