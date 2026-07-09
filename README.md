<p align="center">
  <img src="client/resources/logo.png" alt="OpenSource Communicator" width="128" height="128">
</p>

<p align="center">
  <sub>
    На логотипе изображён спортсмен на роликах, размахивающий руками.
    Логотип не является ассоциацией, товарным знаком или пародией на «Мегафон»
    и не связан с ним.
  </sub>
</p>

# OpenSource Communicator

Совместимый open-source клиент для **ITooLabs Communicator** (Megafon PBX, virtual-ats и др.) на **Qt 6**.

**[⬇ Установка (Arch Linux / AUR)](#установка-arch-linux--aur)** · **[⬇ Установка (Windows)](#установка-windows)**

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

## Возможности (v0.2)

- WebSocket-протокол ITooLabs (seq/ack, login, Bind, BindIM)
- Контакты домена и presence, локальные контакты, импорт/экспорт CSV и vCard
- Чат (через BindIM + SMS API)
- Звонки: сигнализация + WebRTC через libdatachannel, удержание, слепой перевод
- Запись разговоров в MP3 (одна или две дорожки; нужен `ffmpeg` или `lame`)
- История звонков с фильтрами и поиском
- Конференция с выбором собеседников и слушателей
- Демо-режим (`demo` / `demo`) без подключения к серверу
- Без телеметрии Amplitude/Sentry

Полный список изменений: [CHANGELOG.md](CHANGELOG.md).

## Установка (Arch Linux / AUR)

```bash
yay -S opensource-communicator-git
```

Пакет [`opensource-communicator-git`](https://aur.archlinux.org/packages/opensource-communicator-git)
собирает клиент из ветки `main` и ставит его в `/usr`.

## Установка (Windows)

Portable ZIP для Windows и tar.gz для Linux публикуются в [GitHub Releases](https://github.com/shipa-2/Opensource-Communicator/releases):

1. Откройте страницу релиза (например, [последний](https://github.com/shipa-2/Opensource-Communicator/releases/latest))
2. Скачайте `OpenSource-Communicator-*-win64.zip` (Windows) или `OpenSource-Communicator-*-linux-x86_64.tar.gz` (Linux)

Распакуйте архив в любую папку и запустите `opensource-communicator` / `opensource-communicator.exe`. Установщик (MSI) не нужен.

Для отладки в [Actions](https://github.com/shipa-2/Opensource-Communicator/actions/workflows/build.yml) доступны артефакты `*-Debug` — сборка с окном консоли и полными логами протокола/звонков.

При push в `main` CI автоматически создаёт/обновляет GitHub Release с тегом **ДДММГГГГ** (дата сборки) и прикрепляет **Release**-артефакты (Windows ZIP и Linux tar.gz). Debug-сборки остаются только в Actions.

## Сборка (Linux)

```bash
cd client
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

По умолчанию установка идёт в `/opt/opensource-communicator`:

- бинарник: `/opt/opensource-communicator/bin/opensource-communicator`
- `.desktop`: `/opt/opensource-communicator/share/applications/opensource-communicator.desktop`

Другой префикс:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
sudo cmake --install build
```

### Зависимости (Arch)

- `qt6-base` `qt6-websockets` `qt6-multimedia`
- `cmake`
- `libdatachannel`
- `opus`

Готовая сборка для Windows — в разделе [Установка (Windows)](#установка-windows). GitHub Actions также собирает Linux-бинарник (артефакты `linux-build-Release` / `linux-build-Debug`, tar.gz) на Ubuntu 24.04 с системными Qt6 и `libdatachannel-dev`.

### Типы сборки (Debug / Release)

| | **Release** (по умолчанию) | **Debug** |
|---|---------------------------|-----------|
| Назначение | Повседневное использование, CI, релизы | Разработка и отладка |
| Оптимизация | Включена | Отключена, символы отладки |
| Логи | Только предупреждения и ошибки | Полный вывод `itl.*` в консоль |
| Windows | Без окна терминала (GUI-приложение) | Консоль с логами при запуске |
| Linux | Логи в stderr при запуске из терминала | То же, но подробнее |

```bash
# Release (по умолчанию)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Debug
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
```

На Windows в Debug-сборке запускайте `opensource-communicator.exe` из `ucrt64`/`cmd`, чтобы видеть логи протокола и звонков. В GitHub Actions доступны оба артефакта: `windows-portable-Release` и `windows-portable-Debug`.

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
AGENTS.md         — справка для AI-агентов (архитектура, паритет с официальным клиентом)
README.md         — этот файл
```

Локально для разбора могут лежать `extracted/`, `reverse-engineered/`, установщики — они в `.gitignore` и не публикуются.

## Лицензия

Оригинальный Megafon / ITooLabs Communicator — проприетарный продукт.
Этот проект — независимая реализация протокола, не аффилирован с Megafon или ITooLabs.
