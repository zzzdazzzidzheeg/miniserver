#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <sqlite3.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace database {

using Value = std::variant<std::nullptr_t, std::int64_t, double, std::string>;
using Parameters = std::vector<Value>;
using Row = std::unordered_map<std::string, Value>;
using Rows = std::vector<Row>;

class SQLiteDatabase {
public:
    explicit SQLiteDatabase(const std::string& file_path);
    ~SQLiteDatabase();

    SQLiteDatabase(const SQLiteDatabase&) = delete;
    SQLiteDatabase& operator=(const SQLiteDatabase&) = delete;

    // Executes a statement and returns the number of rows changed.
    int execute(std::string_view sql, const Parameters& parameters = {});

    // Executes a query and returns rows indexed by column name.
    Rows query(std::string_view sql, const Parameters& parameters = {});

    std::int64_t last_insert_row_id() const;

private:
    sqlite3* connection_ = nullptr;
    mutable std::mutex connection_mutex_;
};

} // namespace database
