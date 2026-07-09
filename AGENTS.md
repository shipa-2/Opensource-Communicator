# AGENTS.md — справка для AI-агентов

Документ для работы с репозиторием **OpenSource Communicator** вне постоянного рабочего окружения (например, с флешки, в другой машине или в новой сессии агента). Читайте его **до** правок кода.

## Что это за проект

Независимый open-source клиент для корпоративной ВАТС **ITooLabs Communicator** (Megafon PBX, virtual-ats и совместимые домены). Оригинальный клиент Megafon / ITooLabs — проприетарный Electron-приложение (`communicator-megafon`, пакет `com.itoolabs.communicator`, ориентир для сравнения — **v3.13.x**).

Этот проект:

- реализует тот же **WebSocket-протокол** и **WebRTC-звонки** (аудио);
- **не** аффилирован с Megafon и ITooLabs;
- **не** содержит телеметрии Amplitude / Sentry;
- использует **нативный Qt 6 UI** (Plasma-friendly), а не Electron.

Логотип (роликовый спортсмен) — намеренно **не** пародия на бренд «Мегафон».

Репозиторий: `https://github.com/shipa-2/Opensource-Communicator`  
Версия приложения: **0.1.0** (`client/CMakeLists.txt`, `main.cpp`).

---

## Контекст «флешки»

Если вы открыли **только эту директорию** на съёмном носителе:

| Ситуация | Что делать |
|----------|------------|
| Есть весь git-клон | Работайте как обычно; `client/`, `packaging/`, `PROTOCOL.md` — источник истины |
| Рядом лежат `extracted/`, `reverse-engineered/`, `.dmg`/`.exe` | Это **локальные артефакты разбора** официального клиента; в `.gitignore`, **не коммитить** |
| Нет собранного бинарника | Linux: собрать из `client/`; Windows: скачать ZIP из GitHub Actions (см. README) |
| Нет интернета | Демо-режим `demo`/`demo` работает без сервера; для реальной ВАТС нужен `wss://` |

Публикуемое содержимое репозитория — **только** open-source код и документация. Материалы реверса хранятся локально для справки при реализации протокола.

---

## Карта каталогов

```
opensource-communicator/
├── client/                      # Всё приложение (CMake, C++17, Qt 6)
│   ├── CMakeLists.txt           # Зависимости, install prefix, список исходников
│   ├── resources/
│   │   ├── resources.qrc        # logo.png, help.jpg, QSS для диалогов
│   │   ├── logo.png             # Иконка окна и README
│   │   ├── help.jpg             # Картинка в HelpDialog
│   │   └── communicator-dialogs.qss
│   └── src/
│       ├── main.cpp             # Точка входа: CommunicatorClient + CallManager + MainWindow
│       ├── protocol/            # WebSocket, RPC, сессия
│       ├── calls/               # WebRTC (libdatachannel), звонки, конференции
│       ├── chat/                # BindIM, IM, SMS
│       ├── audio/               # Opus, микрофон/динамик, рингтоны
│       ├── settings/            # QSettings + user-data.json
│       ├── demo/                # Офлайн-демо без сервера
│       └── ui/                  # Окна и виджеты
├── PROTOCOL.md                  # Реверс протокола ITooLabs WS (из v3.13.11)
├── README.md                    # Пользовательская документация
├── AGENTS.md                    # Этот файл
├── screenshots/                 # Скриншоты для README
├── packaging/
│   ├── aur/                     # PKGBUILD для AUR (opensource-communicator-git)
│   ├── linux/                   # .desktop, иконки hicolor, build-appimage.sh (опционально)
│   └── windows/                 # build-windows.sh (справочный локальный скрипт)
├── .github/workflows/build.yml  # CI: Windows ZIP + Linux tar.gz (Release+Debug)
└── .gitignore                   # extracted/, reverse-engineered/, dist/, build-*/
```

### Ключевые модули (куда смотреть при задаче)

| Задача | Файлы |
|--------|--------|
| Подключение, login, URL | `CommunicatorClient.cpp`, `WsConnection.cpp` |
| Команды API (Bind, Call, SMS…) | `WsApiClient.cpp`, `WsApiClient.h` |
| Звонки, SDP, WebRTC | `CallManager.cpp`, `AudioBridge.cpp` |
| Чат и SMS | `ChatManager.cpp`, `ChatDialog.cpp` |
| Главный UI, контакты, история | `MainWindow.cpp` |
| Настройки, заметки, аватар | `AppSettings.cpp`, `UserDataStore.cpp`, `SettingsDialog.cpp` |
| Демо без сервера | `DemoData.cpp`, ветки `m_demoMode` в `MainWindow.cpp` |
| Тема светлая/тёмная | `ThemeHelper.cpp` (`#if QT_VERSION >= 6.5` для `colorSchemeChanged`) |
| Совместимость Qt 6.4 | `WsConnection.cpp` (`errorOccurred` vs `error`) |

---

## Архитектура

```
┌─────────────┐     signals      ┌──────────────────┐
│  MainWindow │◄────────────────►│ CommunicatorClient│
└──────┬──────┘                  └────────┬─────────┘
       │                                  │
       │                          ┌───────▼────────┐
       │                          │   WsApiClient    │
       │                          └───────┬────────┘
       │                                  │
       │                          ┌───────▼────────┐
       │                          │   WsConnection   │──► wss:// сервер ВАТС
       │                          └──────────────────┘
       │
       ├────────► CallManager ──► libdatachannel (Opus audio)
       │
       └────────► ChatManager ──► BindIM / sendsms
```

**Слои:**

1. **WsConnection** — WebSocket, seq/ack, keepalive (`noop`), handshake `Main` task, reconnect.
2. **WsApiClient** — RPC (`What: request/response`), состояние `AppState`, обёртки команд протокола.
3. **CommunicatorClient** — учётные данные, URL, демо-режим, маршрутизация presence и call events.
4. **CallManager** — жизненный цикл вызова: `ProvisionCall` → SDP offer/answer → `StartCall`/`AcceptCall` → RTP через **libdatachannel** + **Opus** (`AudioBridge`).
5. **ChatManager** — `BindIM`, история `[IM_HIST]`, исходящие IM, SMS через `smstelnums` + `sendsms`.
6. **MainWindow** — список контактов, фильтры, набор номера, окно звонка, конференция, настройки.

Точка сборки зависимостей: `client/CMakeLists.txt` — Qt6 (Core, Gui, Widgets, WebSockets, Network, Multimedia), OpenSSL, opus, LibDataChannel.

---

## Протокол (кратко)

Полное описание: **`PROTOCOL.md`**.

- Транспорт: **WebSocket** `wss://<auth-domain>/ws/?_domain=<domain>` (Megafon) или `wss://<domain>/ws`.
- После handshake — `login`, затем `Bind` (телефония) и `BindIM` (чат).
- Сообщения с полем `""` = команда; RPC через `What: "request"` / `"response"` и `id`.
- Звонки: `ProvisionCall`, `StartCall`, `AcceptCall`, `DisconnectCall`, `UpdateCall` (re-INVITE для hold), `Transfer` (слепой перевод).
- Контакты: `subscribetoaddressbook`, `listaccounts`, presence через `SetPresence` и push-события.

При добавлении команд сверяйтесь с `PROTOCOL.md` и при необходимости с локальным `reverse-engineered/` (не в git).

---

## Сравнение с официальным клиентом

Официальный **ITooLabs / Megafon Communicator** — Electron, встроенная аналитика, автообновления, богатый UI. Ниже — практический паритет **этой** реализации (v0.1).

### Реализовано (рабочий паритет)

| Область | Детали |
|---------|--------|
| Авторизация | `login` по WS, partner `megafon`, домен / auth-домен |
| Сессия | seq/ack, reconnect, `bye` |
| Телефония | Исходящие/входящие звонки, ответ, сброс, гудки (ringback) |
| Медиа | Аудио WebRTC, кодек **Opus**, выбор входа/выхода в настройках |
| Удержание | Hold через SDP `sendonly` + `UpdateCall` |
| Перевод | **Слепой** (`Transfer`) из окна звонка |
| Конференция | UI выбора участников, JSON `conference` в `StartCall` |
| Контакты | Адресная книга с сервера, presence, поиск, фильтры (все / недавние / внешние) |
| Локальные контакты | Добавление вручную (`customContacts` в настройках) |
| Чат IM | `BindIM`, отправка/приём, загрузка истории |
| SMS | Каналы через `smstelnums`, отправка `sendsms` (если сервер выдал номер) |
| Presence | Смена своего статуса (`SetPresence`) |
| История звонков | Локальная (до 50 записей), вкладка «История» |
| Заметки по абонентам | Локально, в окне звонка и popup |
| Профиль | Аватар и цвет заголовка (локально) |
| Звонки | Встроенные мелодии + свой файл (wav/mp3/ogg), превью в настройках |
| Демо | `demo` / `demo` — UI и симуляция звонка без сети |
| Приватность | Нет Amplitude, Sentry и прочей телеметрии |
| UI | Нативный Qt, адаптация к тёмной/светлой теме ОС |

### Частично реализовано

| Область | Что есть | Чего нет / ограничения |
|---------|----------|-------------------------|
| Конференция | Старт с несколькими участниками | Нет UI управления участниками в разговоре (mute/kick), как в Electron-клиенте |
| Hold | Локальная смена SDP | Может расходиться с поведением официального клиента на сложных сценариях |
| SMS | API и чат-окно для SMS-peer | Нет отдельного «SMS-центра», списков рассылок |
| История чата | `loadImHistory` / `[IM_HIST]` | Нет полного офлайн-архива как в толстом клиенте |
| Контакты | Сервер + локальные | Нет синхронизации локальных контактов на сервер |
| Справка | Статичная картинка `help.jpg` | Нет интерактивной базы знаний |

### Не реализовано (на v0.1)

Следующего **нет в коде** — не ищите зря; для фичи нужна новая разработка:

- **Видеозвонки** и демонстрация экрана
- **Консультативный (attended) перевод** — только `blindTransfer`
- **DTMF** (тональный набор во время разговора)
- **Запись разговоров**
- **Встроенный видеофон / камера**
- Автообновление приложения, MSI/installer (Windows только portable ZIP)
- Интеграции официального клиента (календарь, CRM, deep links)
- Мобильные платформы (только desktop Qt)
- Готовый **AppImage** в GitHub (в CI собирается только tar.gz; AppImage — локально через `build-appimage.sh`)
- Полная копия визуального дизайна Megafon Communicator

При оценке «готово ли» сверяйте с таблицами выше, а не с полным списком команд в `PROTOCOL.md`: многие RPC описаны документально, но **не вызываются** из UI.

### Команды API в коде (`WsApiClient`)

Уже обёрнуты: `login`, `bind`, `bindIm`, `subscribeToAddressBook`, `provisionCall`, `startCall`, `acceptCall`, `rejectCall`, `cancelCall`, `disconnectCall`, `ackAccept`, `updateCall`, `blindTransfer`, `setOwnPresence`, `sendSms`, `getDomainContacts`, `getHistory`, `getSmsTelnums`, `sendIm`, `loadImHistory`.

Не обёрнуты / не используются в UI — потенциальная работа на будущее (см. `PROTOCOL.md`): voicemail, call pickup, парковка, очереди, и т.д.

---

## Демо-режим

- Вход: логин и пароль **`demo`** (`DemoData::isDemoLogin`).
- Не открывает WebSocket; подставляет фиктивный домен `demo.local`, контакты и историю из `DemoData.cpp`.
- Звонок симулируется таймером в `MainWindow` (`startDemoCallSimulation`).
- Выход из демо: обычный logout → `leaveDemoMode`.

Используйте демо для UI/скриншотов без доступа к ВАТС.

---

## Пользовательские данные и настройки

| Данные | Где хранятся |
|--------|----------------|
| Логин, пароль, домен, partner | `QSettings` — org/app `opensource-communicator` |
| Аудиоустройства, рингтоны, custom contacts | `QSettings` через `AppSettings` |
| Заметки, недавние звонки, история звонков | `QStandardPaths::CacheLocation/user-data.json` (`UserDataStore`) |
| Аватар профиля | Путь к файлу в `QSettings` |

На Linux типичный cache: `~/.cache/opensource-communicator/opensource-communicator/`.  
Пароль хранится в открытом виде в QSettings — как и во многих десктоп-клиентах; не усиливайте без запроса пользователя.

---

## Сборка, упаковка, CI

### Linux (основной способ для разработчика)

```bash
cd client
cmake -B build -DCMAKE_BUILD_TYPE=Release   # или Debug
cmake --build build
sudo cmake --install build   # по умолчанию /opt/opensource-communicator
```

По умолчанию **Release**. **Debug** (`OSC_DEBUG_BUILD`): полные логи `itl.*`, на Windows — консольное окно. **Release** на Windows: `WIN32_EXECUTABLE` (без терминала), в лог попадают только warning/critical.

Зависимости: `qt6-base`, `qt6-websockets`, `qt6-multimedia`, `libdatachannel`, `opus`, `openssl`, `cmake`.

**Arch / AUR:** `packaging/aur/PKGBUILD` → пакет `opensource-communicator-git`, установка в `/usr`.

**AppImage:** `packaging/linux/build-appimage.sh` — опционально, локально; в CI собирается tar.gz (не AppImage).

### CI (GitHub Actions)

`.github/workflows/build.yml` собирает **обе платформы** в матрице **Release + Debug**:

| Job | Платформа | Артефакты |
|-----|-----------|-----------|
| `windows-portable` | MSYS2 UCRT64, Qt 6.8, libdatachannel static | `windows-portable-Release` (без консоли), `windows-portable-Debug` (с консолью) |
| `linux-build` | Ubuntu 24.04, системные Qt6 + `libdatachannel-dev` | `linux-build-Release`, `linux-build-Debug` (tar.gz от `install`) |

- Триггер: `workflow_dispatch` или тег `v*`.
- Job `release` (только на тег `v*`) прикрепляет к GitHub Release **только Release**-артефакты (pattern `*-Release`).
- Локально Windows: `packaging/windows/build-windows.sh` (поддерживает `BUILD_TYPE=Debug`).

### Install prefix

`GNUInstallDirs`: бинарник, `.desktop`, иконки из `packaging/linux/icons/`. Переопределение: `-DCMAKE_INSTALL_PREFIX=...`.

---

## Советы агентам при разработке

1. **Минимальный diff** — проект небольшой; не раздувайте абстракции.
2. **Сначала `PROTOCOL.md` и `WsApiClient`** — новая серверная фича почти всегда начинается с RPC.
3. **Звонки** — любые изменения SDP/RTP проверяйте в `CallManager` и `AudioBridge`; payload type Opus = **111**.
4. **Демо-режим** — при добавлении сетевых фич учитывайте ветки `m_demoMode` в `MainWindow`, `CommunicatorClient`, `ChatManager`.
5. **Qt 6.4 vs 6.5+** — используйте `#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)` для новых API (`errorOccurred`, `colorSchemeChanged`). Ubuntu LTS может быть на 6.4.
6. **Windows / libdatachannel** — bundled libsrtp ломается с `-Werror`; в CI уже `ENABLE_WARNINGS_AS_ERRORS=OFF`, static linking.
7. **Не коммитьте** `extracted/`, `reverse-engineered/`, `dist/`, `client/build*/` — см. `.gitignore`.
8. **Брендинг** — не использовать логотип/название Megafon; продукт — «OpenSource Communicator» / `opensource-communicator`.
9. **Телеметрию не добавлять** без явного запроса пользователя.
10. **Скриншоты** — обновляйте `screenshots/` при заметных UI-изменениях и README при необходимости.

### Типичный порядок отладки звонка

1. WS: `ProvisionCall` → ответ с SDP  
2. WebRTC: create offer/answer в `CallManager`  
3. WS: `StartCall` / `AcceptCall` с local SDP  
4. ICE connected → `AudioBridge` шлёт Opus в `localAudioTrack`  
5. Завершение: `DisconnectCall`, очистка `m_peers`

Логи: категории `itl.client`, `itl.call`, `itl.chat` (Qt logging).

---

## Связанные документы

| Файл | Назначение |
|------|------------|
| `README.md` | Установка (AUR, Windows ZIP), быстрый старт, скриншоты |
| `PROTOCOL.md` | Команды и форматы WS (реверс v3.13.11) |
| `packaging/aur/README.md` | Публикация в AUR |
| `.github/workflows/build.yml` | Автоматический билд: Windows + Linux, Release + Debug |

---

## Краткий чеклист для новой сессии агента

- [ ] Прочитать этот файл и при необходимости `PROTOCOL.md`
- [ ] Понять задачу: протокол / UI / звук / упаковка / документация
- [ ] Проверить, нужен ли демо-режим и обе платформы (Linux + Windows)
- [ ] Не трогать gitignored артефакты реверса
- [ ] После UI-изменений — обновить скриншоты/README по согласованию с пользователем
- [ ] Коммиты и PR — **только по явной просьбе** пользователя
