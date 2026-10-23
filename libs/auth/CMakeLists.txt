add_library(auth authentication_middleware.cpp auth_handlers.cpp)

target_compile_features(auth PUBLIC cxx_std_17)
target_include_directories(auth PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(auth PUBLIC http_types middleware router logger nlohmann_json::nlohmann_json)
