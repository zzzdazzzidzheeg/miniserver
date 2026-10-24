#include "sqlite_database.h"

#include <cctype>
#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace database {
namespace {

[[noreturn]] void throw_sqlite_error(sqlite3* connection, int result, const char* operation) {
    const char* detail = connection ? sqlite3_errmsg(connection) : sqlite3_errstr(result);
    throw std::runtime_error(std::string("SQLite ") + operation + " failed: " + detail);
}

std::string sqlite_error_message(sqlite3* connection, int result) {
    return connection ? sqlite3_errmsg(connection) : sqlite3_errstr(result);
}

sqlite3_stmt* prepare(sqlite3* connection, std::string_view sql, const Parameters& parameters) {
    const std::string sql_text(sql);
    if (sql_text.find('\0') != std::string::npos) {
        throw std::invalid_argument("SQLite statement cannot contain a null byte");
    }
    if (sql_text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("SQLite statement is too large");
    }
    sqlite3_stmt* statement = nullptr;
    const char* tail = nullptr;
    const int result = sqlite3_prepare_v2(connection, sql_text.c_str(),
        static_cast<int>(sql_text.size()), &statement, &tail);
    if (result != SQLITE_OK) {
        const std::string detail = sqlite_error_message(connection, result);
        if (statement) sqlite3_finalize(statement);
        throw std::runtime_error("SQLite prepare failed: " + detail);
    }
    if (!statement) throw std::invalid_argument("SQLite statement is empty");
    while (tail && *tail && std::isspace(static_cast<unsigned char>(*tail))) ++tail;
    if (tail && *tail) {
        sqlite3_finalize(statement);
        throw std::invalid_argument("Only one SQLite statement is allowed per call");
    }

    if (parameters.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        sqlite3_bind_parameter_count(statement) != static_cast<int>(parameters.size())) {
        sqlite3_finalize(statement);
        throw std::invalid_argument("SQLite parameter count does not match statement");
    }

    for (std::size_t i = 0; i < parameters.size(); ++i) {
        const int index = static_cast<int>(i + 1);
        const int bind_result = std::visit([statement, index](const auto& value) -> int {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                return sqlite3_bind_null(statement, index);
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                return sqlite3_bind_int64(statement, index, value);
            } else if constexpr (std::is_same_v<T, double>) {
                return sqlite3_bind_double(statement, index, value);
            } else {
                if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
                    return SQLITE_TOOBIG;
                }
                return sqlite3_bind_text(statement, index, value.data(),
                    static_cast<int>(value.size()), SQLITE_TRANSIENT);
            }
        }, parameters[i]);

        if (bind_result != SQLITE_OK) {
            const std::string detail = sqlite_error_message(connection, bind_result);
            sqlite3_finalize(statement);
            throw std::runtime_error("SQLite bind failed: " + detail);
        }
    }

    return statement;
}

Value read_column(sqlite3_stmt* statement, int column) {
    switch (sqlite3_column_type(statement, column)) {
    case SQLITE_INTEGER:
        return static_cast<std::int64_t>(sqlite3_column_int64(statement, column));
    case SQLITE_FLOAT:
        return sqlite3_column_double(statement, column);
    case SQLITE_TEXT: {
        const auto* text = sqlite3_column_text(statement, column);
        const int size = sqlite3_column_bytes(statement, column);
        if (size == 0) return std::string{};
        return std::string(reinterpret_cast<const char*>(text), static_cast<std::size_t>(size));
    }
    case SQLITE_BLOB: {
        const auto* bytes = sqlite3_column_blob(statement, column);
        const int size = sqlite3_column_bytes(statement, column);
        if (size == 0) return std::string{};
        return std::string(static_cast<const char*>(bytes), static_cast<std::size_t>(size));
    }
    case SQLITE_NULL:
    default:
        return nullptr;
    }
}

} // namespace

UsersRepository::UsersRepository(SQLiteDatabase& database, bool email_required) noexcept
    : database_(&database), email_required_(email_required) {}

void UsersRepository::initialize() {
    database_->execute(
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "username TEXT NOT NULL UNIQUE, "
        "email TEXT, "
        "password_hash TEXT NOT NULL, "
        "is_active INTEGER NOT NULL DEFAULT 0 CHECK (is_active IN (0, 1))"
        ")");
    const auto columns = database_->query("PRAGMA table_info(users)");
    bool has_active_column = false;
    bool has_legacy_confirmed_column = false;
    for (const auto& column : columns) {
        const auto& name = std::get<std::string>(column.at("name"));
        has_active_column = has_active_column || name == "is_active";
        has_legacy_confirmed_column = has_legacy_confirmed_column || name == "confirmed";
    }
    if (!has_active_column) {
        database_->execute(
            "ALTER TABLE users ADD COLUMN is_active INTEGER NOT NULL DEFAULT 0 "
            "CHECK (is_active IN (0, 1))");
        if (has_legacy_confirmed_column) {
            database_->execute("UPDATE users SET is_active = confirmed");
        }
    }
    database_->execute(
        "CREATE UNIQUE INDEX IF NOT EXISTS users_email_unique "
        "ON users(email) WHERE email IS NOT NULL AND email <> ''");
}

std::int64_t UsersRepository::add(const NewUser& user) {
    const auto has_null = [](const std::string& value) {
        return value.find('\0') != std::string::npos;
    };
    if (email_required_ && !user.email) {
        throw std::invalid_argument("User email is required by configuration");
    }
    if (user.username.empty() || user.password_hash.empty() ||
        has_null(user.username) || has_null(user.password_hash) ||
        (user.email && (user.email->empty() || has_null(*user.email)))) {
        throw std::invalid_argument("User username, email, and password hash must be valid non-empty text");
    }

    Parameters parameters{
        user.username,
        user.email ? Value{*user.email} : Value{nullptr},
        user.password_hash,
        std::int64_t{user.is_active ? 1 : 0}
    };
    std::lock_guard<std::mutex> lock(database_->connection_mutex_);
    sqlite3_stmt* statement = prepare(database_->connection_,
        "INSERT INTO users (username, email, password_hash, is_active) VALUES (?, ?, ?, ?)",
        parameters);
    const int result = sqlite3_step(statement);
    if (result != SQLITE_DONE) {
        const int extended_result = sqlite3_extended_errcode(database_->connection_);
        const std::string detail = sqlite_error_message(database_->connection_, result);
        sqlite3_finalize(statement);
        if (extended_result == SQLITE_CONSTRAINT_UNIQUE ||
            extended_result == SQLITE_CONSTRAINT_PRIMARYKEY) {
            throw DuplicateUserError("Username or email is already registered");
        }
        throw std::runtime_error("SQLite user insert failed: " + detail);
    }
    sqlite3_finalize(statement);
    return static_cast<std::int64_t>(sqlite3_last_insert_rowid(database_->connection_));
}

namespace {

std::optional<UserRecord> find_user(SQLiteDatabase& database, std::string_view sql,
                                    const Parameters& parameters) {
    auto rows = database.query(sql, parameters);
    if (rows.empty()) return std::nullopt;
    const Row& row = rows.front();
    const auto email = row.at("email");
    return UserRecord{
        std::get<std::int64_t>(row.at("id")),
        std::get<std::string>(row.at("username")),
        std::holds_alternative<std::nullptr_t>(email)
            ? std::optional<std::string>{}
            : std::optional<std::string>{std::get<std::string>(email)},
        std::get<std::string>(row.at("password_hash")),
        std::get<std::int64_t>(row.at("is_active")) != 0
    };
}

constexpr std::string_view user_columns =
    "SELECT id, username, email, password_hash, is_active FROM users WHERE ";

} // namespace

std::optional<UserRecord> UsersRepository::find_by_id(std::int64_t id) const {
    return find_user(*database_, std::string(user_columns) + "id = ?", {id});
}

std::optional<UserRecord> UsersRepository::find_by_username(std::string_view username) const {
    return find_user(*database_, std::string(user_columns) + "username = ?", {std::string(username)});
}

std::optional<UserRecord> UsersRepository::find_by_email(std::string_view email) const {
    return find_user(*database_, std::string(user_columns) + "email = ?", {std::string(email)});
}

bool UsersRepository::set_active(std::int64_t id, bool is_active) {
    return database_->execute("UPDATE users SET is_active = ? WHERE id = ?",
        {std::int64_t{is_active ? 1 : 0}, id}) > 0;
}

SQLiteDatabase::SQLiteDatabase(const std::string& file_path, bool user_email_required)
    : users(*this, user_email_required) {
    if (file_path.empty()) {
        throw std::invalid_argument("SQLite database path cannot be empty");
    }
    if (file_path.find('\0') != std::string::npos) {
        throw std::invalid_argument("SQLite database path cannot contain a null byte");
    }
    const std::filesystem::path database_path(file_path);
    if (database_path.has_parent_path()) {
        std::error_code directory_error;
        std::filesystem::create_directories(database_path.parent_path(), directory_error);
        if (directory_error) {
            throw std::runtime_error("Cannot create SQLite database directory: " +
                directory_error.message());
        }
    }
    const int result = sqlite3_open_v2(file_path.c_str(), &connection_,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (result != SQLITE_OK) {
        const std::string detail = connection_ ? sqlite3_errmsg(connection_) : sqlite3_errstr(result);
        if (connection_) sqlite3_close(connection_);
        connection_ = nullptr;
        throw std::runtime_error("SQLite open failed: " + detail);
    }

    const int timeout_result = sqlite3_busy_timeout(connection_, 5000);
    if (timeout_result != SQLITE_OK) {
        const std::string detail = sqlite3_errmsg(connection_);
        sqlite3_close(connection_);
        connection_ = nullptr;
        throw std::runtime_error("SQLite configure busy timeout failed: " + detail);
    }
    users.initialize();
}

SQLiteDatabase::~SQLiteDatabase() {
    if (connection_) sqlite3_close_v2(connection_);
}

int SQLiteDatabase::execute(std::string_view sql, const Parameters& parameters) {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    sqlite3_stmt* statement = prepare(connection_, sql, parameters);
    if (sqlite3_column_count(statement) != 0) {
        sqlite3_finalize(statement);
        throw std::invalid_argument("Statement returns rows; use query() instead");
    }
    int result = SQLITE_OK;
    do {
        result = sqlite3_step(statement);
    } while (result == SQLITE_ROW);

    if (result != SQLITE_DONE) {
        const std::string detail = sqlite_error_message(connection_, result);
        sqlite3_finalize(statement);
        throw std::runtime_error("SQLite execute failed: " + detail);
    }

    sqlite3_finalize(statement);
    return sqlite3_changes(connection_);
}

Rows SQLiteDatabase::query(std::string_view sql, const Parameters& parameters) {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    sqlite3_stmt* statement = prepare(connection_, sql, parameters);
    if (sqlite3_column_count(statement) == 0) {
        sqlite3_finalize(statement);
        throw std::invalid_argument("Statement returns no rows; use execute() instead");
    }
    Rows rows;
    int result = SQLITE_OK;
    while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
        Row row;
        const int column_count = sqlite3_column_count(statement);
        for (int column = 0; column < column_count; ++column) {
            const char* name = sqlite3_column_name(statement, column);
            row.insert_or_assign(name ? name : "", read_column(statement, column));
        }
        rows.push_back(std::move(row));
    }

    if (result != SQLITE_DONE) {
        const std::string detail = sqlite_error_message(connection_, result);
        sqlite3_finalize(statement);
        throw std::runtime_error("SQLite query failed: " + detail);
    }

    sqlite3_finalize(statement);
    return rows;
}

std::int64_t SQLiteDatabase::last_insert_row_id() const {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    return static_cast<std::int64_t>(sqlite3_last_insert_rowid(connection_));
}

} // namespace database
