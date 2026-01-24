# --- Thread1 ---
test_add_exes_and_tests(MAIN "Thread1.cpp" LANGUAGE "CXX" DIRECTORY
                        ${CMAKE_CURRENT_BINARY_DIR})

# --- Thread2 ---
test_add_exes_and_tests(MAIN "Thread2.cpp" LANGUAGE "CXX" DIRECTORY
                        ${CMAKE_CURRENT_BINARY_DIR})

# --- Thread3 ---
test_add_exes_and_tests(MAIN "Thread3.cpp" LANGUAGE "CXX" DIRECTORY
                        ${CMAKE_CURRENT_BINARY_DIR})

# --- Thread4 ---
test_add_exes_and_tests(MAIN "Thread4.c" LANGUAGE "C" DIRECTORY
                        ${CMAKE_CURRENT_BINARY_DIR})

# --- Thread5 ---
test_add_exes_and_tests(MAIN "Thread5.cpp" LANGUAGE "CXX" DIRECTORY
                        ${CMAKE_CURRENT_BINARY_DIR})
