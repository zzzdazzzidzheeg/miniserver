add_executable(miniserver main.cpp)
target_link_libraries(miniserver PRIVATE miniserver_application)

add_custom_target(copy_runtime_files ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/config" "${PROJECT_BINARY_DIR}/config"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/certs" "${PROJECT_BINARY_DIR}/certs"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/static" "${PROJECT_BINARY_DIR}/static"
    DEPENDS miniserver
    COMMENT "Copying config and TLS certificates to the build directory"
)
