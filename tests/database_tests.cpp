#include "sqlite_database.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename Exception, typename Callable>
void require_throws(Callable&& callable, const std::string& message) {
    try {
        callable();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error(message);
}

template <typename T>
const T& value_as(const database::Row& row, const std::string& column) {
    const auto it = row.find(column);
    require(it != row.end(), "Missing result column: " + column);
    const auto* value = std::get_if<T>(&it->second);
    require(value != nullptr, "Unexpected value type for column: " + column);
    return *value;
}

} // namespace

int main() {
    const auto unique_part = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory = std::filesystem::temp_directory_path() /
        ("miniserver_database_tests_" + std::to_string(unique_part));
    const auto file = directory / "nested" / "test.sqlite3";

    try {
        require_throws<std::invalid_argument>([] {
            database::SQLiteDatabase invalid("");
        }, "Empty database path should be rejected");

        {
            database::SQLiteDatabase db(file.string());
            require(std::filesystem::exists(file), "Database file was not created");

            db.execute("CREATE TABLE records (id INTEGER PRIMARY KEY, name TEXT, score REAL, note TEXT)");
            const std::string injection = "x'); DROP TABLE records; --";
            const auto changed = db.execute(
                "INSERT INTO records (name, score, note) VALUES (?, ?, ?)",
                {injection, 3.5, nullptr});
            require(changed == 1, "INSERT should report one changed row");
            require(db.last_insert_row_id() == 1, "Unexpected last inserted row id");

            const auto rows = db.query(
                "SELECT id, name, score, note, NULL AS absent FROM records WHERE id = ?",
                {std::int64_t{1}});
            require(rows.size() == 1, "Expected one query result row");
            require(value_as<std::int64_t>(rows.front(), "id") == 1, "Unexpected integer result");
            require(value_as<std::string>(rows.front(), "name") == injection,
                "Bound text was not returned intact");
            require(value_as<double>(rows.front(), "score") == 3.5,
                "Unexpected floating point result");
            require(std::holds_alternative<std::nullptr_t>(rows.front().at("note")),
                "Bound NULL was not returned as NULL");
            require(std::holds_alternative<std::nullptr_t>(rows.front().at("absent")),
                "SQL NULL was not returned as NULL");

            require_throws<std::invalid_argument>([&] {
                db.execute("INSERT INTO records (name) VALUES (?)");
            }, "Missing bind parameter should be rejected");
            require_throws<std::invalid_argument>([&] {
                db.execute("SELECT 1");
            }, "execute() should reject statements that return rows");
            require_throws<std::invalid_argument>([&] {
                db.query("UPDATE records SET score = 0");
            }, "query() should reject statements that do not return rows");
            require_throws<std::invalid_argument>([&] {
                db.execute("CREATE TABLE another_table (id INTEGER); DROP TABLE records;");
            }, "Multiple SQL statements should be rejected");
            require_throws<std::runtime_error>([&] {
                db.query("SELECT missing_column FROM records");
            }, "Invalid SQL should report an error");

            const auto count = db.query("SELECT COUNT(*) AS total FROM records");
            require(value_as<std::int64_t>(count.front(), "total") == 1,
                "Bound SQL data was treated as executable SQL");
        }

        {
            database::SQLiteDatabase reopened(file.string());
            const auto rows = reopened.query("SELECT name FROM records");
            require(rows.size() == 1, "Data did not persist after reopening the database");
        }

        std::filesystem::remove_all(directory);
        std::cout << "Database interface checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
