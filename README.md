# OpenSource Communicator

Совместимый open-source клиент для **ITooLabs Communicator** (Megafon PBX, virtual-ats и др.) на **Qt 6**.

## Скриншоты

### Вход

![Экран входа](screenshots/login.png)

### Контакты

![Все контакты](screenshots/all-contacts.png)

![Внешние контакты](screenshots/external-contacts.png)

### Набор и история

![Набрать вручную](screenshots/handcall.png)

![История звонков](screenshots/history.png)

### Звонок и заметки

![Звонок с заметками](screenshots/call-note.png)

### Конференция и контакты

![Конференция](screenshots/conference.png)

![Добавить контакт](screenshots/add-contact.png)

### Настройки

![Настройки звука](screenshots/settings.png)

## Возможности (v0.1)

- WebSocket-протокол ITooLabs (seq/ack, login, Bind, BindIM)
- Контакты домена и presence
- Чат (через BindIM + SMS API)
- Звонки: сигнализация + WebRTC через libdatachannel
- Демо-режим (`demo` / `demo`) без подключения к серверу
- Без телеметрии Amplitude/Sentry

## Сборка

```bash
cd client
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/opensource-communicator
```

### Зависимости (Arch)

- `qt6-base` `qt6-websockets` `qt6-multimedia`
- `cmake`
- `libdatachannel`

## Использование

1. Запустите приложение
2. Для просмотра интерфейса без сервера введите `demo` / `demo`
3. Для подключения к ВАТС укажите логин вида `user@domain.itoolabs.ru`, пароль и домен
4. Для Megafon: partner = `megafon`, auth-домен обычно совпадает с доменом АТС

## Структура

```
client/           — Qt6 приложение (open-source реализация)
screenshots/      — скриншоты интерфейса
PROTOCOL.md       — описание протокола ITooLabs WS
README.md         — этот файл
```

Локально для разбора могут лежать `extracted/`, `reverse-engineered/`, установщики — они в `.gitignore` и не публикуются.

## Лицензия

Оригинальный Megafon / ITooLabs Communicator — проприетарный продукт.
Этот проект — независимая реализация протокола, не аффилирован с Megafon или ITooLabs.
