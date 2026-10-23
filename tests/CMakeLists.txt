add_executable(database_tests database_tests.cpp)
target_compile_features(database_tests PRIVATE cxx_std_17)
target_link_libraries(database_tests PRIVATE database)
add_test(NAME database_tests COMMAND database_tests)

add_executable(config_database_tests
    config_database_tests.cpp
    ../src/app/application_config.cpp)
target_compile_features(config_database_tests PRIVATE cxx_std_17)
target_include_directories(config_database_tests PRIVATE
    ../src/app
    ../libs/server
    ../libs/logging)
target_link_libraries(config_database_tests PRIVATE nlohmann_json::nlohmann_json)
add_test(NAME config_database_tests COMMAND config_database_tests)

add_executable(authentication_tests authentication_tests.cpp)
target_compile_features(authentication_tests PRIVATE cxx_std_17)
target_link_libraries(authentication_tests PRIVATE auth router)
add_test(NAME authentication_tests COMMAND authentication_tests)
