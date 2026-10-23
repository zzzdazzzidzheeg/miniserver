# miniserver

# Общее описание:
Данный проект является минимальным прототипом сервера. Архитектура позволяет добавлять новые маршруты и обработчики. Сервер асинхронный и обрабатывает события в настраиваемом пуле потоков (`server.worker_threads`, по умолчанию 4). Логгер и SQLite-доступ синхронизированы; пользовательские middleware и обработчики должны быть безопасны при одновременных вызовах.


Минимальный HTTPS-сервер на C++17. В приложении зарегистрированы маршруты:

- `GET /api/ping` — проверка доступности сервера.
- `GET /api/echo` — разбирает JSON из тела запроса и возвращает его вместе с методом и временной меткой. Некорректный JSON приводит к ответу `400 Bad Request`.
- `GET /api/info` — версия сервера, время работы и список доступных маршрутов.
- `POST /api/auth/register` — создаёт пользователя в SQLite.
- `POST /api/auth/login` — проверяет пароль и выдаёт bearer access token.
- `GET /api/auth/me` — пример защищённого маршрута, возвращает ID текущего пользователя.
- `POST /api/auth/logout` — отзывает предъявленный bearer-токен.
- `DELETE /api/auth/me` — удаляет текущую учётную запись и её токены.

Ответы обработчиков имеют тип `application/json`. Неизвестный маршрут возвращает `404 Not Found`.

## Структура библиотек

```text
libs/
  http/
    types/
    router/
    middleware/
  auth/
  database/
  logging/
  server/
static/
  index.html
  app.css
  app.js
```

HTTP-типы, маршрутизация и middleware сгруппированы под `http/`. Аутентификация, SQLite, логирование и сетевой сервер остаются отдельными CMake-библиотеками.

## Конфигурация

Параметры приложения, сервера, сессий, логирования, SQLite и аутентификации задаются в `config/server.json`. `application.mode` выбирает `web` или `api-only`; в режиме `web` сервер раздаёт файлы из `application.web_root`, а API-маршруты продолжают работать. По умолчанию режим `api-only`. Адрес и порт, пути к TLS-сертификату и ключу, лимит соединений, тайм-ауты и лимиты HTTP-запроса, уровень логирования, необязательный файл лога, путь к базе данных и провайдер аутентификации также задаются в конфигурации. Относительные пути считаются от каталога конфигурационного файла.

Сборка копирует `config/` и `certs/` в корень каталога сборки. Приложение ищет конфигурацию рядом с исполняемым файлом и на уровень выше, поэтому его можно запустить из `build/`, `build/src/` или другого текущего каталога. Можно передать путь к другому JSON-файлу:

```sh
cd build
./src/miniserver
./src/miniserver /etc/miniserver/server.json
```

Маршруты и middleware регистрируются в коде приложения. Middleware выполняются в порядке регистрации и могут продолжить обработку или сразу вернуть ответ. SIGINT и SIGTERM закрывают активные соединения и останавливают сервер. Ошибка цикла событий завершает приложение с кодом ошибки.

В режиме `web` корневой URL `/` отдаёт `static/index.html`, а CSS и JavaScript загружаются отдельными запросами. CMake копирует каталог `static/` в каталог сборки. В режиме `api-only` fallback для статических файлов не устанавливается.

## Использование в коде

`ServerApplication` объединяет конфигурацию HTTPS-сервера и регистрацию маршрутов. Добавьте обработчики, вызовите `initialize()`, затем `run()`:

```cpp
ServerApplication app(load_application_config("config/server.json"));
app.add_route("GET", "/api/ping", RouteAccess::Public, handlers::PingHandler{});
app.add_route("GET", "/api/echo", RouteAccess::Public, handlers::EchoHandler{});
app.add_route("GET", "/api/info", RouteAccess::Public, handlers::InfoHandler{"1.0"});

app.initialize();
app.run();
```

`initialize()` подготавливает HTTPS-сервер. Вызов `run()` до инициализации или повторный вызов `initialize()` приводит к `std::logic_error`. `stop()` передаёт остановку уже инициализированному серверу.

Для каждого маршрута указывается `RouteAccess::Public` или `RouteAccess::Authenticated`. Защищённые маршруты закрыты по умолчанию на уровне маршрутизации; без установленного аутентификатора они не вызывают обработчик. Подключение аутентификатора описано в [`libs/auth/README.md`](libs/auth/README.md).

### Аутентификация

В `authentication.provider` указывается имя провайдера. Примерный `main` поддерживает значения `none` и `bearer`: первое отключает auth middleware и auth routes, второе включает SQLite account service, bearer-проверку и маршруты login/register/me. В собственном приложении можно зарегистрировать другие именованные провайдеры через `add_authenticator()` и выбрать их этим же параметром. Неизвестное имя без зарегистрированного провайдера приводит к ошибке инициализации. Поле `authentication.challenge` задаёт заголовок `WWW-Authenticate` для отказа в аутентификации. `authentication.access_token_lifetime_seconds` задаёт срок действия access token; по умолчанию — 3600 секунд, допустимый максимум — 30 дней.

Пример приложения регистрирует bearer-провайдер, который извлекает токен из `Authorization: Bearer <token>` и проверяет его в SQLite. Токены хранятся в базе в виде SHA-256 хеша и имеют срок действия из конфига:

```cpp
app.add_authenticator("bearer", auth::make_bearer_authenticator(
    [&account_service](std::string_view token) {
        return account_service.authenticate(token);
    }));
```

Login- и registration-handler создаются через `auth::make_login_handler()` и `auth::make_registration_handler()` и регистрируются как публичные маршруты. Пример `main` использует `auth::SQLiteAccountService`: регистрация принимает `username`, `password` и email, если `database.users.require_email` включён; вход принимает `username` и `password`. Регистрация в примере сразу активирует аккаунт. Если приложению нужно подтверждение по email, телефону или другим способом, оно должно реализовать собственный callback регистрации и активировать пользователя после подтверждения.

```cpp
app.add_route("POST", "/api/auth/login", RouteAccess::Public,
    auth::make_login_handler([&account_service](std::string_view username,
                                                  std::string_view password) {
        return account_service.login(username, password);
    }));

app.add_route("POST", "/api/auth/register", RouteAccess::Public,
    auth::make_registration_handler([&account_service](const nlohmann::json& payload) {
        return account_service.register_account(payload);
    }));
```

Для пользовательского сервиса можно передать собственные callbacks в фабрики handler-ов. SQLite account service хеширует пароли PBKDF2-HMAC-SHA256 с 600 000 итераций и случайной солью; детали параметров хранятся вместе с хешем. Вычисление хеша синхронное и занимает worker-поток на время операции, поэтому при большой нагрузке следует измерить задержки и при необходимости вынести его в отдельный ограниченный пул.

## Требования

- CMake 3.20 или новее
- компилятор с поддержкой C++17
- Boost
- OpenSSL
- SQLite3
- nlohmann-json

## Сборка

Из корня проекта выполните:

```sh
cmake -S . -B build
cmake --build build
```

Исполняемый файл будет создан как `build/src/miniserver`.

## HTTPS-сертификат

В примере конфигурации сервер слушает порт `4433`. Укажите пути к сертификату и закрытому ключу в `config/server.json`:

- `certs/cert.pem`
- `certs/key.pem`

Файлы сертификатов не включаются в репозиторий. Для локального тестирования можно создать самоподписанный сертификат:

```sh
mkdir -p certs
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout certs/key.pem \
  -out certs/cert.pem \
  -days 365
```

Относительные пути к сертификатам, файлу лога и базе данных разрешаются относительно каталога JSON-конфигурации. Раздел `database` необязателен; если он опущен, используется `../data/miniserver.sqlite3`. Приложение создаёт каталог для файла базы при необходимости. Флаг `database.users.require_email` управляет обязательностью email при добавлении пользователя через репозиторий; по умолчанию он выключен. Объект `ServerApplication` предоставляет базу через `database()`, включая репозиторий пользователей `app.database().users`. Пример методов и полей пользователя приведён в [`libs/database/README.md`](libs/database/README.md). Репозиторий хранит только переданный password hash; приложение отвечает за его безопасное создание и проверку.

Запуск из каталога сборки:

```sh
cd build
./src/miniserver
```

## Примеры запросов

Проверка доступности:

```sh
curl --insecure https://localhost:4433/api/ping
```

`--insecure` нужен для локального самоподписанного сертификата; для доверенного сертификата уберите этот параметр.

Echo с JSON-телом:

```sh
curl --insecure \
  -X GET \
  -H 'Content-Type: application/json' \
  -d '{"hello":"world"}' \
  https://localhost:4433/api/echo
```

Информация о сервере:

```sh
curl --insecure https://localhost:4433/api/info
```

Регистрация и вход (примерная конфигурация требует email при регистрации):

```sh
curl --insecure -X POST https://localhost:4433/api/auth/register \
  -H 'Content-Type: application/json' \
  -d '{"username":"alice","email":"alice@example.test","password":"example-password"}'

curl --insecure -X POST https://localhost:4433/api/auth/login \
  -H 'Content-Type: application/json' \
  -d '{"username":"alice","password":"example-password"}'
```

Успешный вход возвращает `access_token`; используйте его для защищённого маршрута в заголовке `Authorization: Bearer <access_token>`.

# ВАЖНО!!!

Сервер принимает любые входящие сообщения. При разработке не забудьте добавить авторизацию для вашихэндпоинтов.
