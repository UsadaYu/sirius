include(${CMAKE_CURRENT_SOURCE_DIR}/external_option.cmake)
set(TARGET_NAME ${USER_TARGET_PREFIX})

include(${CMAKE_CURRENT_SOURCE_DIR}/internal_option.cmake)

# 库文件安装
install(TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

# pkgconfig
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/sirius.pc.in
    ${PROJECT_BINARY_DIR}/lib/sirius.pc
    @ONLY
)
install(
    FILES ${PROJECT_BINARY_DIR}/lib/sirius.pc
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig
)

# 头文件安装
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include
    FILES_MATCHING PATTERN "*.h"
    PATTERN "internal" EXCLUDE
)
