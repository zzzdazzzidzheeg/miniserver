add_library(server server.cpp session.cpp)

target_compile_features(server PUBLIC cxx_std_17)

target_include_directories(server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(server PUBLIC
    logger
    Boost::headers
    OpenSSL::SSL
    OpenSSL::Crypto
)