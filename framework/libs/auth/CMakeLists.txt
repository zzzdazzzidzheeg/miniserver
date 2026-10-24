add_library(auth
    authentication_middleware.cpp
    auth_handlers.cpp
    sqlite_account_service.cpp)

target_compile_features(auth PUBLIC cxx_std_17)
target_include_directories(auth PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/miniserver/auth>)
target_link_libraries(auth PUBLIC
    http_types
    middleware
    router
    logger
    database
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json)
