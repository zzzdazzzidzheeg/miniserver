add_library(middleware request_logging.cpp statistics_middleware.cpp)

target_compile_features(middleware PUBLIC cxx_std_17)
target_include_directories(middleware PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(middleware PUBLIC http_types logger)
