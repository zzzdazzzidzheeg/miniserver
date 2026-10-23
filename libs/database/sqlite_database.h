#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <sqlite3.h>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace database {

class SQLiteDatabase;

struct NewUser {
    std::string username;
    std::optional<std::string> email;
    // Store a password hash produced by an application-selected password
    // hashing function. Never pass a raw password here.
    std::string password_hash;
    bool confirmed = false;
};

struct UserRecord {
    std::int64_t id;
    std::string username;
    std::optional<std::string> email;
    std::string password_hash;
    bool confirmed;
};

class UsersRepository {
public:
    std::int64_t add(const NewUser& user);
    std::optional<UserRecord> find_by_id(std::int64_t id) const;
    std::optional<UserRecord> find_by_username(std::string_view username) const;
    std::optional<UserRecord> find_by_email(std::string_view email) const;
    bool set_confirmed(std::int64_t id, bool confirmed);

private:
    friend class SQLiteDatabase;
    explicit UsersRepository(SQLiteDatabase& database, bool email_required) noexcept;
    void initialize();

    SQLiteDatabase* database_;
    bool email_required_;
};

using Value = std::variant<std::nullptr_t, std::int64_t, double, std::string>;
using Parameters = std::vector<Value>;
using Row = std::unordered_map<std::string, Value>;
using Rows = std::vector<Row>;

class SQLiteDatabase {
private:
    sqlite3* connection_ = nullptr;
    mutable std::mutex connection_mutex_;

public:
    explicit SQLiteDatabase(const std::string& file_path, bool user_email_required = false);
    ~SQLiteDatabase();

    SQLiteDatabase(const SQLiteDatabase&) = delete;
    SQLiteDatabase& operator=(const SQLiteDatabase&) = delete;

    // Executes a statement and returns the number of rows changed.
    int execute(std::string_view sql, const Parameters& parameters = {});

    // Executes a query and returns rows indexed by column name.
    Rows query(std::string_view sql, const Parameters& parameters = {});

    std::int64_t last_insert_row_id() const;

    // Typed account storage backed by this SQLite connection.
    UsersRepository users;

private:
    friend class UsersRepository;
};

} // namespace database
