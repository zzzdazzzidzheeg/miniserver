add_library(webapp router.cpp
    url_parser.cpp)

target_compile_features(webapp PUBLIC cxx_std_17)

target_include_directories(webapp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(webapp PUBLIC
    logger
    Boost::headers
    nlohmann_json::nlohmann_json
)