add_library(router router.cpp
    url_parser.cpp
    static_file_handler.cpp)

target_compile_features(router PUBLIC cxx_std_17)

target_include_directories(router PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/miniserver/http/router>)

target_link_libraries(router PUBLIC
    http_types
    middleware
    logger
    Boost::headers
    nlohmann_json::nlohmann_json
)
