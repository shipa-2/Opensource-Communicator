# ITooLabs Communicator Protocol (reverse-engineered)

Документация получена из `communicator-megafon` v3.13.11 (`com.itoolabs.communicator`).

## Транспорт

- **WebSocket** (`wss://<auth-domain>/ws/?_domain=<domain>` для Megafon)
- Альтернатива: `wss://<domain>/ws`
- После handshake клиент получает `sid` (session id)

## Handshake

Клиент отправляет:

```json
{
  "task": "Main",
  "useragent": "...",
  "params": {
    "apiVersion": 2,
    "platform": "Linux",
    "appId": "Communicator"
  }
}
```

Сервер отвечает: `{"status":"ok","sid":"..."}`

## Формат сообщений

Все сообщения используют seq/ack для надёжной доставки:

```json
{
  "seq": 1,
  "ack": 0,
  "payload": { "": "login", "username": "...", "password": "..." }
}
```

- Пустой ключ `""` содержит команду или ответ
- `What: "request"` / `What: "response"` — RPC-запросы с `id`
- Keepalive: `{"": "noop"}` каждые 57 секунд
- Выход: `{"": "bye"}`

## Авторизация

```json
{ "": "login", "username": "user@domain", "password": "...", "partner": "megafon", "os": "Linux", "appVersion": "..." }
```

## Основные команды

| Команда | Назначение |
|---------|------------|
| `Bind` | Привязка SIP/телефонии |
| `BindIM` | Подписка на мгновенные сообщения |
| `subscribetoaddressbook` | Адресная книга |
| `listaccounts` | Список абонентов домена |
| `ProvisionCall` | Подготовка исходящего вызова |
| `StartCall` | Начало вызова с SDP |
| `AcceptCall` | Принятие входящего |
| `RejectCall` / `CancelCall` / `DisconnectCall` | Управление вызовом |
| `AckAccept` / `UpdateCall` / `AcceptUpdate` | WebRTC renegotiation |
| `Transfer` | Перевод вызова |
| `SetPresence` | Статус присутствия |
| `getsms` / `sendsms` | SMS |
| `gethistory` | История звонков |

## Медиа

- Сигнализация через WebSocket API
- Медиа — **WebRTC** (SDP offer/answer через `StartCall`/`AcceptCall`/`UpdateCall`)
- Оригинал использует `RTCPeerConnection` в Electron

## Безопасность (проблемы оригинала)

- `NSAllowsArbitraryLoads: true` в macOS Info.plist
- Пароли в открытом виде при login
- Телеметрия Amplitude и Sentry
- Статистика отправляется на `communicatorstats.itoolabs.com`

OpenSource Communicator не отправляет телеметрию и хранит настройки локально через QSettings.
