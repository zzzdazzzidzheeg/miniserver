# Database

Small SQLite interface for opening a database, executing parameterized statements, and reading query results.

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
