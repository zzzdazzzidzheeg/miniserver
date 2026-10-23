#include "application_config.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void write_config(const std::filesystem::path& path, const std::string& database_section) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("Could not create test config");
    output << R"({
        "server": {"certificate_file": "cert.pem", "private_key_file": "key.pem"},
        "session": {},
        "logging": {},
        "database": )" << database_section << "\n}\n";
}

} // namespace

int main() {
    const auto unique_part = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory = std::filesystem::temp_directory_path() /
        ("miniserver_config_database_tests_" + std::to_string(unique_part));

    try {
        std::filesystem::create_directories(directory);
        const auto config_path = directory / "server.json";

        write_config(config_path, R"({"file": "state/app.sqlite3"})");
        const auto configured = load_application_config(config_path.string());
        require(configured.database.file == (directory / "state/app.sqlite3").string(),
            "Relative database path was not resolved from the config directory");

        write_config(config_path, R"({"file": ":memory:"})");
        const auto in_memory = load_application_config(config_path.string());
        require(in_memory.database.file == ":memory:",
            "SQLite in-memory path should remain unchanged");

        write_config(config_path, "{}");
        const auto defaulted = load_application_config(config_path.string());
        require(defaulted.database.file ==
                (directory / "../data/miniserver.sqlite3").lexically_normal().string(),
            "Missing database config should use the default path");

        std::filesystem::remove_all(directory);
        std::cout << "Database config checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
