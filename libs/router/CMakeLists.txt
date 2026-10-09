add_library(router router.cpp
    url_parser.cpp)

target_compile_features(router PUBLIC cxx_std_17)

target_include_directories(router PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(router PUBLIC
    logger
    Boost::headers
    nlohmann_json::nlohmann_json
)