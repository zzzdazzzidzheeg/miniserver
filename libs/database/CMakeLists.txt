add_library(database sqlite_database.cpp)

target_compile_features(database PUBLIC cxx_std_17)
target_include_directories(database PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(database PUBLIC SQLite3::SQLite3)
