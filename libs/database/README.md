# Database

Small SQLite interface for opening a database, executing parameterized statements, and reading query results.

The database also exposes a typed users repository as `db.users`. Its table and unique indexes are created when the database opens:

```cpp
database::SQLiteDatabase db("data/app.sqlite3");

const database::NewUser new_user{
    "alice", "alice@example.test",
    password_hasher.hash("correct horse battery staple"), false
};
const auto user_id = db.users.add(new_user);

const auto user = db.users.find_by_username("alice");
if (user && user->confirmed) {
    // Authenticate using the stored password hash.
}

db.users.set_confirmed(user_id, true);
```

`NewUser` accepts a username, optional email, password hash, and confirmation state. `add()` returns the generated integer ID and fails on duplicate usernames or non-empty emails. `find_by_id()`, `find_by_username()`, and `find_by_email()` return `std::optional<UserRecord>`; `set_confirmed()` returns false if the ID does not exist. Lookup text is matched exactly, so applications should normalize usernames/emails consistently before storing and querying them.

Email can be required for repository inserts through `database.users.require_email` in `config/server.json`. The setting defaults to `false`; when enabled, `db.users.add()` throws `std::invalid_argument` if no email is supplied. The example configuration enables it.

The repository deliberately accepts `password_hash`, not a raw password. Password hashing and verification belong to the application; use a password hashing function intended for passwords, with per-password salt and an adjustable work factor. Do not log password values or hashes. Confirmation tokens and access tokens should be stored separately as hashes and should have expiry and revocation handling.

```cpp
#include "sqlite_database.h"

database::SQLiteDatabase db("data/app.sqlite3");

db.execute(
    "CREATE TABLE IF NOT EXISTS notes (id INTEGER PRIMARY KEY, text TEXT NOT NULL)");

db.execute("INSERT INTO notes (text) VALUES (?)", {std::string("hello")});

const auto rows = db.query("SELECT id, text FROM notes WHERE id = ?", {std::int64_t{1}});
if (!rows.empty()) {
    const auto& id = std::get<std::int64_t>(rows.front().at("id"));
    const auto& text = std::get<std::string>(rows.front().at("text"));
}
```

Supported bound values are `nullptr`, `std::int64_t`, `double`, and `std::string`. Query rows map column names to values; SQL `NULL` is represented by `nullptr` in the value variant. Use unique column names (or SQL aliases) in query results. Use bound parameters for data values instead of concatenating them into SQL. Each call accepts one SQL statement; use `execute()` for statements without result columns and `query()` for statements that return columns.

Calls are synchronous. Avoid doing slow database work directly on the server's single event-loop thread; when handlers begin using this component, database work should be moved to a bounded worker pool or the server should run multiple event-loop workers.
