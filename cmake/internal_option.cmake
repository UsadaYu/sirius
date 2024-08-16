set(_src_dir ${PROJECT_SOURCE_DIR}/src)
aux_source_directory(${_src_dir} _src_list)
aux_source_directory(
  ${_src_dir}/internal
  _src_list_internal
)
list(APPEND _src_list ${_src_list_internal})

set(LIBRARY_OUTPUT_PATH ${TARGET_LIB_DIR})
if(BUILD_SHARED_LIBS)
  add_library(${SIRIUS_TARGET_NAME} SHARED ${_src_list})
else()
  add_library(${SIRIUS_TARGET_NAME} STATIC ${_src_list})
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  if(SIRIUS_PIC_ENABLE)
    target_compile_options(
      ${SIRIUS_TARGET_NAME}
      PRIVATE -fPIC
    )
  endif()
endif()

# asan
if(SIRIUS_ASAN)
  target_compile_options(
    ${SIRIUS_TARGET_NAME}
    PUBLIC -fsanitize=address
    PUBLIC -fno-omit-frame-pointer
    PUBLIC -fsanitize-recover=address
  )
  target_link_options(
    ${SIRIUS_TARGET_NAME}
    PUBLIC -fsanitize=address
  )
endif()

# gcov
if(SIRIUS_GCOV)
  target_compile_options(
    ${SIRIUS_TARGET_NAME}
    PUBLIC --coverage
  )
  target_link_options(
    ${SIRIUS_TARGET_NAME}
    PUBLIC --coverage
  )
endif()

# all warnings
if(SIRIUS_WARNING_ALL)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(
      ${SIRIUS_TARGET_NAME}
      PRIVATE /W4
    )
  else()
    target_compile_options(
      ${SIRIUS_TARGET_NAME}
      PRIVATE -Wall
    )
  endif()
endif()

# compile warnings as errors
if(SIRIUS_WARNING_ERROR)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(
      ${SIRIUS_TARGET_NAME}
      PRIVATE /WX
    )
  else()
    target_compile_options(
      ${SIRIUS_TARGET_NAME}
      PRIVATE -Werror
    )
  endif()
endif()

target_compile_definitions(
  ${SIRIUS_TARGET_NAME}
  PRIVATE -Dlog_module_name="${SIRIUS_MODULE_PRINT_NAME}"
)

target_compile_definitions(
  ${SIRIUS_TARGET_NAME}
  PRIVATE -DWRITE_SIZE=${SIRIUS_WRITE_SIZE}
)

target_compile_definitions(
  ${SIRIUS_TARGET_NAME}
  PRIVATE -DINTERNAL_LOG_LEVEL=${SIRIUS_INTERNAL_LOG_LEVEL}
)

target_include_directories(
  ${SIRIUS_TARGET_NAME}
  PRIVATE ${PROJECT_SOURCE_DIR}/include
)
