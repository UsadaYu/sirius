set(sources "main.cpp")

set(target ${SIRIUS_EXE_DAEMON_NAME})

add_executable(${target} ${sources})

bin_set_exe_args(TARGET ${target} DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
