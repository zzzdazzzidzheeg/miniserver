cmake_minimum_required(VERSION 3.20)
project(miniserver LANGUAGES CXX)

include(CTest)

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(SQLite3 REQUIRED)

add_subdirectory(libs/http_types)
add_subdirectory(libs/database)
add_subdirectory(libs/logger)
add_subdirectory(libs/middleware)
add_subdirectory(libs/server)
add_subdirectory(libs/router)
add_subdirectory(libs/auth)
add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
