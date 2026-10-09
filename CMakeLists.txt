cmake_minimum_required(VERSION 3.20)
project(miniserver LANGUAGES CXX)

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)

add_subdirectory(libs/logger)
add_subdirectory(libs/server)
add_subdirectory(libs/webapp)
add_subdirectory(src)
