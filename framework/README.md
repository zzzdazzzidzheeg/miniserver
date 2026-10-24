# miniserver framework

This directory contains the reusable C++ libraries. The example service, its configuration, tests, and web assets live outside this directory.

## Build and install

Build the repository, then install the framework and its CMake package:

```sh
cmake -S .. -B ../build
cmake --build ../build
cmake --install ../build --prefix /path/to/miniserver-install
```

A consuming project can link the application facade:

```cmake
find_package(miniserver CONFIG REQUIRED)
target_link_libraries(my_service PRIVATE miniserver::framework)
```

Public headers are installed under `include/miniserver/`. The exported targets find Boost, OpenSSL, SQLite3, and nlohmann-json as package dependencies.

The application facade provides the HTTPS server, router, middleware setup, SQLite database access, authentication hooks, statistics, and configuration loading. Applications supply their own routes and runtime configuration.
