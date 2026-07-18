# OpenSource Communicator Server

Самостоятельный сигнализационный сервер для клиента **OpenSource Communicator** (Qt6). Реализует WebSocket-протокол, совместимый с ITooLabs/Megafon PBX.

**Стек:** C++17, Qt6 (WebSockets, SQL, Network), PostgreSQL, CMake

---

## Быстрый старт

### Сборка

```bash
cd server
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Зависимости

- Qt6: Core, WebSockets, Network, Sql
- PostgreSQL (libpq)
- OpenSSL
- CMake ≥ 3.16
- C++17 компилятор

**Arch Linux:**
```bash
sudo pacman -S qt6-base qt6-websockets postgresql cmake gcc
```

### Запуск

```bash
# Минимальный (без БД, dev-режим)
./build/communicator-server

# С конфигом
./build/communicator-server --config server.conf

# Демо-режим
./build/communicator-server --demo --port 8443

# С partner-фильтром и видео
./build/communicator-server --partner megafon --allowvideo --port 8443
```

---

## Флаги командной строки

| Флаг | Описание |
|------|----------|
| `-h, --help` | Справка по всем флагам |
| `-v, --version` | Версия сервера |
| `--config <path>` | Путь к JSON-конфигу (по умолчанию `/etc/communicator-server/server.conf`) |
| `--ip <address>` | IP для бинда (по умолчанию `0.0.0.0`) |
| `--port <number>` | Порт (по умолчанию `8443`) |
| `--partner <name>` | Фильтр по partner. Отклоняет клиентов с другим partner |
| `--demo` | Только `demo`/`demo` вход. Остальные логины отклоняются |
| `--allowvideo` | Включает поддержку видеозвонков. Рассылается клиентам через `getcommunicatorsettings` |
| `--bigmessages` | Увеличенный лимит сообщений (для передачи файлов) |
| `--oncall` | Разрешает статус «говорит по телефону» |
| `--servercontacts` | Серверная адресная книга (личные контакты) |
| `--newuser` | Интерактивное создание пользователя в БД (логин, имя, ext, телефон, пароль, роль) |
| `--deluser <login>` | Удаление пользователя (с подтверждением) |
| `--listusers` | Список всех пользователей в БД |

---

## Конфигурация

Файл `server.conf` (JSON):

```json
{
    "server": {
        "host": "0.0.0.0",
        "port": 8443,
        "ssl_cert": "",
        "ssl_key": ""
    },
    "database": {
        "host": "localhost",
        "port": 5432,
        "name": "communicator",
        "user": "communicator",
        "password": ""
    },
    "auth": {
        "session_timeout_sec": 3600,
        "max_sessions_per_user": 5
    },
    "logging": {
        "level": "info"
    }
}
```

### TLS

Для HTTPS/WSS:

```bash
# Self-signed (для разработки)
openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem \
    -days 3650 -nodes -subj "/CN=localhost"

# В конфиге:
"ssl_cert": "/path/to/cert.pem",
"ssl_key": "/path/to/key.pem"
```

### PostgreSQL

```bash
# Создание БД и пользователя
sudo -u postgres psql -h 127.0.0.1 -c "CREATE USER communicator WITH PASSWORD 'communicator';"
sudo -u postgres psql -h 127.0.0.1 -c "CREATE DATABASE communicator OWNER communicator;"
```

Схема создаётся автоматически при первом запуске (auto-migration, ALTER TABLE IF NOT EXISTS).

Если PostgreSQL недоступен, сервер работает **без persistence** (все данные теряются при перезапуске). При dev-входе (пароль `demo`) аккаунт создаётся автоматически без БД.

---

## Управление пользователями

```bash
# Создать пользователя
./build/communicator-server --newuser
# Login: ivan@localhost
# Display name: Иван Иванов
# Internal extension (e.g. 702): 702
# Mobile phone (e.g. +7...): +79001234567
# Password: secret
# Domain (leave empty to extract from login): localhost
# Role: 1) User / 2) Admin / 3) RestrictedUser

# Список пользователей
./build/communicator-server --listusers
#  LOGIN                    DISPLAY NAME         DOMAIN          ROLE
#  ivan@localhost           Иван Иванов          localhost       User
#  shipa@localhost          Змей Шипа            localhost       Admin

# Удалить пользователя
./build/communicator-server --deluser ivan@localhost
# Delete user 'ivan@localhost'? [y/N]: y
```

---

## Архитектура

```
┌─────────────────────────────────────────────────────────┐
│                    communicator-server                    │
│                                                          │
│  ┌──────────┐  ┌──────────────┐  ┌───────────────────┐  │
│  │ WsServer │  │  AuthManager │  │  SessionManager   │  │
│  │ (Qt6 WS) │──│ (SHA-256)    │──│ (sid → session)   │  │
│  └────┬─────┘  └──────────────┘  └───────────────────┘  │
│       │                                                  │
│  ┌────▼─────────────────────────────────────────────┐   │
│  │              CommandDispatcher                     │   │
│  │  (routes "" command key → handler)                │   │
│  └──┬────┬────┬────┬────┬────┬────┬────┬────┬──────┘   │
│     │    │    │    │    │    │    │    │    │            │
│  ┌──▼──┐┌─▼──┐┌▼───┐┌▼──┐┌▼───┐┌▼──┐┌▼──┐┌▼───┐       │
│  │Auth ││IM  ││Call││AB ││Hist││SMS││Pres││Conf│       │
│  └──┬──┘└─┬──┘└┬───┘└┬──┘└┬───┘└┬──┘└┬──┘└┬───┘       │
│     │     │    │     │    │     │    │    │            │
│  ┌──▼─────▼────▼─────▼────▼─────▼────▼────▼────────┐  │
│  │              PostgreSQL (Qt6 SQL)                │  │
│  └─────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

### Модули

| Модуль | Файлы | Описание |
|--------|-------|----------|
| **WsServer** | `network/WsServer.*` | QWebSocketServer, TLS/Plain |
| **WsSession** | `network/WsSession.*` | seq/ack фрейминг, hello handshake, keepalive, bye |
| **SessionManager** | `session/SessionManager.*` | sid→session map, cleanup, multi-device, match по user part |
| **UserSession** | `session/UserSession.*` | login, domain, bound services, presence |
| **AuthManager** | `auth/AuthManager.*` | SHA-256 хэши, demo-режим, partner-фильтр |
| **CommandDispatcher** | `protocol/CommandDispatcher.*` | RPC-маршрутизация по ключу `""`, unwrap двойного `""` |
| **ImManager** | `im/ImManager.*` | SendIM, BindIM, loadimhistory, [IM] push, доставка получателю |
| **PresenceManager** | `im/PresenceManager.*` | SetPresence, [PRESENCE] broadcast, initial presence |
| **CallManager** | `calls/CallManager.*` | StartCall→incomingCall→AcceptCall→accepted |
| **AddressBookManager** | `addressbook/AddressBookManager.*` | listaccounts (accList), CRUD contacts |
| **HistoryManager** | `history/HistoryManager.*` | gethistory, notes |
| **SmsManager** | `sms/SmsManager.*` | getsmschannels, sendsms |
| **ConferenceManager** | `conference/ConferenceManager.*` | conference commands (stubs) |
| **Database** | `db/Database.*` | PostgreSQL, schema, auto-migration, imHistoryForUser |
| **Config** | `server/Config.*` | JSON config reader |
| **Server** | `server/Server.*` | Оркестратор, wiring модулей |

---

## Протокол (совместимость с клиентом)

### Формат сообщений

Все сообщения используют **seq/ack** для надёжной доставки:

```json
{
  "seq": 1,
  "ack": 0,
  "payload": { ... }
}
```

### Handshake

Клиент отправляет:
```json
{"task":"Main","useragent":"...","params":{"apiVersion":2,"platform":"Linux","appId":"Communicator"}}
```

Сервер отвечает:
```json
{"status":"ok","sid":"<session-id>"}
```

### RPC-запросы

Клиент оборачивает команду в два уровня `""`:
```json
{
  "What": "request",
  "id": 0,
  "": {
    "": "login",
    "username": "user@domain",
    "password": "pass"
  }
}
```

Сервер отвечает (Megafon-обёртка — все поля внутри `""`):
```json
{
  "What": "response",
  "id": 0,
  "": {
    "userRole": "Admin",
    "partner": "opensource",
    "domain": "localhost",
    "userId": 1,
    "displayName": "Иван Иванов"
  }
}
```

### Поддерживаемые команды

#### Авторизация

| Команда | Описание | Ответ |
|---------|----------|-------|
| `login` | Авторизация | `userRole`, `partner`, `domain`, `userId`, `displayName` |
| `bye` | Выход | `bye` |

#### Привязка

| Команда | Описание | Ответ |
|---------|----------|-------|
| `Bind` | Привязка SIP/телефонии | `response: "ok"` |
| `BindIM` | Подписка на IM | `response: "ok"` + `[IM_CONTACTS]` |

#### Присутствие

| Команда | Описание | Ответ |
|---------|----------|-------|
| `SetPresence` | Статус присутствия | `response: "ok"` + initial presence + broadcast `[PRESENCE]` |

При `SetPresence("online")` сервер:
1. Отправляет текущий статус всех онлайн-пользователей в домене новому пользователю
2. Рассылает статус нового пользователя всем остальным

Ключи в `batch` — **логин без `@domain`** (как ожидает клиент):
```json
{"What":"[PRESENCE]","batch":{"ivan":{"voice":"online","im":{"status":"online"}}}}
```

#### Мгновенные сообщения

| Команда | Описание | Ответ |
|---------|----------|-------|
| `SendIM` | Отправка IM | `response: {id: N}` + `[IM]` push **получателю** |
| `loadimhistory` | История IM | пустой RPC-ответ + N× `[IM_HIST]` push (по одному на каждого пира) |

**Доставка IM:** Сервер ищет сессию получателя по user part (до `@`), доставляет `[IM]` push всем его IM-сессиям.

**`from` поле:** Формируется как `login@domain` (без удвоения `@`).

**История:** При `loadimhistory` без `chatId` (старт клиента):
1. Сервер загружает все сообщения пользователя из БД
2. Фильтрует эфемерные (Openping!, аватар, цвета) сервер-side
3. Группирует по peer (`to` поле)
4. Отправляет отдельный `[IM_HIST]` push для каждого пира
5. Клиент обрабатывает каждый push → `historyLoaded(peer)` → UI обновляет диалог

**chat_id формат:** `user1:user2` (оба — без `@domain`, отсортированы алфавитно). Старые записи с нестандартным форматом поддерживаются через LIKE-запрос.

#### Контакты

| Команда | Описание | Ответ |
|---------|----------|-------|
| `listaccounts` | Контакты домена | `accList` (объект, не массив) |
| `subscribetoaddressbook` | Подписка на адресную книгу | `[CONTACTS]` push |
| `createcontact` | Создать контакт | `response: {contactId}` |
| `deletecontact` | Удалить контакт | `response: "ok"` |
| `uploadcontacts` | Массовая загрузка | `response: "ok"` |

**Формат `listaccounts`** (Megafon-совместимый):
```json
{"": {"response": {"accList": {
  "ivan": {
    "RealName": "Иван Иванов",
    "ext": ["702"],
    "mobile": "+79001234567",
    "tn": [],
    "sim": null,
    "Email": "",
    "Position": ""
  }
}}}}
```

- Ключ — login без `@domain`
- `RealName` — display_name из БД
- `ext` — внутренний номер (JSON array)
- `mobile` — мобильный телефон
- `tn` — телефон (fallback)
- `sim` — личный номер

При пустой БД или dev-входе — возвращает текущего пользователя как единственный контакт.

#### Звонки

| Команда | Описание | Push |
|---------|----------|------|
| `ProvisionCall` | Выделить leg | `response: "ok"` |
| `StartCall` | Исходящий вызов | `incomingCall` → callee |
| `AcceptCall` | Принять вызов | `accepted` → caller, `acceptAcked` → callee |
| `RejectCall` | Отклонить | `rejected` → caller |
| `CancelCall` | Отменить | `cancelled` → callee |
| `DisconnectCall` | Завершить | `terminated` → другая сторона |
| `AckAccept` | Подтверждение | `acceptAcked` → callee |
| `UpdateCall` | Обновить SDP | `updated` → другая сторона |
| `AcceptUpdate` | Принять обновление | `updateAccepted` → другая сторона |
| `Transfer` | Слепой перевод | — |

#### Удержание (Hold)

Сервер контролирует состояние удержания:

| Правило | Описание |
|---------|----------|
| **Только владелец снимает** | Если A поставил удержание, то только A может его снять. B получает `updated` но удержание не снимается |
| **Нельзя ставить повторно** | Если уже на удержании — вторая попытка отклоняется молча |
| **Очистка при завершении** | При `DisconnectCall` или `terminated` удержание снимается автоматически |

Поток:
```
A → UpdateCall (sendonly) → B: updated (sendonly)
B: hold active, ждёт A
B → UpdateCall (sendrecv) → отклоняется (не владелец)
A → UpdateCall (sendrecv) → B: updated (sendrecv)
B: hold снят
```

**Hold music:** Сервер загружает `hold_music.wav` при старте (8kHz mono 16-bit). При удержании логирует доступность. Для полного микширования аудио нужен SFU.

#### История

| Команда | Описание | Ответ |
|---------|----------|-------|
| `gethistory` | История звонков | `response: {calls[]}` |
| `createorupdatenote` | Заметка к звонку | `response: "ok"` |
| `deletenote` | Удалить заметку | `response: "ok"` |

#### Серверные настройки

| Команда | Описание | Ответ |
|---------|----------|-------|
| `getcommunicatorsettings` | Настройки сервера | `response: {videoEnabled: bool}` |

Клиент вызывает при старте. Если `--allowvideo` включён, сервер возвращает `videoEnabled: true`.

#### SMS

| Команда | Описание | Ответ |
|---------|----------|-------|
| `smstelnums` | Номера для SMS | `response: []` |
| `getsmschannels` | Каналы SMS | `response: []` |
| `getsms` | Получить SMS | `response: []` |
| `sendsms` | Отправить SMS | `response: "ok"` |

#### Конференция (stubs)

| Команда | Описание |
|---------|----------|
| `kickAll` | Завершить конференцию |
| `changedescription` | Изменить тему |
| `inviteParticipants` | Пригласить |
| `kickparticipants` | Исключить |
| `muteparticipant` | Отключить микрофон |
| `giveadmin` | Выдать права админа |
| `listConferences` | Список конференций |

---

## Push-уведомления (сервер → клиент)

| Payload `What` | Описание | Формат |
|----------------|----------|--------|
| `[IM]` | Новое сообщение | `{message: {id, from, to, body, timestamp}}` |
| `[IM_CONTACTS]` | Непрочитанные | `{contacts: {peer: {unseen: [id...]}}}` |
| `[IM_HIST]` | История чата (по одному на пира) | `{chatId, messages[]}` |
| `[CONTACTS]` | Адресная книга | `{objects: [...], subId}` |
| `[PRESENCE]` | Статус присутствия | `{batch: {login: {voice, im: {status}}}}` |
| `incomingCall` | Входящий звонок | `{leg, sdp, dest: {From: {""}}}` |
| `accepted` | Звонок принят | `{leg, sdp}` |
| `terminated` | Звонок завершён | `{leg, code}` |

---

## Схема PostgreSQL

```sql
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    login VARCHAR(255) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    domain VARCHAR(255) NOT NULL,
    partner VARCHAR(64) DEFAULT 'opensource',
    display_name VARCHAR(255),
    role VARCHAR(32) DEFAULT 'User',
    presence_status VARCHAR(32) DEFAULT 'offline',
    phone VARCHAR(64) DEFAULT '',
    ext VARCHAR(32) DEFAULT '',
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE TABLE personal_contacts (
    id SERIAL PRIMARY KEY,
    owner_id INT REFERENCES users(id) ON DELETE CASCADE,
    server_id VARCHAR(255),
    name_full VARCHAR(255),
    phone VARCHAR(64),
    ext VARCHAR(255),
    peer VARCHAR(255),
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE TABLE im_messages (
    id BIGSERIAL PRIMARY KEY,
    chat_id VARCHAR(512) NOT NULL,      -- формат: "user1:user2" (без @domain)
    sender_id INT REFERENCES users(id),
    sender_login VARCHAR(255),
    body TEXT,
    msg_type VARCHAR(32) DEFAULT 'text',
    persist BOOLEAN DEFAULT true,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE TABLE call_history (
    id SERIAL PRIMARY KEY,
    uid VARCHAR(255) NOT NULL UNIQUE,
    owner_id INT REFERENCES users(id),
    caller VARCHAR(255),
    callee VARCHAR(255),
    direction VARCHAR(16),
    start_time TIMESTAMPTZ,
    end_time TIMESTAMPTZ,
    duration INT,
    status VARCHAR(32),
    note TEXT,
    note_author VARCHAR(255),
    created_at TIMESTAMPTZ DEFAULT NOW()
);
```

---

## Логирование

```bash
# Все логи
QT_LOGGING_RULES="*.debug=true" ./build/communicator-server

# Только сервер
QT_LOGGING_RULES="server.*.debug=true" ./build/communicator-server

# Только WebSocket
QT_LOGGING_RULES="server.ws.debug=true;server.wsserver.debug=true" ./build/communicator-server

# Только диспетчер
QT_LOGGING_RULES="server.dispatch.debug=true" ./build/communicator-server
```

### Категории логов

| Категория | Модуль |
|-----------|--------|
| `server.main` | Точка входа, старт |
| `server.config` | Загрузка конфига |
| `server.ws` | WebSocket Send/Receive |
| `server.wsserver` | Сервер соединений |
| `server.session` | Управление сессиями |
| `server.dispatch` | RPC-маршрутизация |
| `server.auth` | Авторизация |
| `server.im` | Мгновенные сообщения |
| `server.presence` | Присутствие |
| `server.call` | Звонки |
| `server.addressbook` | Адресная книга |
| `server.history` | История звонков |
| `server.db` | PostgreSQL |
| `server.sms` | SMS |
| `server.conference` | Конференции |

---

## Примеры использования

### Тестовый сценарий (два клиента)

```bash
# Терминал 1: сервер
./build/communicator-server --config server.conf

# Терминал 2: клиент 1 (admin)
# Ввести: shipa@localhost / пароль

# Терминал 3: клиент 2 (user)
# Ввести: test@localhost / пароль
```

### Demo-режим (без БД)

```bash
./build/communicator-server --demo --port 8443
# Клиент: demo / demo
```

### Megafon-совместимый режим

```bash
./build/communicator-server --partner megafon --port 8443
# Только клиенты с partner=megafon подключатся
```

### Генерация hold-музыки

```bash
python3 tools/generate_hold_music.py resources/hold_music.wav
# 30 секунд, аккордовая прогрессия C→Am→F→G→C→G7, 8kHz mono
```

### Очистка истории

```bash
# Очистить всё
sudo -u postgres psql -h 127.0.0.1 -d communicator -c "TRUNCATE im_messages;"

# Очистить историю между двумя пользователями
sudo -u postgres psql -h 127.0.0.1 -d communicator -c \
  "DELETE FROM im_messages WHERE chat_id LIKE '%user1%' AND chat_id LIKE '%user2%';"
```

---

## Структура каталогов

```
server/
├── CMakeLists.txt                    # Сборка
├── README.md                         # Этот файл
├── server.conf                       # Конфигурация
├── certs/                            # TLS-сертификаты
├── resources/
│   └── server.conf.example           # Шаблон конфига
├── build/                            # Release сборка
├── build-debug/                      # Debug сборка
├── tools/
│   └── generate_hold_music.py        # Генератор hold-музыки (WAV)
└── src/
    ├── main.cpp                      # Точка входа, --newuser, --deluser, --listusers
    ├── server/
    │   ├── Server.{h,cpp}            # Оркестратор
    │   └── Config.{h,cpp}            # Конфигурация
    ├── network/
    │   ├── WsServer.{h,cpp}          # QWebSocketServer
    │   └── WsSession.{h,cpp}         # Per-connection state, Megafon-обёртка
    ├── session/
    │   ├── SessionManager.{h,cpp}    # sid→session map, match по user part
    │   └── UserSession.{h,cpp}       # Single user session
    ├── auth/
    │   └── AuthManager.{h,cpp}       # Login, хэши, demo-режим
    ├── protocol/
    │   ├── ProtocolTypes.h            # Типы протокола
    │   └── CommandDispatcher.{h,cpp}  # RPC-маршрутизатор, unwrap ""
    ├── im/
    │   ├── ImManager.{h,cpp}         # SendIM, BindIM, per-peer [IM_HIST]
    │   └── PresenceManager.{h,cpp}   # Presence broadcast, initial presence
    ├── calls/
    │   ├── CallManager.{h,cpp}       # Call state machine, hold logic
    │   ├── CallSession.{h,cpp}       # Per-call state (heldBy tracking)
    │   └── HoldMusicPlayer.{h,cpp}   # WAV loader for hold music
    ├── addressbook/
    │   └── AddressBookManager.{h,cpp} # Contacts CRUD, accList
    ├── history/
    │   └── HistoryManager.{h,cpp}    # Call history
    ├── sms/
    │   └── SmsManager.{h,cpp}        # SMS (stubs)
    ├── conference/
    │   └── ConferenceManager.{h,cpp} # Conference (stubs)
    └── db/
        └── Database.{h,cpp}          # PostgreSQL, imHistoryForUser
```

---

## Известные ограничения

- **SFU** — нет аудио-микшера; звонки работают через SDP pass-through (P2P или внешний шлюз)
- **Видео** — `--allowvideo` рассылает флаг клиентам; сервер передаёт video SDP между сторонами без перекодирования, SFU/MCU и записи видеопотока
- **Конференция** — stub-обработчики, нет управления участниками
- **SMS** — stub-ответы, нет интеграции с SMS-шлюзом
- **TLS** — self-signed сертификаты отвергаются клиентом без ручного bypass
- **Однопоточность** — Qt event loop, достаточно для ~100 concurrent сессий
- **Chat_id** — новые сообщения в формате `user1:user2` (без @domain); старые записи с нестандартным форматом поддерживаются через LIKE

---

## Лицензия

Та же, что и основной проект OpenSource Communicator.
