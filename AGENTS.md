# AGENTS.md — справка для AI-агентов

Документ для работы с репозиторием **OpenSource Communicator** вне постоянного рабочего окружения (например, с флешки, в другой машине или в новой сессии агента). **Читайте его до правок кода.**

Последнее крупное обновление документа: **2026-07-09**, после коммита `1ce754b` (*Sync server contacts, improve manual dial UX, and timestamp pre-releases*).

---

## Что это за проект

Независимый open-source клиент для корпоративной ВАТС **ITooLabs Communicator** (Megafon PBX, virtual-ats и совместимые домены). Оригинальный клиент Megafon / ITooLabs — проприетарный Electron-приложение (`communicator-megafon`, пакет `com.itoolabs.communicator`, ориентир для сравнения — **v3.13.x**).

Этот проект:

- реализует тот же **WebSocket-протокол** и **WebRTC-звонки** (аудио);
- **не** аффилирован с Megafon и ITooLabs;
- **не** содержит телеметрии Amplitude / Sentry;
- использует **нативный Qt 6 UI** (Plasma-friendly), а не Electron.

Логотип (роликовый спортсмен) — намеренно **не** пародия на бренд «Мегафон».

| | |
|---|---|
| Репозиторий | `https://github.com/shipa-2/Opensource-Communicator` |
| Ветка разработки | `main` |
| Версия приложения | **0.2.0** (`client/CMakeLists.txt`, `main.cpp`) |
| Протокол (реверс) | `PROTOCOL.md` (ориентир v3.13.11) |

---

## Состояние на момент передачи (handoff)

### Что сделано в последней сессии (до `1ce754b`)

1. **Личные контакты на сервере ВАТС** — модуль `AddressBookManager`, RPC `createcontact` / `deletecontact` / `uploadcontacts`, подписка на push `[CONTACTS]`. Контакты синхронизируются между машинами после входа под тем же аккаунтом.
2. **Миграция локальных контактов** — при первом входе после обновления `customContacts` из QSettings один раз уходят на сервер через `uploadcontacts`; после успеха локальный список в QSettings очищается.
3. **Drag-and-drop** — перетаскивание vCard/CSV/текста в окно добавляет контакты (с диалогом подтверждения).
4. **Обработка `tel:`** — single-instance через `QLocalServer`; второй процесс передаёт URI работающему; поле набора на вкладке «Набрать вручную» заполняется; `.desktop` регистрирует `x-scheme-handler/tel`.
5. **Вкладка «Набрать вручную»** — цифровой кейпад `DialKeypadWidget`, стили кнопок под тему, кнопка «Позвонить» с явными цветами через `updateDialCallButtonStyle()`.
6. **Удержание ⌫ на кейпаде** — 1 с очищает весь номер; визуальный индикатор: 0–0.5 с акцент → сброс → 0.5–1 с плавное заполнение акцентом.
7. **UI-подписи** — см. таблицу «Актуальные строки UI» ниже.
8. **CI** — после push в `main` публикуется **pre-release** с тегом `DDMMYYYY-HHMM` (UTC, время коммита).

### Что НЕ делать без запроса пользователя

- **Коммиты и push** — только по явной просьбе.
- **Телеметрия** — не добавлять.
- **Коммитить** `extracted/`, `reverse-engineered/`, `dist/`, `client/build*/`.

### Известные ограничения / незавершённое

- `CHANGELOG.md` частично устарел по UI-строкам истории и CI (в AGENTS.md актуальнее).
- Скриншоты в `screenshots/` могут не отражать кейпад и новые подписи.
- AppImage в CI не собирается (только локально `packaging/linux/build-appimage.sh`).
- Теги `v*` запускают сборку, но job `release` **не** публикует релиз (только `main` / `workflow_dispatch`).

---

## Контекст «флешки»

Если открыта **только эта директория** на съёмном носителе:

| Ситуация | Что делать |
|----------|------------|
| Есть весь git-клон | Работайте как обычно; `client/`, `packaging/`, `PROTOCOL.md` — источник истины |
| Рядом лежат `extracted/`, `reverse-engineered/`, `.dmg`/`.exe` | Локальные артефакты разбора официального клиента; в `.gitignore`, **не коммитить** |
| Нет собранного бинарника | Linux: собрать из `client/`; Windows: ZIP из GitHub Releases (pre-release) |
| Нет интернета | Демо `demo`/`demo` без сервера; для реальной ВАТС нужен `wss://` |

---

## Карта каталогов

```
opensource-communicator/
├── client/                          # Всё приложение (CMake, C++17, Qt 6)
│   ├── CMakeLists.txt               # Зависимости, VERSION, список исходников
│   ├── resources/
│   │   ├── resources.qrc            # logo.png, help.png, QSS
│   │   ├── logo.png
│   │   ├── help.png                 # HelpDialog (PNG — важно для Windows portable)
│   │   └── communicator-dialogs.qss
│   └── src/
│       ├── main.cpp                 # Single-instance, tel: IPC, MainWindow
│       ├── protocol/
│       │   ├── WsConnection.*       # WebSocket, seq/ack, reconnect
│       │   ├── WsApiClient.*        # RPC-обёртки команд
│       │   ├── CommunicatorClient.* # Сессия, login, маршрутизация событий
│       │   ├── AddressBookManager.* # Личные контакты на сервере [CONTACTS]
│       │   └── CallHistoryParser.*  # Разбор серверной истории звонков
│       ├── calls/                   # CallManager, AudioBridge, WebRTC
│       ├── chat/                    # ChatManager, BindIM, SMS
│       ├── audio/
│       │   ├── Opus, рингтоны...
│       │   └── MessageNotifyPlayer.* # Звук уведомления о сообщении
│       ├── settings/                # AppSettings (QSettings), UserDataStore (JSON)
│       ├── demo/                    # DemoData — офлайн-режим
│       └── ui/
│           ├── MainWindow.*         # Главное окно, контакты, история, dial, DnD
│           ├── DialKeypadWidget.*   # Кейпад вкладки «Набрать вручную»
│           ├── AppInstance.*        # QLocalServer single-instance + tel: forward
│           └── ...                  # диалоги, ContactRowWidget, ThemeHelper...
├── PROTOCOL.md
├── README.md                        # Пользовательская документация (серверные контакты, tel:)
├── CHANGELOG.md                     # История версий (может отставать от UI)
├── AGENTS.md                        # Этот файл
├── screenshots/
├── packaging/
│   ├── aur/
│   ├── linux/
│   │   └── opensource-communicator.desktop  # MimeType=x-scheme-handler/tel; Exec=... %u
│   └── windows/
└── .github/workflows/build.yml      # CI + pre-release job
```

### Ключевые модули (куда смотреть при задаче)

| Задача | Файлы |
|--------|--------|
| Подключение, login, URL | `CommunicatorClient.cpp`, `WsConnection.cpp` |
| Команды API (Bind, Call, SMS…) | `WsApiClient.cpp`, `WsApiClient.h` |
| **Личные контакты на сервере** | `AddressBookManager.cpp`, `CommunicatorClient.cpp`, `MainWindow::mergeCustomContacts` |
| Звонки, SDP, WebRTC | `CallManager.cpp`, `AudioBridge.cpp` |
| Чат и SMS | `ChatManager.cpp`, `ChatDialog.cpp` |
| Главный UI, контакты, история | `MainWindow.cpp`, `MainWindow.h` |
| **Кейпад набора** | `DialKeypadWidget.cpp`, `MainWindow::updateDialCallButtonStyle` |
| **`tel:` / второй экземпляр** | `AppInstance.cpp`, `main.cpp`, `MainWindow::handleIncomingTelUri` |
| **Drag-and-drop контактов** | `MainWindow::setupDragDrop`, `handleDroppedMimeData`, `importContactsFromPath` |
| История звонков (сервер) | `CallHistoryParser.cpp`, `MainWindow::refreshServerHistory` |
| Настройки, заметки, аватар | `AppSettings.cpp`, `UserDataStore.cpp`, `SettingsDialog.cpp` |
| Демо без сервера | `DemoData.cpp`, ветки `m_demoMode` / `useServerContacts()` |
| Тема светлая/тёмная | `ThemeHelper.cpp` (`#if QT_VERSION >= 6.5` для `colorSchemeChanged`) |
| Уведомление о сообщении | `MessageNotifyPlayer.cpp` |
| Совместимость Qt 6.4 | `WsConnection.cpp` (`errorOccurred` vs `error`) |

---

## Архитектура

```
┌─────────────┐     signals      ┌──────────────────┐
│  MainWindow │◄────────────────►│ CommunicatorClient│
└──────┬──────┘                  └────────┬─────────┘
       │                                  │
       │                    ┌─────────────┼─────────────┐
       │                    │             │             │
       │            ┌───────▼────────┐ ┌──▼──────────┐ ┌▼──────────────┐
       │            │   WsApiClient    │ │AddressBook  │ │  ChatManager  │
       │            └───────┬────────┘ │  Manager     │ └───────────────┘
       │                    │          └──────────────┘
       │            ┌───────▼────────┐
       │            │   WsConnection   │──► wss:// сервер ВАТС
       │            └──────────────────┘
       │
       ├────────► CallManager ──► libdatachannel (Opus audio)
       │
       ├────────► DialKeypadWidget ──► m_dialInput (вкладка «Набрать вручную»)
       │
       └────────► AppInstance (QLocalServer) ◄── второй процесс / tel: из ОС

main.cpp:
  1) sendToRunningInstance(argv) → exit 0 если уже запущен
  2) startServer() → handleIncomingTelUri / raise window
```

**Слои:**

1. **WsConnection** — WebSocket, seq/ack, keepalive (`noop`), handshake `Main` task, reconnect.
2. **WsApiClient** — RPC (`What: request/response`), состояние `AppState`, обёртки команд.
3. **CommunicatorClient** — учётные данные, URL, демо, маршрутизация presence/call/chat/**address book**.
4. **AddressBookManager** — in-memory кэш личных контактов с сервера; CRUD + `uploadcontacts`; push `[CONTACTS]`.
5. **CallManager** — `ProvisionCall` → SDP → `StartCall`/`AcceptCall` → RTP (**libdatachannel** + **Opus**, PT **111**).
6. **ChatManager** — `BindIM`, `[IM_HIST]`, SMS.
7. **MainWindow** — контакты, фильтры, история, набор, звонок, конференция, DnD, `tel:`.

Зависимости сборки: `client/CMakeLists.txt` — Qt6 (Core, Gui, Widgets, WebSockets, Network, Multimedia), OpenSSL, opus, LibDataChannel.

---

## Личные контакты на сервере (`AddressBookManager`)

### Назначение

Контакты, добавленные через **Добавить → Контакт** или **Импорт**, хранятся в адресной книге **на сервере ВАТС**, а не только в QSettings. После входа с другого ПК подтягиваются автоматически.

### Поток данных

```
login OK → CommunicatorClient::onLoginSuccess
  → m_addressBook->setDomain(domain); clear()
  → если AppSettings.customContacts не пуст → uploadLocalContacts()
  → subscribeToAddressBook (как и раньше для доменных контактов)

Push [CONTACTS] → AddressBookManager::handlePayload
  → objects[]: новые/обновлённые контакты или deleted:true

createcontact / deletecontact / uploadcontacts response
  → AddressBookManager::handleResponse
  → contactsChanged → MainWindow::onAddressBookChanged → mergeCustomContacts()
```

### Идентификация на сервере

- `serverId` строится в `buildServerId(rawId, subId)`; `subId` нормализуется (`ab:` префикс, `~` для email/ext).
- Поиск для удаления: `findServerIdForPeer(peer)` — по точному peer, ext@domain, телефону (`peersMatch`).
- **Важно:** при удалении нельзя опираться только на UI-кэш без serverId — иначе «призрак» в списке и ошибка при повторном delete. Исправлено через `removeContactByServerId` и обработку `deleted` в `[CONTACTS]`.

### Где в UI

| Действие | Поведение при `useServerContacts()` (= online && !demo) |
|----------|----------------------------------------------------------|
| Добавить контакт | `addressBook()->createContact()` |
| Импорт CSV/vCard | `importContactsFromPath` → batch create/upload на сервер |
| Удалить (ПКМ) | `addressBook()->deleteContactByPeer(peer)` |
| Список | `mergeCustomContacts()` берёт `addressBook()->contacts()`, не QSettings |

### Демо-режим

`useServerContacts()` возвращает `false` → контакты как раньше только в `AppSettings.customContacts` (локально).

### Отладка

```bash
QT_LOGGING_RULES="itl.addressbook=true" opensource-communicator
```

Категория лога: `itl.addressbook` (`Q_LOGGING_CATEGORY` в `AddressBookManager.cpp`).

### RPC в `WsApiClient`

- `createContact(QJsonObject)` — поля `phones`, `emails`, `name` (см. `contactToServerJson`)
- `deleteContact(contactId)` — server id
- `uploadContacts(QJsonArray)` — массовая загрузка при миграции

Сверяйте с `PROTOCOL.md` при расширении полей контакта.

---

## `tel:`-ссылки и single-instance (`AppInstance`)

### Зачем

Клик по `tel:+7...` в браузере или повторный запуск с URI не должен открывать второе окно — номер попадает в поле набора.

### Реализация

| Компонент | Роль |
|-----------|------|
| `AppInstance::serverName()` | `"opensource-communicator-v1"` |
| `main.cpp` | Сначала `sendToRunningInstance(args)`; иначе `startServer` + `MainWindow` |
| `handleIncomingTelUri` | `applyTelUriToDial` + raise/focus |
| `applyTelUriToDial` | Парсит `tel:`, percent-encoding, обрезает `;param` |
| `opensource-communicator.desktop` | `MimeType=x-scheme-handler/tel;`, `Exec=... %u` |

### Вставка `tel:` из буфера

`MainWindow::eventFilter` перехватывает paste глобально (кроме модальных окон и `ChatDialog`), если текст — `tel:` URI.

### Drag-and-drop

`setupDragDrop()` — `setAcceptDrops` на central widget, вкладки, список контактов, dial page; `eventFilter` для `DragEnter`/`Drop`. Поддерживаются URL, текст, файлы `.vcf`/`.csv`. При drop показывается диалог; импорт идёт через те же пути, что и меню **Импорт**.

---

## Вкладка «Набрать вручную» и `DialKeypadWidget`

### UI (актуальные строки)

| Элемент | Текст |
|---------|--------|
| Заголовок поля | «Набрать номер или внутренний код:» |
| Placeholder | «702, ivan или +7...» |
| Вкладка | «Набрать вручную» |
| Кнопка | «Позвонить» |
| Меню «Добавить» | «Контакт», «Импорт» (без «...») |

Поле **без** `setClearButtonEnabled` — очистка через кейпад.

### Кейпад

Сетка 1–9, 0, `#`, ⌫. Стили: круглые кнопки, hover — акцентная обводка без заливки, pressed — акцент.

### Удержание ⌫ (константы в `DialKeypadWidget.cpp`)

| Фаза | Время | Поведение |
|------|-------|-----------|
| SolidAccent | 0–500 мс | Кнопка полностью акцентного цвета |
| Filling | 500–1000 мс | Цвет сброшен, плавный переход base → accent (индикатор) |
| Timeout | 1000 мс | `m_edit->clear()`, флаг `m_backspaceHoldClearDone` |
| Короткий клик | < 1000 мс | Один символ назад (`onBackspace`) |

Таймеры: `m_backspaceHoldTimer` (1000 мс), `m_backspaceHoldPhaseTimer` (500 мс), `m_backspaceHoldProgressTimer` (16 мс tick).

### Кнопка «Позвонить»

`MainWindow::updateDialCallButtonStyle()` — явный QSS с цветами палитры; текст на акценте через `textOnAccentBackground()`. Вызывается при смене темы (`refreshTheme`).

---

## Вкладка «История» (актуальные строки UI)

| Элемент | Текст |
|---------|--------|
| Период | «Показать за:» + ссылка-кнопка (меню периода) |
| Фильтр направления | «Все», «Входящие», **«Без ответа»**, «Исходящие» |

Парсинг серверных записей: `CallHistoryParser.cpp`. Локальная история — по-прежнему в `UserDataStore` (до 50 записей).

---

## Протокол (кратко)

Полное описание: **`PROTOCOL.md`**.

- Транспорт: `wss://<auth-domain>/ws/?_domain=<domain>` (Megafon) или `wss://<domain>/ws`.
- После handshake: `login` → `Bind` (телефония) + `BindIM` (чат).
- RPC: `What: "request"` / `"response"`, поле `id`.
- Звонки: `ProvisionCall`, `StartCall`, `AcceptCall`, `DisconnectCall`, `UpdateCall` (hold), `Transfer`.
- Контакты домена: `subscribetoaddressbook`, presence.
- **Личные контакты:** `createcontact`, `deletecontact`, `uploadcontacts`, push `What: "[CONTACTS]"`.

При добавлении команд сверяйтесь с `PROTOCOL.md` и локальным `reverse-engineered/` (не в git).

---

## Сравнение с официальным клиентом (v0.2)

### Реализовано

| Область | Детали |
|---------|--------|
| Авторизация | `login`, partner `megafon`, домен / auth-домен |
| Сессия | seq/ack, reconnect, `bye` |
| Телефония | Исходящие/входящие, ответ, сброс, ringback |
| Медиа | WebRTC + **Opus**, выбор устройств |
| Удержание / перевод / конференция | Hold, слепой `Transfer`, UI конференции |
| Контакты | Домен + presence; **личные на сервере**; импорт/экспорт CSV/vCard |
| **`tel:` / DnD** | Single-instance, handler в .desktop, drop файлов |
| Чат / SMS / Presence | BindIM, sendsms, SetPresence |
| История | Серверная вкладка + локальная; фильтры периода и направления |
| Запись разговоров | WAV → MP3 (`ffmpeg`/`lame`) |
| Демо | `demo`/`demo` без сети |
| UI | Qt 6, тёмная/светлая тема, кейпад набора |

### Частично реализовано

| Область | Что есть | Ограничения |
|---------|----------|-------------|
| Конференция | Старт с ролями | Нет mute/kick в разговоре |
| Hold | Локальный SDP | Сложные сценарии могут отличаться от Electron |
| SMS | API + чат-peer | Нет SMS-центра |
| История чата | `loadImHistory` | Нет полного архива |
| Справка | `help.png` | Статичная картинка |

### Не реализовано

Видео, attended transfer, DTMF, автообновление, MSI, CRM-интеграции, мобильные платформы, AppImage в CI, копия дизайна Megafon.

### Команды API в `WsApiClient`

Обёрнуты: `login`, `bind`, `bindIm`, `subscribeToAddressBook`, **`createContact`**, **`deleteContact`**, **`uploadContacts`**, `provisionCall`, `startCall`, `acceptCall`, `rejectCall`, `cancelCall`, `disconnectCall`, `ackAccept`, `updateCall`, `blindTransfer`, `setOwnPresence`, `sendSms`, `getDomainContacts`, `getHistory`, `getSmsTelnums`, `sendIm`, `loadImHistory`.

Не в UI: voicemail, pickup, парковка, очереди — см. `PROTOCOL.md`.

---

## Демо-режим

- Вход: **`demo`** / **`demo`** (`DemoData::isDemoLogin`).
- WebSocket не открывается; домен `demo.local`, фиктивные контакты/история.
- **Контакты только локально** (`AppSettings`), `useServerContacts() == false`.
- Звонок: `startDemoCallSimulation` в `MainWindow`.
- Выход: logout → `leaveDemoMode`.

---

## Пользовательские данные

| Данные | Где |
|--------|-----|
| Логин, пароль, домен, partner | `QSettings` (`opensource-communicator`) |
| Аудио, рингтоны | `AppSettings` / QSettings |
| **customContacts** | QSettings; используется в демо и до миграции; после успешного `uploadcontacts` **очищается** |
| **Серверные личные контакты** | Кэш в `AddressBookManager` (runtime), источник — сервер |
| Заметки, недавние, локальная история | `~/.cache/.../user-data.json` |
| Аватар | Путь в QSettings |

Пароль в QSettings в открытом виде — не усиливать без запроса.

---

## Сборка, упаковка, CI

### Linux (разработчик)

```bash
cd client
cmake -B build -DCMAKE_BUILD_TYPE=Release   # или Debug
cmake --build build
sudo cmake --install build   # по умолчанию /opt/opensource-communicator
```

- **Release** — без консоли на Windows, логи warning/critical.
- **Debug** (`OSC_DEBUG_BUILD`) — полные логи `itl.*`, консоль на Windows.

Зависимости: `qt6-base`, `qt6-websockets`, `qt6-multimedia`, `libdatachannel`, `opus`, `openssl`, `cmake`.

**AUR:** `packaging/aur/PKGBUILD` → `opensource-communicator-git`.

**AppImage:** локально `packaging/linux/build-appimage.sh` (не в CI).

### CI (`.github/workflows/build.yml`)

Триггер: push `main`, `workflow_dispatch`, тег `v*` (сборка без auto-release для тегов).

| Job | Артефакты |
|-----|-----------|
| `windows-portable` Release/Debug | ZIP portable |
| `linux-build` Release/Debug | tar.gz install tree |
| **`release`** | Только после успеха **обоих** jobs на `main` / manual |

**Публикация (`release` job):**

- Скачивает артефакты `*-Release` (Windows ZIP + Linux tar.gz).
- Тег и имя: **`DDMMYYYY-HHMM`** — UTC, **время коммита**:  
  `TZ=UTC git log -1 --format=%cd --date=format:%d%m%Y-%H%M`
- **`prerelease: true`**, **`make_latest: false`**
- Описание: версия из `CMakeLists.txt` + фрагмент `CHANGELOG.md` для этой версии.

Пример тега: `09072026-1803` → в тексте релиза «09.07.2026 18:03 (UTC)».

Windows локально: `packaging/windows/build-windows.sh`.

---

## Советы агентам

1. **Минимальный diff** — проект компактный; не раздувайте абстракции.
2. **Серверная фича** → `PROTOCOL.md` → `WsApiClient` → менеджер/клиент → `MainWindow`.
3. **Контакты** → всегда проверять ветки `useServerContacts()` и демо.
4. **Звонки** → `CallManager` / `AudioBridge`; Opus PT = **111**.
5. **Qt 6.4 vs 6.5+** → `#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)`.
6. **Windows / libdatachannel** → `ENABLE_WARNINGS_AS_ERRORS=OFF` в CI.
7. **Брендинг** — не Megafon; продукт «OpenSource Communicator».
8. **Скриншоты/README** — по согласованию с пользователем после заметного UI.
9. **Коммиты** — только по запросу.

### Отладка звонка

1. WS: `ProvisionCall` → SDP  
2. WebRTC offer/answer в `CallManager`  
3. WS: `StartCall` / `AcceptCall`  
4. ICE → `AudioBridge` → Opus  
5. `DisconnectCall`, очистка peers  

### Категории логов Qt

`itl.client`, `itl.call`, `itl.chat`, **`itl.addressbook`**

Release по умолчанию: только warning/critical. Debug: `itl.*.debug=true`.

---

## Связанные документы

| Файл | Назначение |
|------|------------|
| `README.md` | Установка, серверные контакты, `tel:` |
| `PROTOCOL.md` | WS-команды (реверс) |
| `CHANGELOG.md` | История версий |
| `packaging/aur/README.md` | AUR |
| `.github/workflows/build.yml` | CI и pre-release |

---

## Чеклист для новой сессии агента

- [ ] Прочитать этот файл; при задачах по WS — `PROTOCOL.md`
- [ ] Уточнить: протокол / UI / звук / упаковка / CI
- [ ] Учесть **демо** и **обе платформы** (Linux + Windows), если меняется поведение
- [ ] Контакты: `AddressBookManager` + `useServerContacts()` + миграция QSettings
- [ ] `tel:` / DnD: `AppInstance`, `main.cpp`, `.desktop`
- [ ] Кейпад: `DialKeypadWidget` (тайминги hold в константах в .cpp)
- [ ] Не трогать gitignored артефакты реверса
- [ ] Коммиты и push — **только по явной просьбе** пользователя
- [ ] После UI-изменений — скриншоты/README по согласованию
