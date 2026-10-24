include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(SQLite3 REQUIRED)

add_subdirectory(libs/http/types)
add_subdirectory(libs/database)
add_subdirectory(libs/logging)
add_subdirectory(libs/http/middleware)
add_subdirectory(libs/server)
add_subdirectory(libs/http/router)
add_subdirectory(libs/auth)
add_subdirectory(libs/application)

add_library(miniserver_framework INTERFACE)
add_library(miniserver::framework ALIAS miniserver_framework)
set_target_properties(miniserver_framework PROPERTIES EXPORT_NAME framework)
target_link_libraries(miniserver_framework INTERFACE miniserver_application)

set(MINISERVER_FRAMEWORK_TARGETS
    http_types database logger middleware server router auth miniserver_application miniserver_framework)

foreach(target IN LISTS MINISERVER_FRAMEWORK_TARGETS)
    install(TARGETS ${target} EXPORT miniserverTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endforeach()

install(DIRECTORY libs/application/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/miniserver/application
    FILES_MATCHING PATTERN "*.h")
install(DIRECTORY libs/auth/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/miniserver/auth
    FILES_MATCHING PATTERN "*.h")
install(DIRECTORY libs/database/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/miniserver/database
    FILES_MATCHING PATTERN "*.h")
install(DIRECTORY libs/http/middleware/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/miniserver/http/middleware
    FILES_MATCHING PATTERN "*.h")
install(DIRECTORY libs/http/router/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/miniserver/http/router
    FILES_MATCHING PATTERN "*.h")
install(DIRECTORY libs/http/types/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/miniserver/http/types
    FILES_MATCHING PATTERN "*.h")
install(DIRECTORY libs/logging/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/miniserver/logging
    FILES_MATCHING PATTERN "*.h")
install(DIRECTORY libs/server/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/miniserver/server
    FILES_MATCHING PATTERN "*.h")

configure_package_config_file(
    cmake/miniserverConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/miniserverConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/miniserver)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/miniserverConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(EXPORT miniserverTargets
    FILE miniserverTargets.cmake
    NAMESPACE miniserver::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/miniserver)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/miniserverConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/miniserverConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/miniserver)
