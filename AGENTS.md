# AGENTS.md — справка для AI-агентов

Документ для работы с репозиторием **OpenSource Communicator** вне постоянного рабочего окружения (например, с флешки, в другой машине или в новой сессии агента). **Читайте его до правок кода.**

Последнее обновление: **2026-07-18**, релиз **0.4.0** (клиент + `communicator-server` в репозитории).

---

## Что это за проект

Независимый open-source клиент для корпоративной ВАТС **ITooLabs Communicator** (Megafon PBX, virtual-ats и совместимые домены). Ориентир для сравнения — проприетарный Electron-клиент Megafon / ITooLabs **v3.13.x** (`communicator-megafon`, `com.itoolabs.communicator`).

Этот проект:

- реализует тот же **WebSocket-протокол** и **WebRTC-звонки** (аудио);
- **не** аффилирован с Megafon и ITooLabs;
- **не** содержит телеметрии Amplitude / Sentry;
- использует **нативный Qt 6 UI** (Plasma-friendly), а не Electron.

Логотип (роликовый спортсмен) — намеренно **не** пародия на бренд «Мегафон».

| | |
|---|---|
| Репозиторий | `https://github.com/shipa-2/Opensource-Communicator` |
| Ветка разработки | `main`; экспериментальное видео — `videotest` |
| Версия приложения | **0.4.0** (`client/CMakeLists.txt`, `main.cpp`) |
| Сервер | **`server/`** — `communicator-server` (не в CI-артефактах; см. `server/README.md`) |
| Протокол (реверс) | `PROTOCOL.md` (ориентир v3.13.11) |

---

## Состояние на момент передачи (handoff)

### Что нового в 0.4.0 (кратко)

| Область | Суть |
|---------|------|
| **Сервер** | `server/communicator-server` — WS, IM, presence, звонки, hold music, PostgreSQL; сборка локально |
| **OSC тема/аватар** | Share в настройках, IM `fnm=`, `Themeapplied!`, обои + opacity |
| **Чат** | Вложения «+», кликабельные URL, file notify |
| **Hold** | `AcceptUpdate`, «На удержании», mute исходящего при local hold |
| **Video (videotest)** | Камера/экран → H.264/WebRTC, preview, blur, landscape CallWindow |
| **Клиент** | `--newinstance`, порт/TLS в login, IM localhost normalize |

### Что сделано к 0.3.0 (основное)

| Область | Суть |
|---------|------|
| **Личные контакты на сервере** | `AddressBookManager`, RPC `createcontact` / `deletecontact` / `uploadcontacts`, push `[CONTACTS]`, миграция из QSettings |
| **`tel:` / single-instance** | `AppInstance` (QLocalServer), `.desktop` с `x-scheme-handler/tel`, paste `tel:` из буфера |
| **Кейпад «Набрать вручную»** | `DialKeypadWidget`, удержание ⌫ 1 с → полная очистка |
| **Запись разговоров** | WAV → MP3 (`ffmpeg`/`lame`); таймер звонка и старт записи — по **первому слышимому Opus** от абонента (≥40 байт), не по SIP `connected` |
| **Вход** | «Запомнить меня», список сохранённых аккаунтов, автологин при старте; **demo не сохраняется** |
| **Чат** | История снизу вверх, метки времени, короткое `RealName`; dedup исходящих; непрочитанные с миганием 💬 |
| **Цвет аватарки (advertise)** | Broadcast `**#RRGGBB**` через IM; цвета контактов в `ContactRowWidget` и `CallWindow` |
| **OSC IM (Openping / avatar / theme)** | `Openping!`, `**fnm=avatar.png;…**`, `**fnm=theme.jpg;ui;list;…**`, `Themeapplied!` — см. `PROTOCOL.md` |
| **Окно звонка** | Буква имени в круге, цвет peer, подсветка border при речи (VAD), заметки всегда видны |
| **MPRIS-пауза медиа** | `ExternalMediaPauser` — пауза музыки/видео в браузере и плеерах на Linux при звонке |
| **Демо-режим** | Чат (1 непрочитанное от «Администратор»), симуляция голоса, MPRIS-пауза, цвет admin |
| **Окно приложения** | Фиксированный размер **390×620**, без fullscreen/maximize (`NoFullscreenGuard`) |
| **CI / Releases** | После push в `main` — pre-release с тегом `DDMMYYYY-HHMM` (UTC) |

### Последняя сессия (коммиты `f7b7edf`, `884d18f`)

- **`ExternalMediaPauser`** — D-Bus MPRIS: `Pause` всех плееров при звонке, `Play` только тех, что играли; ref-count; работает в **демо** через `CallManager::pauseExternalMedia()` / `resumeExternalMedia()`.
- **Демо-чат** — одно непрочитанное «ты уволен» от `admin@demo.local`, цвет admin `#c0392b`.
- **Демо-звонок** — случайное моргание индикатора голоса (`startDemoVoiceSimulation`), буква собеседника в аватаре.
- **Advertise цвета** — вызов при смене аватара в шапке, после настроек, при входе в демо (на сервер в demo не уходит — только debug-log).
- **AUR** — `qt6-dbus` в depends (pkgrel 2).

### Ветка `videotest` — экспериментальное видео

- `VideoCapture` — `QCamera` + `QMediaCaptureSession` + `QVideoSink`, целевой кадр 640×360.
- `ScreenCapture` — Qt ≥6.5: `QScreenCapture` и PipeWire/xdg-desktop-portal на Wayland; Qt 6.4 CI: X11 fallback через `QScreen::grabWindow()`.
- `H264Encoder` — OpenH264 baseline, 640×360@15 FPS, 700 Кбит/с, Annex B.
- `CallManager` — H.264 PT 96 / 90 kHz, video track в общем BUNDLE с Opus, RTP step 6000.
- `H264Decoder` — OpenH264 I420 → `QImage`.
- `CallWindow` — широкое окно: одна строка ФИО/номер/статус/таймер, remote video слева, local preview и управление справа.
- Управление: stop/start video, screen share, blur исходящего кадра.
- Ошибка `Camera is in use` должна отключать видео без завершения аудиозвонка.
- CI собирает push в `videotest` на Linux/Windows, но release job остаётся только для `main` / ручного запуска.

### Что НЕ делать без запроса пользователя

- **Коммиты и push** — только по явной просьбе.
- **Телеметрия** — не добавлять.
- **Коммитить** `extracted/`, `reverse-engineered/`, `dist/`, `client/build*/`.

### Известные ограничения

- **MPRIS** — только Linux/FreeBSD (`Qt6::DBus`); на Windows пауза внешней медиа **не реализована**.
- **Полноэкранный режим** — заблокирован для всех окон приложения; maximize главного окна тоже (свойство `itlNoMaximize`).
- `CHANGELOG.md` может отставать от мелких UI-правок; **AGENTS.md** — приоритет для агентов.
- AppImage в CI не собирается (локально `packaging/linux/build-appimage.sh`).
- Push тегов `v*` запускает CI, но job **`release`** публикует артефакты **только** для `main` / `workflow_dispatch`.

---

## Контекст «флешки»

| Ситуация | Что делать |
|----------|------------|
| Есть весь git-клон | `client/`, `packaging/`, `PROTOCOL.md` — источник истины |
| Рядом `extracted/`, `reverse-engineered/` | Локальный реверс; в `.gitignore`, **не коммитить** |
| Нет бинарника | Linux: собрать из `client/`; Windows: ZIP из GitHub Releases |
| Нет интернета | Демо `demo`/`demo`; для ВАТС нужен `wss://` |

---

## Карта каталогов

```
opensource-communicator/
├── client/
│   ├── CMakeLists.txt               # VERSION 0.4.0; Qt6::DBus на UNIX (не Apple)
│   ├── resources/
│   │   ├── resources.qrc            # logo.png, help.png (PNG!), QSS
│   │   └── communicator-dialogs.qss
│   └── src/
│       ├── main.cpp                 # AppInstance, NoFullscreenGuard, MainWindow
│       ├── protocol/
│       │   ├── WsConnection.*       # WebSocket, seq/ack, reconnect
│       │   ├── WsApiClient.*        # RPC-обёртки
│       │   ├── CommunicatorClient.* # login, savedAccounts, rememberMe, routing
│       │   ├── AddressBookManager.* # Личные контакты [CONTACTS]
│       │   └── CallHistoryParser.*
│       ├── calls/
│       │   ├── CallManager.*        # WebRTC, MPRIS hook, remoteAudioStarted
│       │   └── CallTypes.h
│       ├── audio/
│       │   ├── AudioBridge.*        # Opus encode/decode, PCM
│       │   ├── CallRecorder.*       # WAV → MP3
│       │   ├── ExternalMediaPauser.*# MPRIS pause/resume (Linux)
│       │   ├── RingbackPlayer.* / IncomingRingPlayer.*
│       │   └── MessageNotifyPlayer.*
│       ├── chat/ChatManager.*       # IM, SMS, peerColor, demo messages
│       ├── settings/                # AppSettings (QSettings), UserDataStore (JSON)
│       ├── demo/DemoData.*          # demo/demo, seedChatMessages
│       └── ui/
│           ├── MainWindow.*         # Центр UI: контакты, история, dial, demo
│           ├── CallWindow.*         # Окно звонка, аватар-буква, VAD border
│           ├── LoginDialog.*        # rememberMe, saved accounts combo
│           ├── ChatDialog.*         # Формат «ЧЧ:ММ Имя: текст»
│           ├── ContactRowWidget.*   # Аватар-буква, peerColor, unread blink
│           ├── DialKeypadWidget.*
│           ├── AppInstance.*        # Single-instance + tel:
│           ├── ThemeHelper.*        # ThemeWatcher, NoFullscreenGuard
│           └── ...
├── PROTOCOL.md
├── CHANGELOG.md
├── README.md
├── AGENTS.md
├── server/                          # communicator-server (не в GitHub Release)
├── packaging/aur/PKGBUILD           # qt6-dbus, ffmpeg
└── .github/workflows/build.yml
```

### Куда смотреть при задаче

| Задача | Файлы |
|--------|--------|
| Подключение, login, URL | `CommunicatorClient.cpp`, `WsConnection.cpp` |
| **Сохранённые аккаунты / автологин** | `CommunicatorClient.cpp`, `LoginDialog.cpp`, `MainWindow::startSession` |
| RPC (Bind, Call, контакты…) | `WsApiClient.cpp` |
| **Личные контакты на сервере** | `AddressBookManager.cpp`, `MainWindow::mergeCustomContacts` |
| Звонки, SDP, WebRTC | `CallManager.cpp`, `AudioBridge.cpp` |
| **Пауза музыки при звонке** | `ExternalMediaPauser.cpp`, `CallManager::pause/resumeExternalMedia`, demo в `MainWindow` |
| **Таймер / запись по реальному аудио** | `CallManager::noteRemoteOpusFrame`, `remoteAudioStarted`, `CallRecorder` |
| Чат, SMS, цвета аватарок | `ChatManager.cpp`, `ChatDialog.cpp`, `ThemePreviewDialog.cpp` |
| Главный UI | `MainWindow.cpp` |
| Окно звонка | `CallWindow.cpp`, `MainWindow::onCallStateChanged` |
| **`tel:` / DnD** | `AppInstance.cpp`, `main.cpp`, `MainWindow::handleIncomingTelUri` |
| Кейпад | `DialKeypadWidget.cpp` |
| История (сервер) | `CallHistoryParser.cpp`, `MainWindow::refreshServerHistory` |
| Настройки, аватар | `AppSettings.cpp`, `SettingsDialog.cpp`, `ProfileAvatarWidget.cpp` |
| **Блок fullscreen** | `ThemeHelper.cpp` (`NoFullscreenGuard`), `MainWindow` (`itlNoMaximize`, fixed size) |
| Демо | `DemoData.cpp`, `m_demoMode`, `startDemoCallSimulation` |
| Тема ОС | `ThemeHelper.cpp` (`#if QT_VERSION >= 6.5`) |
| Qt 6.4 совместимость | `WsConnection.cpp` (`errorOccurred` vs `error`) |

---

## Архитектура

```
┌─────────────┐     signals      ┌──────────────────┐
│  MainWindow │◄────────────────►│ CommunicatorClient│
└──────┬──────┘                  └────────┬─────────┘
       │                    ┌───────────────┼───────────────┐
       │                    │               │               │
       │            ┌───────▼────────┐ ┌────▼─────────┐ ┌──▼──────────┐
       │            │   WsApiClient    │ │AddressBook   │ │ ChatManager │
       │            └───────┬────────┘ │ Manager       │ └─────────────┘
       │                    │          └──────────────┘
       │            ┌───────▼────────┐
       │            │   WsConnection   │──► wss://
       │            └──────────────────┘
       │
       ├────────► CallManager ──► libdatachannel + Opus (PT 111)
       │              └── ExternalMediaPauser (MPRIS, Linux)
       │
       ├────────► CallWindow / ChatDialog
       │
       └────────► AppInstance (QLocalServer) ◄── tel: / второй процесс
```

**Зависимости сборки** (`client/CMakeLists.txt`):

- Qt6: Core, Gui, Widgets, WebSockets, Network, Multimedia
- **Linux/FreeBSD:** дополнительно **Qt6 DBus** (MPRIS)
- OpenSSL, opus, LibDataChannel
- OpenH264 для кодирования и декодирования видео

---

## Вход, «Запомнить меня», сохранённые аккаунты

| Ключ QSettings | Назначение |
|----------------|------------|
| `rememberMe` | bool, по умолчанию `true` |
| `savedAccounts` | JSON-массив `LoginCredentials` (login, password, domain, …) |

**Поведение:**

- `MainWindow::startSession()` — если `rememberMe` и есть непустой не-demo аккаунт → `beginSessionWithCurrentCredentials()` без диалога.
- `LoginDialog` — выпадающий список сохранённых логинов; demo **никогда** не попадает в `savedAccounts`.
- Пустые логин/пароль в диалоге → подстановка `demo`/`demo`.

Файлы: `CommunicatorClient::loadSettings` / `saveSettings`, `upsertSavedAccount`, `LoginDialog::loadFromClient`.

---

## Пауза внешней медиа (MPRIS)

**Класс:** `itl::ExternalMediaPauser` (`client/src/audio/`).

| Событие | Действие |
|---------|----------|
| Начало звонка (первый leg в `CallManager`) | `pause()` — D-Bus `Pause` **всех** `org.mpris.MediaPlayer2.*` |
| Конец последнего звонка (`teardownCall`, `m_calls` пуст) | `resume()` — `Play` только для плееров, бывших `Playing` (или с пустым status — браузеры) |
| Демо-звонок | `MainWindow::startDemoCallSimulation` → `pauseExternalMedia`; сброс → `stopDemoCallSimulation` → `resume` |
| Перевод / сброс | `resumeExternalMediaIfIdle()` в `MainWindow::onCallStateChanged` |

**Ref-count** (`m_pauseDepth`) — вложенные pause/resume безопасны.

**Лог:** `itl.media` (`Q_LOGGING_CATEGORY` в `ExternalMediaPauser.cpp`).

**Windows:** stub (no-op) — DBus не линкуется.

**AUR:** зависимость `qt6-dbus`.

---

## Окно звонка (`CallWindow`)

| Элемент | Реализация |
|---------|------------|
| Буква в аватаре | `setAvatarLetter(displayName)` — первая буква имени (как в `ContactRowWidget`) |
| Цвет фона | `setAvatarColor` ← `ChatManager::peerColor(peer)` |
| Индикатор речи | `updateRemoteAudioLevel` — автокалибровка шума, border → `QPalette::Highlight` |
| Демо-VAD | `setRemoteSpeakingIndicator` + таймер в `startDemoVoiceSimulation` |
| Таймер разговора | `beginConversationTimer()` по сигналу `CallManager::remoteAudioStarted`, не по `connected` |
| До первого аудио | Текст «Соединение...» в `m_timerLabel` |

---

## Цвет аватарки (color advertisement)

**Протокол:** исходящее IM с телом `**#RRGGBB**` (regex в `ChatManager::isColorAdvertisement`) **каждому** контакту того же домена, что и логин (`login@domain`). Не `__broadcast__` (сервер его не знает).

| Когда отправляется | Где |
|--------------------|-----|
| После загрузки контактов | `MainWindow::onContactsLoaded` → `refreshColorAdvertisementPeers` → send |
| Смена цвета в шапке | `MainWindow::onProfileAvatarChanged` |
| После OK / «Рассылка цвета» в настройках | `MainWindow::onSettings` / `SettingsDialog` |
| Вход в demo | `enterDemoInterface` (без сети — только log) |

Список пиров: `MainWindow::refreshColorAdvertisementPeers` — все `m_contacts` с `@domain` логина, кроме self и телефонов. `ChatManager::setColorAdvertisementPeers` + `sendIm(peer, …, persist=false, copyToSelf=false)`.

**Приём:** push/history IM → `m_peerColors[peer]` → `peerColorReceived` → `ContactRowWidget::setPeerColor`, `CallWindow::setAvatarColor`.

**Демо:** `DemoData::seedChatMessages` → `setDemoPeerColor(admin, "#c0392b")`.

Сообщения-рекламы **не показываются** в чате (фильтр в `handlePayload` / history).

---

## OSC-only расширения IM (`ChatManager`)

Полное описание форматов: **`PROTOCOL.md`** (раздел «Расширения OpenSource Communicator»).

| Сообщение | persist | UI |
|-----------|---------|-----|
| `Openping!` | false | Список OSC-пиров; ответ + `sendSelfPresenceTo` |
| `**#RRGGBB**` | false | Цвет контакта (не в чате) |
| `**fnm=avatar.png;enc=b64;cnt=…**` | false | Фото аватарки 140×140 (не в чате) |
| `**fnm=theme.jpg;enc=b64;ui=N;list=N;cnt=…**` | true | Чат: «*Имя* поделился с вами темой» → предпросмотр |
| `Themeapplied!` | true | Чат у отправителя: «*Имя* применил вашу тему» |

**Openping!:** `setOpenpingCandidates` / `sendOpenpingBroadcast` при загрузке контактов (`MainWindow::onContactsLoaded`). Пиры в `UserDataStore::oscPeers`.

**Аватарка / цвет вручную:** «Настройки → Аккаунт» — «Поделиться аватаркой» / «Поделиться цветом» (`TransferDialog`, только OSC).

**Тема:** «Поделиться темой» под «Убрать обои»; JPEG 390×620 + `ui` / `list`. Приём: `ChatDialog` + `ThemePreviewDialog`; после «Применить» → `sendThemeApplied(peer)`.

**Файлы:** `ChatManager.cpp`, `SettingsDialog.cpp`, `ChatDialog.cpp`, `ThemePreviewDialog.cpp`, `UserDataStore` (`oscPeers`).

---

## Запись разговоров и таймер

1. SIP `connected` — UI «Разговор», но таймер **не** стартует.
2. Первый Opus-кадр ≥40 байт от remote → `onFirstRemoteAudio` → `remoteAudioStarted` → `beginConversationTimer` + `startCallRecording`.
3. `CallRecorder` — WAV PCM 16-bit mono 48 kHz; по завершении → MP3 через `ffmpeg` или `lame`.
4. Смешанная дорожка — очередь пар local+remote кадров (не писать mix на каждый кадр отдельно).

Настройки: `SettingsDialog` / `RecordingSettingsDialog`, `AppSettings`.

---

## Личные контакты на сервере (`AddressBookManager`)

См. предыдущую логику: CRUD на сервере, миграция `customContacts` → `uploadcontacts`, push `[CONTACTS]`.

| UI | `useServerContacts()` (= online && !demo) |
|----|------------------------------------------|
| Добавить / импорт / удалить | через `AddressBookManager` |
| Демо | только `AppSettings.customContacts` |

**Лог:** `itl.addressbook`.

**Отладка:** `QT_LOGGING_RULES="itl.addressbook=true" opensource-communicator`.

---

## `tel:` и single-instance

| Компонент | Роль |
|-----------|------|
| `AppInstance::serverName()` | `"opensource-communicator-v1"` |
| `main.cpp` | `sendToRunningInstance` → exit 0; иначе `startServer` |
| `.desktop` | `MimeType=x-scheme-handler/tel;`, `Exec=... %u` |
| Paste `tel:` | `MainWindow::eventFilter` (кроме модальных окон / ChatDialog) |

---

## Вкладка «Набрать вручную»

| Элемент | Текст |
|---------|--------|
| Заголовок поля | «Набрать номер или внутренний код:» |
| Placeholder | «702, ivan или +7...» |
| Кнопка | «Позвонить» |

**⌫ / 0 удержание:** 0–0.25 с акцент → 0.25–1 с fill → 1 с действие (`clear()` / `+`). Константы в `DialKeypadWidget.cpp`.

---

## Вкладка «История»

| Элемент | Текст |
|---------|--------|
| Период | «Показать за:» + меню |
| Направление | «Все», «Входящие», **«Без ответа»**, «Исходящие» |

Двойной щелчок / «Заметка» в ПКМ → `NotePopupDialog` с кнопками «Позвонить» и «Закрыть» (как у контактов). Парсинг сервера: `CallHistoryParser.cpp`.

**Self-test:** `OSC_SELFTEST=1` — автologin demo + `runHistorySelfTest()` (4 mine, 4 company, 1 internal в demo).

---

## Чат (UI и логика)

| Тема | Детали |
|------|--------|
| Порядок | Новые сообщения **снизу** (`ChatManager::messagesForPeer` sort) |
| Формат строки | `ЧЧ:ММ Имя: текст` или `29 янв. ЧЧ:ММ …` |
| Имя | Короткое RealName (первое слово), см. `ChatDialog::shortDisplayName` |
| Dedup | Исходящие echo от сервера не дублируют optimistic insert |
| Тема OSC | Уведомление «поделился темой» (ссылка) → `ThemePreviewDialog`; ack `Themeapplied!` |
| Непрочитанные | `ContactRowWidget::setUnreadBlink` — мигание 💬 каждые 500 ms |
| Уведомление | `MessageNotifyPlayer` если чат не открыт для peer |

---

## Демо-режим

**Вход:** `demo` / `demo` (`DemoData::isDemoCredentials`).

| Аспект | Поведение |
|--------|-----------|
| Сеть | WebSocket **не** открывается; домен `demo.local` |
| Контакты | `DemoData::contacts()` — фиксированный список |
| История | `DemoData::callHistory()` в `m_demoCallHistory` |
| Чат | 1 непрочитанное от **Администратор**: «ты уволен»; цвет admin `#c0392b` |
| Звонок | `startDemoCallSimulation` — таймеры connecting→ringing→connected; **не** через `CallManager` |
| Голос в дemo | `startDemoVoiceSimulation` — random blink border |
| MPRIS | pause/resume как у реального звонка |
| Контакты | `useServerContacts() == false` — локально |
| Выход | logout → `exitDemoInterface` → `leaveDemoMode` |

**Не симулируется в demo:** WebRTC, реальный MPRIS advertise на сервер, серверная история.

---

## Главное окно: размер и fullscreen

| Параметр | Значение |
|----------|----------|
| Размер | **390×620**, `setFixedSize` |
| Maximize | Кнопка убрана (`~WindowMaximizeButtonHint`) |
| Свойство | `itlNoMaximize=true` на `MainWindow` |
| Guard | `NoFullscreenGuard` в `main.cpp` — F11, `QWindow::windowStateChanged`, Show |
| Старт | `QTimer::singleShot(0)` сброс WM-состояния после `show()` |

Диалоги (звонок, чат): fullscreen блокируется guard'ом, maximize — нет (кроме main).

---

## Протокол (кратко)

Полное описание: **`PROTOCOL.md`**.

- `wss://<auth-domain>/ws/?_domain=<domain>` или `wss://<domain>/ws`
- `login` → `Bind` + `BindIM`
- Звонки: `ProvisionCall`, `StartCall`, `AcceptCall`, `DisconnectCall`, `UpdateCall`, `Transfer`
- Контакты: `subscribetoaddressbook`, `createcontact`, `deletecontact`, `uploadcontacts`, `[CONTACTS]`

---

## Сравнение с официальным клиентом (v0.3)

### Реализовано (рабочий паритет)

Авторизация, сессия, телефония, Opus/WebRTC, hold, слепой перевод, конференция (старт), контакты домена + **личные на сервере**, IM/SMS, presence, серверная/локальная история, запись MP3, дemo, **`tel:`/DnD**, color advertise, **OSC Openping / avatar / theme share**, VAD в звонке, **MPRIS-пауза (Linux)**, saved accounts, контекстное меню контактов.

### Частично

Видео и screen share (ветка `videotest`, требуется проверка с Megafon/ITooLabs gateway), конференция (нет mute/kick в разговоре), hold на сложных сценариях, SMS-центр, полный архив чата, attended transfer, DTMF.

### Не реализовано

Адаптивное видео/PLI-FIR/выбор камеры, автообновление, MSI, CRM, мобильные платформы, AppImage в CI, MPRIS на Windows, полная копия UI Megafon.

### RPC в `WsApiClient`

Обёрнуты: `login`, `bind`, `bindIm`, `subscribeToAddressBook`, `createContact`, `deleteContact`, `uploadContacts`, `provisionCall`, `startCall`, `acceptCall`, `rejectCall`, `cancelCall`, `disconnectCall`, `ackAccept`, `updateCall`, `blindTransfer`, `setOwnPresence`, `sendSms`, `getDomainContacts`, `getHistory`, `getSmsTelnums`, `sendIm`, `loadImHistory`.

---

## Пользовательские данные

| Данные | Где |
|--------|-----|
| Логин, пароль, домен, partner, rememberMe, savedAccounts | QSettings |
| Аудио, рингтоны, showChatButtons, showCallButtons | `AppSettings` |
| customContacts | QSettings; после миграции на сервер — **очищается** |
| Серверные личные контакты | Runtime `AddressBookManager` |
| Заметки, локальная история звонков | `~/.cache/.../user-data.json` |
| Аватар (путь + цвет) | QSettings |

Пароль в QSettings в открытом виде — не усиливать без запроса.

---

## Сборка, упаковка, CI

### Linux

```bash
cd client
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build   # /opt/opensource-communicator по умолчанию
```

Зависимости: `qt6-base`, `qt6-websockets`, `qt6-multimedia`, **`qt6-dbus`**, `libdatachannel`, `opus`, `openh264`, `openssl`, `cmake`. Для конвертации записей в MP3 дополнительно нужен исполняемый `ffmpeg` или `lame`; клиент с библиотеками FFmpeg не линкуется.

**AUR:** `opensource-communicator-git` — `packaging/aur/PKGBUILD`, pkgrel отражает зависимости.

### CI (`.github/workflows/build.yml`)

| Триггер | Сборка | Release job |
|---------|--------|-------------|
| push `main` | Win + Linux, Release+Debug | **Да** — pre-release |
| push `videotest` | Win + Linux, Release+Debug | **Нет**, только Actions artifacts |
| `workflow_dispatch` | то же | **Да** |
| tag `v*` | то же | **Нет** |

**Release:** артефакты `*-Release` (Windows ZIP + Linux tar.gz); тег **`DDMMYYYY-HHMM`** UTC; `prerelease: true`, `make_latest: false`; body из `CHANGELOG.md` для версии из CMake.

**Runner:** `ubuntu-24.04` (не `ubuntu-latest`) — иначе возможны отмены в очереди.

Windows локально: `packaging/windows/build-windows.sh`.

---

## Советы агентам

1. **Минимальный diff** — не раздувать абстракции.
2. Новая серверная фича → `PROTOCOL.md` → `WsApiClient` → manager → UI.
3. **Контакты** — ветки `useServerContacts()` и demo.
4. **Звонки** — Opus PT **111**; incoming ring **без** WebRTC до AcceptCall.
5. **Таймер/запись** — привязка к `remoteAudioStarted`, не к `connected`.
6. **MPRIS** — только Linux; тест с браузером/Spotify; pause **всех** MPRIS, resume только playing.
7. **Демо** — отдельные ветки `m_demoMode`; звонок не через `CallManager`.
8. **Qt 6.4** — `#if QT_VERSION >= 6.5` для новых API.
9. **Help на Windows** — только **PNG** (`help.png`), не JPEG.
10. **Коммиты/push** — только по запросу пользователя.

### Отладка звонка

1. WS: `ProvisionCall` → SDP  
2. WebRTC в `CallManager` (`disableAutoNegotiation`)  
3. `StartCall` / `AcceptCall`  
4. Opus ≥40 B → `remoteAudioStarted` → audio + timer  
5. `DisconnectCall`, teardown, MPRIS resume  

### Категории логов

| Категория | Модуль |
|-----------|--------|
| `itl.client` | CommunicatorClient |
| `itl.call` | CallManager |
| `itl.chat` | ChatManager |
| `itl.addressbook` | AddressBookManager |
| `itl.media` | ExternalMediaPauser |
| `itl.history` | MainWindow history self-test |

Release: warning/critical. Debug: `OSC_DEBUG_BUILD`, `itl.*.debug=true`.

---

## Связанные документы

| Файл | Назначение |
|------|------------|
| `README.md` | Пользовательская документация |
| `PROTOCOL.md` | WS-команды |
| `CHANGELOG.md` | История версий |
| `packaging/aur/README.md` | Публикация в AUR |
| `.github/workflows/build.yml` | CI и pre-release |

---

## Чеклист для новой сессии агента

- [ ] Прочитать этот файл; при WS — `PROTOCOL.md`
- [ ] Версия в `client/CMakeLists.txt` / `main.cpp`
- [ ] Дemo + обе платформы (Linux + Windows), если меняется поведение
- [ ] Контакты: `AddressBookManager`, `useServerContacts()`, миграция
- [ ] Звонки: `remoteAudioStarted`, MPRIS, demo simulation
- [ ] Чат: dedup, peerColor, unread blink
- [ ] Не трогать gitignored реверс-артефакты
- [ ] Коммиты/push — **только по просьбе**
- [ ] После UI — скриншоты/README по согласованию
