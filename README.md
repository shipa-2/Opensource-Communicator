# OpenSource Communicator

Совместимый open-source клиент для **ITooLabs Communicator** (Megafon PBX, virtual-ats и др.) на **Qt 6**.

## Возможности (v0.1)

- WebSocket-протокол ITooLabs (seq/ack, login, Bind, BindIM)
- Контакты домена и presence
- Чат (через BindIM + SMS API)
- Звонки: сигнализация + WebRTC через libdatachannel
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
2. Меню **Аккаунт → Войти**
3. Укажите логин вида `user@domain.itoolabs.ru`, пароль, домен
4. Для Megafon: partner = `megafon`, auth-домен обычно совпадает с доменом АТС

## Структура

```
client/           — Qt6 приложение (open-source реализация)
PROTOCOL.md       — описание протокола ITooLabs WS
README.md         — этот файл
```

Локально для разбора могут лежать `extracted/`, `reverse-engineered/`, установщики — они в `.gitignore` и не публикуются.

## Лицензия

Оригинальный Megafon / ITooLabs Communicator — проприетарный продукт.
Этот проект — независимая реализация протокола, не аффилирован с Megafon или ITooLabs.
