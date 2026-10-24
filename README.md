# miniserver

`miniserver` — небольшой модульный каркас на C++17 для создания HTTPS-сервисов. Проект развивается как простой фундамент для серверных приложений: он берёт на себя сетевую обработку, маршрутизацию и подключение общих компонентов, а разработчик описывает конфигурацию и поведение своего приложения.

Это не готовая платформа со всеми решениями за пользователя. Маршруты, правила доступа, middleware и нужные сервисы собираются в приложении. Повторно используемый код собран отдельно в `miniserver/`; `src/` содержит демонстрационное приложение. Руководство по подключению и использованию framework продублировано в [`miniserver/README.md`](miniserver/README.md).

## Что входит

- асинхронный HTTPS-сервер на Boost.Asio и Boost.Beast с настраиваемым пулом потоков;
- HTTP-маршрутизатор, типы ответов, обработка файловых ответов и middleware;
- загрузка конфигурации приложения и оболочка `ServerApplication`;
- SQLite-доступ с типизированным репозиторием пользователей;
- подключаемая аутентификация и пример bearer-аутентификатора с SQLite-хранилищем аккаунтов;
- логирование и раздача статических файлов для веб-интерфейса.

Компоненты можно развивать и подключать отдельно. Например, приложение может использовать собственные callbacks для входа и регистрации, не полагаясь на встроенное SQLite-хранилище аккаунтов.

## Структура

```text
miniserver/
  CMakeLists.txt             # библиотеки и установка CMake package
  cmake/                     # конфигурация find_package(miniserver)
  libs/
    application/ # загрузка конфигурации и ServerApplication
    auth/        # аутентификация и общие auth handlers
    database/    # SQLite и репозиторий пользователей
    http/
      middleware/ # middleware HTTP-запросов
      router/     # маршрутизация, ответы и static file handler
      types/      # общие HTTP-типы
    logging/     # потокобезопасный логгер
    server/      # HTTPS-сервер и клиентские сессии
src/
  main.cpp      # сборка конкретного приложения и запуск
  handlers.h    # обработчики примера
static/          # демонстрационная веб-страница
config/          # пример конфигурации
```

`miniserver/libs/application` связывает сервер с маршрутизатором и конфигурацией, но не определяет маршруты конкретного продукта. Их регистрация и прикладные обработчики остаются в `src/` или в коде приложения, использующего framework.

## Сборка и запуск

Нужны CMake 3.20+, компилятор с поддержкой C++17, Boost, OpenSSL, SQLite3 и nlohmann-json.

```sh
cmake -S . -B build
cmake --build build
```

Чтобы установить библиотеки и CMake package:

```sh
cmake --install build --prefix /path/to/miniserver-install
```

Другой CMake-проект может подключить framework так:

```cmake
find_package(miniserver CONFIG REQUIRED)
target_link_libraries(my_service PRIVATE miniserver::framework)
```

## Используемые библиотеки и лицензии

Код `miniserver` распространяется под MIT License (см. [`LICENSE`](LICENSE)). Framework использует следующие внешние библиотеки:

| Библиотека | Для чего используется | Лицензия |
|---|---|---|
| [Boost](https://www.boost.org/) | Asio и Beast: сеть, TLS и HTTP | Boost Software License 1.0 |
| [OpenSSL](https://www.openssl.org/) | TLS и криптографические функции | Apache-2.0 начиная с OpenSSL 3.0; более ранние версии — лицензии OpenSSL/SSLeay |
| [nlohmann/json](https://github.com/nlohmann/json) | Чтение JSON/JSONC конфигурации и JSON-ответы | MIT; в отдельных версиях есть сторонние компоненты с собственными уведомлениями |
| [SQLite3](https://www.sqlite.org/) | Текущий драйвер `miniserver/libs/database` и локальное хранилище аккаунтов; подключается через CMake target `SQLite3::SQLite3` | Код ядра SQLite находится в общественном достоянии |

CMake находит эти зависимости в системе сборки и не включает их исходники в репозиторий. Если распространяете собранные бинарники или упаковываете сами зависимости, проверяйте лицензии и уведомления именно тех версий, которые вошли в пакет. В частности, версия OpenSSL зависит от окружения сборки.

При сборке конфигурация, сертификаты и статические файлы копируются в `build/`. Исполняемый файл: `build/src/miniserver`.

Пример `config/server.jsonc` слушает `127.0.0.1:8443`. Серверу нужны сертификат и соответствующий закрытый ключ, указанные в `server.certificate_file` и `server.private_key_file`. Файлы сертификатов в репозиторий не включаются. Для локальной проверки можно создать самоподписанный сертификат:

```sh
mkdir -p certs
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout certs/key.pem \
  -out certs/cert.pem \
  -days 365
```

Затем запустить сервер:

```sh
cd build
./src/miniserver
```

Можно передать путь к другому конфигурационному файлу:

```sh
./src/miniserver /path/to/server.jsonc
```

Если конфигурация рядом с исполняемым файлом не найдена, приложение создаёт `config/server.jsonc` с пояснениями и завершает запуск. Укажите в нём пути к TLS-сертификату и ключу, затем запустите приложение ещё раз. JSONC поддерживает комментарии `//` и `/* ... */`; остальные значения можно опустить, если подходят встроенные значения по умолчанию. Относительные пути к сертификату, ключу, базе данных, лог-файлу и корню веб-файлов отсчитываются от каталога конфигурационного файла. `SIGINT` и `SIGTERM` останавливают сервер и закрывают активные соединения.

## Конфигурация

Полный пример с комментариями находится в [`config/server.jsonc`](config/server.jsonc). Основные параметры:

- `application.mode`: `api-only` или `web`; пример использует `web`;
- `application.web_root`: каталог статических файлов в режиме `web`;
- `server.address`, `server.port`: адрес и порт прослушивания;
- `server.worker_threads`: число потоков обработки событий, от 1 до 256;
- `server.max_connections`: максимальное число активных соединений;
- `session.*`: тайм-ауты TLS/HTTP и ограничения заголовка и тела запроса;
- `session.handler_timeout_seconds`: максимальное время ожидания завершения асинхронного обработчика;
- `logging.*`: уровень и необязательный файл журнала;
- `middleware.request_logging.enabled`: включение журналирования запросов; по умолчанию `true`;
- `middleware.statistics.enabled`: сбор счётчиков запросов и ответов; по умолчанию `true`;
- `middleware.rate_limit.enabled`: включение лимита запросов; по умолчанию `false`;
- `middleware.rate_limit.key`: `principal-or-ip` группирует запросы прошедшего аутентификацию пользователя по его principal, а остальные — по IP; `ip` всегда считает по IP;
- `middleware.rate_limit.requests` и `window_seconds`: число запросов от одного ключа за временное окно;
- `middleware.rate_limit.max_tracked_clients`: ограничение числа IP/пользовательских ключей в памяти лимитера;
- `database.file`: файл SQLite; каталог создаётся автоматически;
- `database.users.require_email`: требует email при создании учётной записи;
- `registration.auto_activate`: автоматически активирует аккаунты встроенного SQLite registration handler; по умолчанию `true`;
- `authentication.provider`: `none` или `bearer` в текущей сборке примера;
- `authentication.access_token_lifetime_seconds`: срок жизни bearer-токена.

В режиме `web` сервер отдаёт `static/index.html` для `/` и обслуживает CSS/JavaScript и другие файлы из `web_root`. API-маршруты продолжают обрабатываться маршрутизатором. В режиме `api-only` статический fallback отключён.

Middleware, необходимые для корректной работы приложения, подключаются по умолчанию. Необязательные middleware включаются настройками в секции `middleware`: логирование запросов и статистика включены по умолчанию, а rate limit выключен. Статистика считает общее число запросов, число активных запросов, классы HTTP-ответов и отклонённые лимитером запросы. Снимок доступен приложению через `ServerApplication::request_statistics()`; публичного endpoint для метрик пример не создаёт. По умолчанию rate limit объединяет запросы одного аутентифицированного пользователя по `principal.subject`; для запросов без установленного principal используется IP удалённого TCP-соединения. Действительный bearer-токен учитывается и на публичных маршрутах, поэтому пользователь не может обойти лимит, меняя IP. Стратегию можно сменить на `ip`. Лимитер не доверяет переданному клиентом `X-Forwarded-For` и возвращает `429 Too Many Requests` при превышении лимита. Auth middleware включается выбором провайдера в `authentication.provider` (`none` отключает его, `bearer` выбирает bearer-провайдер). Сервер обрабатывает сессии параллельно, поэтому пользовательские обработчики и middleware могут вызываться одновременно и должны безопасно обращаться к своему изменяемому состоянию. Встроенные логгер и SQLite-доступ синхронизированы.

Глобальный лимит применяется к каждому маршруту, у которого не задан собственный лимит. Например, чтобы разрешить до 100 запросов за минуту на пользователя (или на IP для неаутентифицированного клиента), задайте:

```json
"rate_limit": {
  "enabled": true,
  "key": "principal-or-ip",
  "requests": 100,
  "window_seconds": 60
}
```

Отдельным маршрутам можно назначить собственную квоту последним аргументом `add_route()`. Она применяется независимо от `rate_limit.enabled`; глобальный лимит остаётся настройкой по умолчанию для остальных маршрутов:

```cpp
app.add_route("POST", "/api/auth/login", RouteAccess::Public,
    handlers::LoginHandler{}, RouteRateLimit{5, std::chrono::seconds{60}});
```

Квота считается отдельно для каждой пары «маршрут + пользователь» (или «маршрут + IP», если выбрана стратегия `ip`). Значения запросов и длительности окна должны быть положительными.

## Подключение маршрутов и компонентов

`ServerApplication` загружает конфигурацию рядом с исполняемым файлом (для запуска из `build/src` — из `build/config/server.jsonc`). Путь можно передать конструктору или задать через `set_config_path()` до настройки приложения. Конфигурация хранится в объекте приложения и доступна через `config()`:

```cpp
ServerApplication app(argv[0], argc > 1 ? argv[1] : "");

const auto& config = app.config();

app.add_route("GET", "/api/ping", RouteAccess::Public,
    handlers::PingHandler{});

app.add_route("GET", "/api/profile", RouteAccess::Authenticated,
    [](HttpRequestContext& context) {
        return http_utils::make_json_response(200,
            {{"user", context.principal->subject}}, context.request.version());
    });

app.add_middleware(make_request_logging_middleware());
app.initialize();
app.run();
```

Маршруты с `RouteAccess::Authenticated` требуют настроенный аутентификатор. Если middleware аутентификации не подключено или запрос не прошёл проверку, обработчик защищённого маршрута не вызывается. Аутентификация определяет пользователя; прикладные проверки прав на отдельные действия и данные остаются ответственностью обработчика или сервиса.

Для обработчиков, ожидающих асинхронный результат, есть `add_async_route()` с callback `ResponseCompletion`; middleware для таких маршрутов подключается через `add_async_middleware()`. Контекст запроса владеет HTTP-запросом и может безопасно сохраняться до завершения callback. Синхронные и асинхронные пользовательские middleware можно зарегистрировать в одном приложении: синхронная цепочка оборачивает `add_route()`, асинхронная — `add_async_route()`. Каждый стиль применяется только к соответствующим маршрутам. Асинхронный интерфейс сам по себе не делает блокирующий SQLite или вычисления неблокирующими.

После успешной аутентификации обработчик получает `context.principal` — `std::optional<Principal>`. Для `RouteAccess::Authenticated` значение гарантированно заполнено. `Principal::subject` — идентификатор, который вернул настроенный аутентификатор; обычно это стабильный ID пользователя, а не отображаемое имя. Чтобы показать персональное приветствие, используйте subject для поиска профиля в своей базе данных или сервисе:

```cpp
app.add_route("GET", "/api/users/me", RouteAccess::Authenticated,
    [&users](HttpRequestContext& context) -> HttpResponse {
        const auto user_id = std::stoll(context.principal->subject);
        const auto user = users.find_by_id(user_id);
        if (!user) {
            return http_utils::make_error_response(
                404, "User not found", context.request.version());
        }

        return http_utils::make_json_response(200,
            {{"message", "Привет, " + user->username + "!"}},
            context.request.version());
    });
```

У публичного маршрута `context.principal` может содержать пользователя, если аутентификатор распознал предъявленные credentials, но приложение должно учитывать, что значение может отсутствовать. Подробности о настройке и обязанностях аутентификатора приведены в [`auth README`](miniserver/libs/auth/README.md).

Подробности интерфейсов компонентов приведены в README соответствующих библиотек: [`auth`](miniserver/libs/auth/README.md), [`database`](miniserver/libs/database/README.md), [`server`](miniserver/libs/server/README.md).

## Пример приложения

Демонстрационные обработчики и регистрация маршрутов находятся в `src/handlers.h`; `src/main.cpp` настраивает и запускает приложение. В примере зарегистрированы такие маршруты:

| Метод | Путь | Назначение |
|---|---|---|
| `GET` | `/api/ping` | Проверка доступности |
| `GET` | `/api/echo` | Возврат данных запроса и времени |
| `GET` | `/api/async-demo` | Отложенный ответ; демонстрирует async handler и middleware |
| `GET` | `/api/info` | Информация о сервере и список маршрутов |
| `POST` | `/api/auth/register` | Регистрация в SQLite при `provider: bearer` |
| `POST` | `/api/auth/login` | Проверка пароля и выдача токена |
| `GET` | `/api/auth/me` | Пример защищённого маршрута |
| `POST` | `/api/auth/logout` | Отзыв предъявленного токена |
| `DELETE` | `/api/auth/me` | Удаление текущего аккаунта и его токенов |

Auth-маршруты доступны только при включённом bearer-провайдере. В демонстрационной реализации новые аккаунты активируются сразу по умолчанию. При `registration.auto_activate: false` запись создаётся неактивной и регистрация отвечает `202`; приложение должно реализовать свой процесс активации через callback и `users.set_active()`. Это не привязано к email: `database.users.require_email` лишь определяет, обязателен ли адрес при регистрации.

Пример API-запроса:

```sh
curl --insecure https://localhost:8443/api/ping
curl --insecure https://localhost:8443/api/info
```

`--insecure` нужен только для локального самоподписанного сертификата. Для регистрации в конфигурации из репозитория требуется email:

```sh
curl --insecure -X POST https://localhost:8443/api/auth/register \
  -H 'Content-Type: application/json' \
  -d '{"username":"alice","email":"alice@example.test","password":"example-password"}'
```

Для демонстрации асинхронного маршрута запросите `GET /api/async-demo`. В `src/handlers.h` также зарегистрированы sync middleware и async middleware: первый добавляет `X-Demo-Sync-Middleware` к ответам синхронных маршрутов, второй — `X-Demo-Async-Middleware` к ответам асинхронных маршрутов. Их можно проверить по заголовкам ответа.

Вход возвращает bearer-токен. Передавайте его в заголовке `Authorization: Bearer <token>` при запросах к защищённым маршрутам. `/api/auth/logout` отзывает только предъявленный токен.

## Область применения

Проект предназначен для небольших сервисов и как основа для собственных серверных приложений на C++. Он даёт компактные переиспользуемые механизмы и оставляет прикладную модель, правила доступа, валидацию и сценарии регистрации владельцу приложения. Перед эксплуатацией в недоверенной сети нужно настроить адрес прослушивания, TLS, доступ к каждому маршруту, лимиты ресурсов и подходящее хранилище секретов.
