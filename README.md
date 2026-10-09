# miniserver

Минимальный HTTPS-сервер на C++17. В приложении зарегистрированы три маршрута:

- `GET /api/ping` — проверка доступности сервера.
- `GET /api/echo` — разбирает JSON из тела запроса и возвращает его вместе с методом и временной меткой. Некорректный JSON приводит к ответу `400 Bad Request`.
- `GET /api/info` — версия сервера, время работы и список доступных маршрутов.

Ответы обработчиков имеют тип `application/json`. Неизвестный маршрут возвращает `404 Not Found`.

## Требования

- CMake 3.20 или новее
- компилятор с поддержкой C++17
- Boost
- OpenSSL
- nlohmann-json

## Сборка

Из корня проекта выполните:

```sh
cmake -S . -B build
cmake --build build
```

Исполняемый файл будет создан как `build/src/miniserver`.

## HTTPS-сертификат

Сервер принимает TLS-соединения на порту `4433`. По умолчанию CMake ищет сертификат и закрытый ключ по путям:

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

Если сертификат и ключ лежат в другом месте, задайте пути при конфигурации:

```sh
cmake -S . -B build \
  -DSERVER_CERT_SOURCE=/path/to/cert.pem \
  -DSERVER_KEY_SOURCE=/path/to/key.pem
cmake --build build
```

Без доступных файлов сертификата и ключа проект может сконфигурироваться и собраться, но сервер не сможет запуститься: TLS настраивается при старте приложения.

Запуск из корня проекта:

```sh
./build/src/miniserver
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
