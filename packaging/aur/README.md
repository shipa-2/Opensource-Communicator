# AUR-пакет

`opensource-communicator-git` — VCS-пакет, собирающий клиент из ветки `main`
репозитория GitHub. Ставит приложение в системный префикс `/usr`.

## Локальная сборка и установка

```bash
cd packaging/aur
makepkg -si
```

`makepkg` сам подтянет зависимости (`qt6-*`, `libdatachannel`, `opus`, `openssl`)
и обновит `pkgver` из git.

## Публикация в AUR

Требуется настроенный SSH-ключ в аккаунте AUR (https://aur.archlinux.org/account).

```bash
# 1. Клонировать (пустой) AUR-репозиторий пакета
git clone ssh://aur@aur.archlinux.org/opensource-communicator-git.git
cd opensource-communicator-git

# 2. Скопировать файлы пакета
cp /path/to/Opensource-Communicator/packaging/aur/PKGBUILD .
cp /path/to/Opensource-Communicator/packaging/aur/.SRCINFO .

# 3. Проверить сборку
makepkg -f

# 4. Обновить .SRCINFO после любых правок PKGBUILD
makepkg --printsrcinfo > .SRCINFO

# 5. Закоммитить и запушить
git add PKGBUILD .SRCINFO
git commit -m "Initial import: opensource-communicator-git"
git push
```

## Обновление пакета

При новых коммитах в `main` версия пересчитывается автоматически на этапе
`makepkg` (функция `pkgver()`). Для публикации обновления достаточно:

```bash
makepkg --printsrcinfo > .SRCINFO
git commit -am "Update to <новая версия>"
git push
```

## Замечания

- `license=('custom')` — в репозитории пока нет файла лицензии. После добавления
  `LICENSE` замените на корректный SPDX-идентификатор (например `MIT`, `GPL-3.0-or-later`)
  и добавьте установку файла в `/usr/share/licenses/$pkgname/`.
- Для стабильных релизов имеет смысл завести отдельный пакет
  `opensource-communicator` с `source` из тега/тарбола и контрольными суммами.
