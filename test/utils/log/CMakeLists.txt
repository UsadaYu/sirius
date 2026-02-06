# --- Log1 ---
test_add_exes_and_tests(
  MAIN
  "Log1.cpp"
  LANGUAGE
  "CXX"
  DIRECTORY
  ${CMAKE_CURRENT_BINARY_DIR}
  TARGETS
  targets)

foreach(target IN LISTS targets)
  cmake_path(SET _gen NORMALIZE
             "${CMAKE_CURRENT_BINARY_DIR}/_gen_${target}.log")
  target_compile_definitions(${target} PRIVATE "GEN_FILE_NAME=\"${_gen}\"")
  set_property(
    DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    APPEND
    PROPERTY ADDITIONAL_CLEAN_FILES ${_gen})
endforeach()
