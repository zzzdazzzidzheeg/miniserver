add_library(miniserver_application
    application_config.cpp
    server_application.cpp)

target_compile_features(miniserver_application PUBLIC cxx_std_17)
target_include_directories(miniserver_application PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(miniserver_application PUBLIC
    server
    router
    middleware
    auth
    database
    logger
    nlohmann_json::nlohmann_json)
