add_executable(miniserver main.cpp)
target_link_libraries(miniserver PRIVATE server router logger nlohmann_json::nlohmann_json)

set(SERVER_CERT_SOURCE "${PROJECT_SOURCE_DIR}/certs/cert.pem"
    CACHE FILEPATH "TLS certificate")
set(SERVER_KEY_SOURCE "${PROJECT_SOURCE_DIR}/certs/key.pem"
    CACHE FILEPATH "TLS private key")

set(SERVER_CERT_RUNTIME "${CMAKE_CURRENT_BINARY_DIR}/cert.pem")
set(SERVER_KEY_RUNTIME "${CMAKE_CURRENT_BINARY_DIR}/key.pem")

if(EXISTS "${SERVER_CERT_SOURCE}" AND EXISTS "${SERVER_KEY_SOURCE}")
    configure_file("${SERVER_CERT_SOURCE}" "${SERVER_CERT_RUNTIME}" COPYONLY)
    configure_file("${SERVER_KEY_SOURCE}" "${SERVER_KEY_RUNTIME}" COPYONLY)
else()
    message(STATUS "TLS certificate or private key not found; the server will require them at runtime")
    set(SERVER_CERT_RUNTIME "${SERVER_CERT_SOURCE}")
    set(SERVER_KEY_RUNTIME "${SERVER_KEY_SOURCE}")
endif()

target_compile_definitions(miniserver PRIVATE
    "SERVER_CERT_PATH=\"${SERVER_CERT_RUNTIME}\""
    "SERVER_KEY_PATH=\"${SERVER_KEY_RUNTIME}\""
)