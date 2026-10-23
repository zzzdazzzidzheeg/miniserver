# miniserver

# Общее описание:
Данный проект является минимальным прототипом сервера. Архитектура позволяет относительно легко добавлять новые маршруты запросов и их обработку. Сам сервер являяется асинхронным, логгер внутри поддерживает многопоточность, так что если какие-то задачи потребуют разбиения на несколько потоков, в каждом из них будет безопасно вызывать логгер. Хорошо подходит для проектирования домашнего сервера


Минимальный HTTPS-сервер на C++17. В приложении зарегистрированы три маршрута:

- `GET /api/ping` — проверка доступности сервера.
- `GET /api/echo` — разбирает JSON из тела запроса и возвращает его вместе с методом и временной меткой. Некорректный JSON приводит к ответу `400 Bad Request`.
- `GET /api/info` — версия сервера, время работы и список доступных маршрутов.

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
```

HTTP-типы, маршрутизация и middleware сгруппированы под `http/`. Аутентификация, SQLite, логирование и сетевой сервер остаются отдельными CMake-библиотеками.

## Конфигурация

Параметры сервера, сессий, логирования, SQLite и аутентификации задаются в `config/server.json`. Файл содержит адрес и порт, пути к TLS-сертификату и ключу, лимит соединений, тайм-ауты и лимиты HTTP-запроса, уровень логирования, необязательный файл лога, путь к базе данных и выбранный провайдер аутентификации. Относительные пути считаются от каталога конфигурационного файла.

Сборка копирует `config/` и `certs/` в корень каталога сборки. Приложение ищет конфигурацию рядом с исполняемым файлом и на уровень выше, поэтому его можно запустить из `build/`, `build/src/` или другого текущего каталога. Можно передать путь к другому JSON-файлу:

```sh
cd build
./src/miniserver
./src/miniserver /etc/miniserver/server.json
```

Маршруты и middleware регистрируются в коде приложения. Middleware выполняются в порядке регистрации и могут продолжить обработку или сразу вернуть ответ. SIGINT и SIGTERM закрывают активные соединения и останавливают сервер. Ошибка цикла событий завершает приложение с кодом ошибки.

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

В `authentication.provider` указывается имя зарегистрированного провайдера. Значение `none` (по умолчанию) отключает middleware аутентификации. Если выбран другой провайдер, приложение должно зарегистрировать его до `initialize()`; иначе запуск завершится ошибкой. Поле `authentication.challenge` задаёт заголовок `WWW-Authenticate` для отказа в аутентификации.

Встроенный bearer-адаптер извлекает токен из `Authorization: Bearer <token>`, но не хранит пользователей и сам по себе не считает токен действительным. Приложение передаёт callback, который проверяет токен, его срок действия и отзыв:

```cpp
app.add_authenticator("bearer", auth::make_bearer_authenticator(
    [&account_service](std::string_view token) {
        return account_service.validate_access_token(token);
    }));
```

Login- и registration-handler создаются через `auth::make_login_handler()` и `auth::make_registration_handler()`. Их нужно явно зарегистрировать как публичные маршруты. Login-handler принимает JSON с непустыми `username` и `password`, а callback приложения проверяет пароль и выдаёт токен. Registration-handler передаёт весь JSON callback приложения, поэтому приложение определяет поля регистрации, хранение пользователя и необходимость подтверждения по email, телефону или другим способом.

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

Эти фабрики не создают схему пользователей, не хешируют пароли, не выдают токены и не отправляют сообщения подтверждения: это ответственность переданных приложением сервисов. В демонстрационном `main` провайдер остаётся `none`, а маршруты login/register не подключены, потому что в проекте пока нет реализации хранилища учётных записей и выдачи токенов.

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

# ВАЖНО!!!

Сервер принимает любые входящие сообщения. При разработке не забудьте добавить авторизацию для вашихэндпоинтов.
