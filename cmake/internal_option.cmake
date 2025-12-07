set(_src_list "")
set(_src_dir ${PROJECT_SOURCE_DIR}/src)
aux_source_directory(${_src_dir} _aux_src_list)
aux_source_directory(${_src_dir}/internal _aux_src_internal_list)
list(APPEND _src_list ${_aux_src_list})
list(APPEND _src_list ${_aux_src_internal_list})
unset(_aux_src_internal_list)
unset(_aux_src_list)

if(BUILD_SHARED_LIBS)
  add_library(${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME} SHARED ${_src_list})

  if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME}
                               PUBLIC ${SIRIUS_WIN_DLL})
  endif()
else()
  add_library(${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME} STATIC ${_src_list})
endif()
add_library(
  ${SIRIUS_TARGET_CMAKE_NAMESPACE}::${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME} ALIAS
  ${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME})

foreach(_suffix "" "_DEBUG" "_RELEASE" "_RELWITHDEBINFO" "_MINSIZEREL")
  set_target_properties(
    ${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME}
    PROPERTIES ARCHIVE_OUTPUT_DIRECTORY${_suffix} ${TARGET_LIB_DIR}
               LIBRARY_OUTPUT_DIRECTORY${_suffix} ${TARGET_LIB_DIR}
               RUNTIME_OUTPUT_DIRECTORY${_suffix} ${TARGET_LIB_DIR})
endforeach()
unset(_suffix)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  if(SIRIUS_PIC_ENABLE)
    target_compile_options(${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME} PRIVATE -fPIC)
  endif()

  target_compile_options(${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME}
                         PRIVATE -fvisibility=hidden)
endif()

# asan
if(SIRIUS_ASAN)
  target_compile_options(
    ${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME}
    PUBLIC -fsanitize=address
    PUBLIC -fno-omit-frame-pointer
    PUBLIC -fsanitize-recover=address)
  target_link_options(${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME} PUBLIC
                      -fsanitize=address)
endif()

# all warnings
if(SIRIUS_WARNING_ALL)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME} PRIVATE /W4)
  else()
    target_compile_options(${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME} PRIVATE -Wall)
  endif()
endif()

# compile warnings as errors
if(SIRIUS_WARNING_ERROR)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME} PRIVATE /WX)
  else()
    target_compile_options(${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME} PRIVATE -Werror)
  endif()
endif()

target_compile_definitions(${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME}
                           PRIVATE SIRIUS_BUILDING)

target_compile_definitions(
  ${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME}
  PRIVATE -Dlog_module_name="${SIRIUS_MODULE_PRINT_NAME}")

target_compile_definitions(${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME}
                           PRIVATE -DWRITE_SIZE=${SIRIUS_WRITE_SIZE})

target_compile_definitions(
  ${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME}
  PRIVATE -DINTERNAL_LOG_LEVEL=${SIRIUS_INTERNAL_LOG_LEVEL})

target_include_directories(${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME}
                           PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_directories(${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME} PRIVATE
                        ${PRIVATE_LINK_DIR_LIST})
target_link_libraries(${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME}
                      PRIVATE ${PRIVATE_LINK_LIBS_LIST})

target_include_directories(${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME} SYSTEM
                           INTERFACE ${ITFC_INCLUDE_DIR_LIST})
target_link_directories(${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME} INTERFACE
                        ${ITFC_LINK_DIR_LIST})
target_link_libraries(${SIRIUS_TARGET_SIRIUS_LIBRARY_NAME}
                      INTERFACE ${ITFC_LINK_LIBS_LIST})
