set(sources "main.cpp")

set(target ${SIRIUS_EXE_LOG_NAME})

add_executable(${target} ${sources})

bin_set_exe_args(TARGET ${target} DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(
  ${target} PRIVATE "${SIRIUS_NAMESPACE}::${SIRIUS_FOUNDATION_LIBRARY_NAME}")
