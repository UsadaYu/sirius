#pragma once

#ifdef _SIRIUS_WIN_CRTDBG
#  define _CRTDBG_MAP_ALLOC
#  include <crtdbg.h>
#  include <stdlib.h>
#  include <string.h>
#endif

class Debug {
 public:
  Debug() {
#ifdef _SIRIUS_WIN_CRTDBG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
#endif
  }

  ~Debug() = default;
};
