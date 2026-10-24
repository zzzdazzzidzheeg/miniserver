add_library(server server.cpp session.cpp)

target_compile_features(server PUBLIC cxx_std_17)

target_include_directories(server PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/miniserver/server>)

target_link_libraries(server PUBLIC
    http_types
    logger
    Boost::headers
    OpenSSL::SSL
    OpenSSL::Crypto
)
