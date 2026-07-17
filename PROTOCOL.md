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

---

## Расширения OpenSource Communicator (IM)

Следующие форматы **не** являются частью официального клиента Megafon / ITooLabs. Их понимают только клиенты **OpenSource Communicator** (OSC), обнаруженные через `Openping!`. Сообщения идут через обычный `SendIM` / push `[IM]`.

### Обнаружение OSC-пира

| Тело IM | `persist` | Назначение |
|---------|-----------|------------|
| `Openping!` | `false` | При входе в сеть — broadcast всем контактам домена; ответ `Openping!` + авто-реклама цвета/аватарки отправителю. Защита от петли: ~15 с на пару. |

Получатель добавляет peer в локальный список OSC (`user-data.json` → `oscPeers`).

### Цвет аватарки

| Тело IM | `persist` | Назначение |
|---------|-----------|------------|
| `**#RRGGBB**` | `false` | Реклама цвета фона аватарки. Не показывается в чате. |

### Передача аватарки (фото)

| Тело IM | `persist` | Назначение |
|---------|-----------|------------|
| `**fnm=avatar.png;enc=b64;cnt=<BASE64>**` | `false` | PNG 140×140. Лимит ~96 KiB. Не показывается в чате; применяется к аватарке контакта. |

### Передача темы (обои)

| Тело IM | `persist` | Назначение |
|---------|-----------|------------|
| `**fnm=theme.jpg;enc=b64;ui=<0–100>;list=<0–100>;cnt=<BASE64>**` | `true` | JPEG обои 390×620 (уже обрезанные) + затемнение интерфейса (`ui`) и списков (`list`). Лимит ~384 KiB. В чате — уведомление «*Имя* поделился с вами темой» (клик → предпросмотр → «Применить» / «Отменить»). |

### Подтверждение применения темы

| Тело IM | `persist` | Назначение |
|---------|-----------|------------|
| `Themeapplied!` | `true` | Ответ получателя после «Применить» в предпросмотре. В чате у отправителя темы — «*Имя* применил вашу тему». |

### Порядок обмена темой

1. A → B: footer темы (`persist=true`).
2. B: клик по уведомлению → предпросмотр → «Применить» → локально сохраняются обои и ползунки.
3. B → A: `Themeapplied!`.
4. A: уведомление в чате.

Ручная отправка из настроек: «Настройки → Аккаунт → Поделиться темой» (только OSC-пиры, как «Поделиться аватаркой»).
