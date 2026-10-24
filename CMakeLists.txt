cmake_minimum_required(VERSION 3.20)
project(miniserver VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

add_subdirectory(framework)
add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
