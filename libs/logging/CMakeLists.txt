add_library(logger logger.cpp)

target_compile_features(logger PUBLIC cxx_std_17)

target_include_directories(logger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})