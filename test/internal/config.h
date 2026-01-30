#pragma once

// --- sirius_test_win_crtdbg ---
#ifdef sirius_test_win_crtdbg
#  undef sirius_test_win_crtdbg
#endif

#ifdef _SIRIUS_TEST_WIN_CRTDBG
#  ifdef _MSC_VER
#    define sirius_test_win_crtdbg
#  endif
#endif

// ---
