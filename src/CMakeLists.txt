add_executable(miniserver main.cpp app/server_application.cpp app/application_config.cpp)
target_link_libraries(miniserver PRIVATE server router middleware auth database logger nlohmann_json::nlohmann_json)

add_custom_target(copy_runtime_files ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/config" "${PROJECT_BINARY_DIR}/config"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/certs" "${PROJECT_BINARY_DIR}/certs"
    DEPENDS miniserver
    COMMENT "Copying config and TLS certificates to the build directory"
)
