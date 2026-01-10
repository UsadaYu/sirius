# --- Time1 ---
test_add_exes_and_tests(MAIN "Time1.c" LANGUAGE "C" DIRECTORY
                        ${CMAKE_CURRENT_BINARY_DIR})

# --- Time2 ---
test_add_exes_and_tests(MAIN "Time2.cpp" LANGUAGE "CXX" DIRECTORY
                        ${CMAKE_CURRENT_BINARY_DIR})
