add_library(http_types INTERFACE)

target_compile_features(http_types INTERFACE cxx_std_17)
target_include_directories(http_types INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/miniserver/http/types>)
target_link_libraries(http_types INTERFACE Boost::headers)
