# Middleware

Middleware оборачивает обработку запроса: может проверить его, вернуть ранний ответ или передать запрос дальше и обработать ответ. Все middleware регистрируются через `app.add_middleware()` до `app.initialize()` и выполняются в порядке регистрации.

```cpp
app.add_middleware([](const std::shared_ptr<HttpRequestContext>& context,
                      const MiddlewareNext& next, ResponseCompletion complete) {
    if (context->matched_route && context->matched_route->path == "/api/maintenance") {
        complete(http_utils::make_error_response(
            503, "Service unavailable", context->request.version()));
        return;
    }

    next(context, [complete = std::move(complete)](HttpResponse response) mutable {
        std::visit([](auto& message) {
            message.set("X-Content-Type-Options", "nosniff");
        }, response);
        complete(std::move(response));
    });
});
```

В примере middleware сразу отклоняет запрос к маршруту на обслуживании, а для остальных ответов добавляет защитный заголовок. Роутер выбирает маршрут до вызова middleware, поэтому доступны `context->matched_route`, `context->principal` и исходный запрос. `next` передаёт работу следующему middleware; `complete` завершает обработку. Вызывайте каждый не больше одного раза. Чтобы изменить ответ после обработчика, передайте в `next` обёртку над `complete`.

Цепочка одна для `add_route()` и `add_async_route()`. Обычный синхронный обработчик выполняется в конце цепочки и сразу передаёт ответ в completion; асинхронный — вызывает completion по готовности. Middleware может выполнять синхронную проверку до `next`, но не должно ждать блокирующую операцию ради async-ответа.

Сигнатура использует shared-контекст, поэтому запрос остаётся доступным до завершения асинхронной обработки. Сервер вызывает обработчики параллельно; состояние, захваченное middleware, должно быть потокобезопасным. `add_async_middleware()` сохранён как совместимый alias для `add_middleware()`.

Встроенные middleware журнала запросов, статистики, аутентификации и ограничения частоты используют ту же цепочку. Для аутентификации используйте компонент из `libs/auth/`, а не реализуйте разбор credentials вручную.
