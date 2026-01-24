message(STATUS "--------------")
message(STATUS "--- SIRIUS ---")
message(STATUS "--------------")

# --- Global Variables ---
cmake_path(SET GLOB_INCLUDE NORMALIZE "")

# --- Preprocessing ---
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

# ---
add_subdirectory(include/sirius)
add_subdirectory(lib)
