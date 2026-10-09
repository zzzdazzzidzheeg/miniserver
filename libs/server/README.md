# HTTP Server

Этот модуль реализует HTTPS-сервер на базе Boost.Asio и Boost.Beast. Он принимает входящие соединения, выполняет TLS-рукопожатие, читает HTTP-запросы, вызывает пользовательский обработчик и отправляет HTTP-ответы.

## Состав папки

В этой папке находятся два основных компонента:

- `server.h` / `server.cpp` — серверный цикл приёма соединений и управление жизненным циклом;
- `session.h` / `session.cpp` — отдельная сессия для каждого клиента, обработка HTTP-запроса и TLS-соединения.

## Основные типы

В модуле объявлены типы для HTTP-запросов и ответов:

```cpp
using HttpRequest = http::request<http::string_body>;
using HttpResponse = http::response<http::string_body>;
using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;
```

`RequestHandler` — это функция, которая принимает HTTP-запрос и возвращает HTTP-ответ. Именно она является точкой входа для бизнес-логики приложения.

## Класс `Server`

```cpp
class Server {
public:
    Server(unsigned short port,
        const std::string& cert_file,
        const std::string& key_file,
        RequestHandler handler);
    void run();
    void stop();
};
```

### Назначение

`Server` отвечает за:

- открытие TCP-сокета на заданном порту;
- настройку SSL/TLS-контекста;
- приём новых входящих соединений;
- ограничение количества одновременно активных соединений;
- создание `Session` для каждого клиента;
- запуск и остановку `io_context`.

### Конструктор

```cpp
Server server(
    443,
    "certs/server.crt",
    "certs/server.key",
    handler
);
```

Параметры:

- `port` — порт, на котором будет слушать сервер;
- `cert_file` — путь к цепочке сертификатов;
- `key_file` — путь к приватному ключу сертификата;
- `handler` — функция, обрабатывающая HTTP-запрос.

### Жизненный цикл

```cpp
Server server(...);
server.run();
```

`run()` запускает цикл обработки событий `io_context_`, после чего сервер начинает принимать соединения и обрабатывать их асинхронно.

Для остановки сервера используется:

```cpp
server.stop();
```

Метод `stop()` отменяет приём новых соединений, закрывает acceptor и останавливает event loop.

## Внутреннее поведение `Server`

### SSL/TLS

Сервер работает через TLS 1.3:

```cpp
ssl_context_(boost::asio::ssl::context::tlsv13)
```

Перед запуском он загружает:

- цепочку сертификатов через `use_certificate_chain_file`;
- приватный ключ через `use_private_key_file`.

### Ограничение соединений

Сервер поддерживает максимум 1024 активных соединений:

```cpp
static constexpr std::size_t max_connections_ = 1024;
```

Если лимит достигнут, новый сокет закрывается и логируется предупреждение.

### Логирование

Сервер пишет диагностические сообщения через логгер:

- при инициализации SSL-контекста;
- при ожидании новых соединений;
- при ошибках приёма;
- при остановке сервера;
- при достижении лимита соединений.

## Класс `Session`

```cpp
class Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(asio::ssl::stream<asio::ip::tcp::socket> socket,
        RequestHandler request_handler,
        std::function<void()> on_close);
    ~Session();
    void start();
};
```

### Назначение

`Session` представляет одно соединение клиента. Она:

- выполняет TLS-рукопожатие;
- читает HTTP-запрос;
- вызывает `request_handler_`;
- отправляет ответ клиенту;
- управляет тайм-аутами;
- корректно закрывает соединение.

### Этапы работы

1. Создаётся объект `Session` вместе с SSL-сокетом.
2. Вызывается `start()`, после чего начинается рукопожатие.
3. После успешного handshake вызывается `do_read()`.
4. HTTP-запрос парсится через `http::request_parser`.
5. Запрос передаётся в `request_handler_`.
6. Ответ отправляется через `send_response(...)`.
7. Затем выполняется `do_shutdown()`.

## Тайм-ауты и ограничения

У `Session` есть несколько защитных механизмов:

- handshake timeout: 10 секунд;
- read timeout: 15 секунд;
- write timeout: 15 секунд;
- header limit: 8192 байта;
- body limit: 1 MB.

Если тело запроса слишком большое, сервер возвращает ответ `413 Payload Too Large`.

## Обработка ошибок

При ошибках сессия логирует их и закрывает соединение. В случае исключения при обработке запроса сервер возвращает стандартный ответ `500 Internal Server Error`.

## Пример обработки запроса

```cpp
HttpResponse handler(const HttpRequest& req) {
    HttpResponse res{http::status::ok, req.version()};
    res.set(http::field::content_type, "application/json");
    res.body() = R"({"status":"ok"})";
    res.prepare_payload();
    return res;
}
```

Использование:

```cpp
Server server(8080, "certs/server.crt", "certs/server.key", handler);
server.run();
```

## Пример полного использования

```cpp
#include "server.h"
#include "logger.h"

HttpResponse handle_request(const HttpRequest& request) {
    HttpResponse response{http::status::ok, request.version()};
    response.set(http::field::content_type, "text/plain");
    response.body() = "Hello from server";
    response.prepare_payload();
    return response;
}

int main() {
    log_set_level(LogLevel::Info);
    log_set_file("logs/server.log");

    Server server(443, "certs/server.crt", "certs/server.key", handle_request);
    server.run();
    return 0;
}
```

## Рекомендации

- для production используйте корректные TLS-сертификаты;
- логируйте входящие запросы и ошибки на уровне `Info`/`Error`;
- ограничивайте размер тела запроса, чтобы избежать перегрузки памяти;
- обрабатывайте HTTP-ошибки явно в `RequestHandler`;
- не держите слишком много долгоживущих соединений без тайм-аутов.

## Итог

Модуль в этой папке предоставляет базовый, но довольно надёжный HTTPS-сервер для приложений на C++. Он удобен для маршрутизации HTTP-запросов, TLS-обработки и запуска асинхронного сервиса с минимальной дополнительной логикой.
