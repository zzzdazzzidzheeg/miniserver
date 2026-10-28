# miniserver framework

`miniserver` — набор переиспользуемых C++17 компонентов для небольших HTTPS-сервисов. Он берёт на себя сетевую обработку, маршрутизацию и подключение общих механизмов, а приложение задаёт свои маршруты, модели данных и правила доступа.

Каталог `miniserver/` содержит только framework. Демонстрационное приложение, конфигурация, тесты и web-страница находятся в корне репозитория и каталогах `src/`, `config/`, `tests/`, `static/`.

## Подключение к CMake-проекту

### Установка и `find_package`

Соберите и установите framework из корня этого репозитория:

```sh
cmake -S . -B build
cmake --build build
cmake --install build --prefix /path/to/miniserver-install
```

В проекте-потребителе найдите установленный пакет и подключите цель:

```cmake
find_package(miniserver CONFIG REQUIRED)
target_link_libraries(my_service PRIVATE miniserver::framework)
```

Публичные заголовки устанавливаются в `include/miniserver/`. Экспортированная CMake-цель передаёт приложению нужные зависимости framework.

### `add_subdirectory`

При разработке можно добавить репозиторий как submodule или локальную копию и включить его CMake-проект:

```cmake
add_subdirectory(path/to/miniserver-repository)
target_link_libraries(my_service PRIVATE miniserver::framework)
```

В `add_subdirectory()` передаётся корень репозитория, так как верхний `CMakeLists.txt` добавляет каталог `miniserver/` и объявляет цели демонстрационного приложения.

## Создание приложения

Минимальный пример создаёт `ServerApplication`, регистрирует маршрут и запускает сервер:

```cpp
#include "server_application.h"
#include "response_utils.h"

int main(int argc, char* argv[]) {
    ServerApplication app(argv[0], argc > 1 ? argv[1] : "");

    app.add_route("GET", "/api/hello", RouteAccess::Public,
        [](HttpRequestContext& context) -> HttpResponse {
            return http_utils::make_json_response(200,
                {{"message", "Hello from my service"}}, context.request.version());
        });

    app.initialize();
    app.run();
}
```

Если путь конфигурации не передан, приложение ищет её рядом с исполняемым файлом. Можно передать путь конструктору, как в примере, либо вызвать `app.set_config_path(path)` до настройки приложения. Конфигурация доступна через `app.config()`; `app.database()` даёт доступ к настроенному SQLite-компоненту.

Для операции, завершающейся позже, используйте `add_async_route()`. `client` здесь — предоставленный приложением асинхронный клиент:

```cpp
app.add_async_route("GET", "/api/remote", RouteAccess::Public,
    [&client](const std::shared_ptr<HttpRequestContext>& context,
              ResponseCompletion complete) {
        client.async_get("/data", [context, complete = std::move(complete)](auto result) mutable {
            if (!result) {
                complete(http_utils::make_error_response(
                    502, "Remote service unavailable", context->request.version()));
                return;
            }
            complete(http_utils::make_json_response(200,
                {{"data", *result}}, context->request.version()));
        });
    });
```

## Маршруты и обработчики

`add_route(method, path, access, handler, rate_limit)` связывает HTTP-метод и путь с обработчиком. Политика доступа обязательна:

- `RouteAccess::Public` — маршрут доступен без успешной аутентификации;
- `RouteAccess::Authenticated` — обработчик вызывается только после успешной аутентификации.

Вместо лямбд можно передавать именованные типы обработчиков с `operator()(HttpRequestContext&)`. Это удобно для самостоятельных компонентов, тестирования и более читаемой регистрации маршрутов. Пример демонстрационных типов и регистрации находится в `src/handlers.h`.

Обработчик получает `HttpRequestContext&`. HTTP-запрос доступен в `context.request`, а аутентифицированный пользователь — в `context.principal`, имеющем тип `std::optional<Principal>`. У `Principal` есть поле `subject`: идентификатор, возвращаемый аутентификатором. На защищённом маршруте principal установлен до вызова обработчика. Обычно `subject` — стабильный ID, а отображаемое имя следует загрузить из пользовательского профиля:

```cpp
app.add_route("GET", "/api/users/me", RouteAccess::Authenticated,
    [&users](HttpRequestContext& context) -> HttpResponse {
        const auto user = users.find_by_id(context.principal->subject);
        if (!user) {
            return http_utils::make_error_response(
                404, "User not found", context.request.version());
        }
        return http_utils::make_json_response(200,
            {{"message", "Hello, " + user->display_name + "!"}},
            context.request.version());
    });
```

На публичном маршруте `principal` может быть заполнен, если credentials были переданы и проверены, но может и отсутствовать.

### Ограничение частоты для маршрута

Последний аргумент `add_route()` необязателен. `RouteRateLimit` задаёт количество запросов и длительность окна для одной пары «маршрут + пользователь» или «маршрут + IP» согласно конфигурации лимитера:

```cpp
app.add_route("POST", "/api/auth/login", RouteAccess::Public,
    LoginHandler{}, RouteRateLimit{5, std::chrono::seconds{60}});
```

Если собственный лимит не указан, применяется глобальный `middleware.rate_limit` из конфигурации, если он включён. Явный лимит маршрута действует независимо от `middleware.rate_limit.enabled`. Количество запросов и окно должны быть положительными.

## Аутентификация

Аутентификацию выбирает `authentication.provider`. Для выбранного провайдера приложение регистрирует соответствующий authenticator через `app.add_authenticator()` до `initialize()`. Встроенный bearer-адаптер разбирает заголовок `Authorization: Bearer ...` и передаёт токен callback-функции приложения для проверки и получения `Principal`.

Framework не навязывает хранилище учётных записей и политику регистрации. Auth-библиотека содержит общие фабрики обработчиков входа, регистрации, выхода и удаления аккаунта; приложение передаёт им callbacks для проверки и хранения данных. Есть также пример реализации на SQLite (`SQLiteAccountService`). Подтверждение регистрации по email или телефону приложение реализует в своём callback.

Встроенный SQLite account service использует `registration.auto_activate` (по умолчанию `true`). Если значение `false`, он создаёт пользователя неактивным и возвращает `202 Accepted`; приложение должно реализовать свой механизм активации и вызвать `database::SQLiteDatabase::users.set_active()`. Требование email задаётся независимо параметром `database.users.require_email`.

Аутентификация устанавливает личность пользователя. Проверки прав на конкретные действия и ресурсы должны выполняться приложением или его сервисами.

## Middleware и статистика

Приложение может добавлять собственные middleware через `app.add_middleware()` до `initialize()`. Роутер сначала выбирает маршрут, затем запускает middleware по порядку регистрации. Все middleware используют callback-интерфейс и одну цепочку для sync- и async-маршрутов. Middleware может читать `context->matched_route`, `context->principal` и запрос, вернуть ответ через `complete` или передать управление через `next`. Пример раннего ответа и изменения ответа находится в [руководстве по middleware](libs/http/middleware/README.md).

Встроенная статистика доступна через `app.request_statistics()`. Она считает запросы, активные запросы, классы HTTP-ответов и отклонения rate limiter. Rate limiter включается настройкой `middleware.rate_limit.enabled`; маршрутные лимиты можно задать через `RouteRateLimit`.

Асинхронный обработчик регистрируется через `add_async_route()`. Он получает `std::shared_ptr<HttpRequestContext>` и `ResponseCompletion`; сохраните копию контекста, если используете его после возврата из обработчика, и вызовите completion ровно один раз. Контекст владеет запросом и остаётся доступен до завершения. Синхронные маршруты продолжают регистрироваться через `add_route()` и автоматически завершаются в общей callback-цепочке. `add_async_middleware()` оставлен как alias для `add_middleware()`. `session.handler_timeout_seconds` ограничивает ожидание ответа от async-обработчика.

## Компоненты

В `libs/` находятся библиотеки framework:

- `application/` — `ServerApplication`, загрузка конфигурации и связывание компонентов;
- `auth/` — authenticator middleware, общие auth handlers и пример SQLite account service;
- `database/` — SQLite-доступ и интерфейс пользователей;
- `http/types/` — HTTP-типы, контекст запроса и метаданные маршрута;
- `http/router/` — router, ответы и обработчик статических файлов;
- `http/middleware/` — middleware журнала запросов и статистики/rate limiting;
- `logging/` — логгер;
- `server/` — HTTPS-сервер, конфигурация и сессии.

Интерфейсы и примеры отдельных компонентов описаны в README внутри каталогов `libs/`.

## Зависимости

Для сборки требуются CMake 3.20+, компилятор C++17, Boost, OpenSSL, SQLite3 и nlohmann-json. Проект использует Boost.Asio/Beast для сети и HTTP, OpenSSL для TLS и криптографии, SQLite3 для локального хранилища и nlohmann-json для конфигурации и JSON.

Код проекта распространяется под MIT License. Подробности о внешних лицензиях и примере запуска находятся в [корневом README](../README.md).
