# Web Application Layer

Этот модуль отвечает за HTTP-маршрутизацию, парсинг URL и формирование HTTP-ответов для веб-части сервера. Он используется как слой между сетевым стеком (`server`/`session`) и бизнес-логикой приложения.

## Состав папки

В этой папке находятся три основных компонента:

- `router.h` / `router.cpp` — маршрутизатор HTTP-запросов;
- `url_parser.h` / `url_parser.cpp` — разбор целевого URL запроса;
- `response_utils.h` — helper-функции для создания ответов разных типов.

## 1. Router

### Назначение

`Router` хранит соответствия между HTTP-методом, путём и обработчиком и выбирает нужный обработчик по входящему запросу.

### Тип обработчика

```cpp
using HttpResponse = http::response<http::string_body>;
using Handler = std::function<HttpResponse(const http::request<http::string_body>&)>;
```

То есть обработчик принимает `http::request<http::string_body>` и возвращает `http::response<http::string_body>`.

### API

```cpp
class Router : public std::enable_shared_from_this<Router> {
public:
    Router();
    void add_route(std::string method, std::string path, Handler handler);
    HttpResponse route(const http::request<http::string_body>& req) const;
    void setup_router();
};
```

### Основные методы

#### `add_route(method, path, handler)`

Добавляет маршрут в словарь. Ключ формируется по схеме:

```cpp
"METHOD PATH"
```

Например:

```cpp
router.add_route("GET", "/api/status", handler);
```

#### `route(req)`

Основная логика маршрутизатора:

1. Разбирает URL запроса через `parse_url`.
2. Нормализует метод и путь.
3. Ищет обработчик по ключу `METHOD PATH`.
4. Если маршрут найден, вызывает обработчик.
5. Если нет — возвращает `404 Not Found` через `default_handler_`.

### Обработчик по умолчанию

Если путь не найден, возвращается ответ:

- статус: `404`
- тело: `Resource not found: ...`

Если во время выполнения обработчика возникает исключение, маршрутизатор возвращает:

- статус: `500`
- JSON-ошибка через `make_error_response(...)`

### Пример использования

```cpp
Router router;

router.add_route("GET", "/health", [](const auto& req) {
    return http_utils::make_json_response(
        200,
        nlohmann::json{{"status", "ok"}},
        req.version()
    );
});

auto response = router.route(request);
```

---

## 2. URL Parser

### Назначение

`url_parser` разбирает raw request target и возвращает структурированные данные о URL.

### Структура `ParsedUrl`

```cpp
struct ParsedUrl {
    std::vector<std::string> path_segments;
    std::unordered_map<std::string, std::string> query_params;
    bool valid = true;
    bool is_asterisk = false;
    std::string error;
};
```

Поле `path_segments` хранит путь без ведущего слэша в виде частей, например:

- `/api/users` → `["api", "users"]`
- `/` → пустой список

`query_params` хранит параметры запроса вида `key=value`.

### Поддерживаемые форматы

Парсер обрабатывает:

- origin-form, например `/api/status`;
- absolute-form, например `http://example.com/api?x=1`;
- asterisk-form: `*`.

### Валидация

Парсер проверяет:

- пустой target;
- слишком длинный URL;
- управляющие символы в URL;
- недопустимые percent-escape последовательности;
- сегменты вида `.` и `..`;
- слишком длинные query-параметры или цепочки путей.

Если URL некорректен, возвращается:

```cpp
ParsedUrl result;
result.valid = false;
result.error = "...";
```

### Функции

```cpp
std::string decode_percent_encoding(std::string_view encoded_text);
ParsedUrl parse_url(std::string_view raw_request_url);
```

### Пример

```cpp
auto parsed = parse_url("/api/users?id=42&name=John+Doe");

// parsed.path_segments == ["api", "users"]
// parsed.query_params["id"] == "42"
// parsed.query_params["name"] == "John Doe"
```

---

## 3. Response Utils

### Назначение

`response_utils.h` содержит фабричные функции для создания типовых HTTP-ответов с уже выставленными заголовками.

### Доступные функции

#### `make_text_response(status, text, version)`

Создаёт текстовый ответ:

```cpp
http::response<http::string_body> res = http_utils::make_text_response(
    200,
    "OK",
    11
);
```

Устанавливает заголовки:

- `Server: miniserver/1.0`
- `Content-Type: text/plain`
- `X-Content-Type-Options: nosniff`
- `Content-Security-Policy: default-src 'none'`

#### `make_json_response(status, json_data, version)`

Создаёт JSON-ответ.

```cpp
nlohmann::json payload = { {"status", "ok"} };
auto res = http_utils::make_json_response(200, payload, 11);
```

#### `make_file_response(status, file_path, version)`

Создаёт ответ с содержимым файла через `http::file_body`.

```cpp
auto res = http_utils::make_file_response(
    200,
    "static/index.html",
    11
);
```

#### `make_empty_response(status, version)`

Создаёт пустой ответ без тела, например для `204 No Content`.

#### `make_error_response(status, message, version)`

Создаёт стандартный JSON-ответ ошибки:

```json
{
  "error": true,
  "status": 404,
  "message": "..."
}
```

---

## Взаимодействие компонентов

Типичный поток обработки выглядит так:

1. `Session` получает HTTP-запрос.
2. В `handle_request` вызывается `request_handler_`.
3. Приложение или слой маршрутизации вызывает `Router::route(req)`.
4. `Router` раскладывает URL через `parse_url()`.
5. Вызванный обработчик формирует ответ через `http_utils::make_*_response(...)`.
6. Ответ отправляется клиенту.

---

## Пример маршрутизатора

```cpp
#include "router.h"
#include "response_utils.h"

int main() {
    Router router;

    router.add_route("GET", "/health", [](const auto& req) {
        return http_utils::make_json_response(
            200,
            nlohmann::json{{"status", "ok"}},
            req.version()
        );
    });

    router.add_route("GET", "/users", [](const auto& req) {
        return http_utils::make_text_response(
            200,
            "users list",
            req.version()
        );
    });

    // далее запрос передаётся в router.route(req)
}
```

---

## Рекомендации

- маршруты лучше регистрировать централизованно в `setup_router()`;
- используйте `make_json_response()` для API-эндпоинтов;
- проверяйте `parsed_url.valid` перед обработкой пути;
- не передавайте невалидные строки в `Router` без предварительной проверки;
- для статических файлов используйте `make_file_response()` и типы MIME.

## Итог

Модуль `webapp` служит HTTP-слоем приложения: он маршрутизирует запросы, разбирает URL, валидирует входные параметры и формирует корректные ответы. Это делает его удобной прослойкой между сетевым сервером и логикой приложения.
