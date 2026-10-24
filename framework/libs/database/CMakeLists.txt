add_library(database sqlite_database.cpp)

target_compile_features(database PUBLIC cxx_std_17)
target_include_directories(database PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/miniserver/database>)
target_link_libraries(database PUBLIC SQLite3::SQLite3)
