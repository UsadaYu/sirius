list(APPEND TEST_LINK_LIBRARIES
     "${SIRIUS_TARGET_CMAKE_NAMESPACE}::${SIRIUS_THREAD_LIBRARY_NAME}")

add_subdirectory(cond)
add_subdirectory(lock)
add_subdirectory(sem)
add_subdirectory(thread)
