# undef_rfid
RFID-считыватель [undef.space](https://undef.club)

# Что делает?
Впускает админов в спейс по картам MIFARE. Аутентификация происходит по номеру
карты Тройка или по UID любой другой карты.

# Документация
Устройство состоит из:
  - отладочной платы на базе ESP32-C3
  - NFC-считывателя на базе PN532
  - платы реле
  - держателя

## Использование
Поднесите карту к считывателю. Если авторизация прошла успешно, светодиод
загорится зелёным, устройство откроет дверь и зафиксирует факт входа в админской
беседе. Если нет - светодиод загорится красным.

## Администрирование
Чтобы использовать встроенный REPL, подключите устройство по USB и откройте
монитор порта (например, при помощи `idf.py monitor`). Если любая из команд
`add`, `remove` или `list` используется впервые за последние пять минут, нужно
будет ввести пароль. Используйте `help`, чтобы узнать список команд с
аргументами.

Формат `credential`:
  - `T<10 цифр>` - карта Тройка
  - `U<8 hex-цифр>` - любая карта с 4-байтовым UID
  - `U<14 hex-цифр>` - любая карта с 7-байтовым UID

## Сборка

### Держатель
TODO:

### Электроника
TODO:

### Прошивка
Написана на C с использованием ESP-IDF. Чтобы собрать прошивку:
  - установите ESP-IDF
  - склонируйте репозиторий
  - скачайте зависимости: `git submodule update --recursive`
  - создайте файл `main/include/secrets.h` со следующим содержанием:
    ```c
    #pragma once
    #define WIFI_SSID  "название сети"
    #define WIFI_PASS  "пароль сети"
    #define HASS_KEY   "ключ API HomeAssistant"
    #define REPL_PASS  "пароль от REPL"
    #define TG_KEY     "ключ API Telegram"
    #define TG_CHAT_ID -123456789
    ```
  - получите ключи шифрования:
    - если собираете новое устройство, сгенерируйте ключи:
      `espsecure.py generate_flash_encryption_key flash_key.bin`,
      `espsecure.py generate_signing_key --version 1 signing_key.pem`
    - если работаете с уже собранным устройством, получите их от админов и
      поместите в корень проекта (файлы `flash_key.bin` и `signing_key.pem`)
  - соберите проект и загрузите прошивку:
    - в самый первый раз: `idf.py build flash monitor`
    - в последующие разы, в т.ч. для уже собранного устройства: `./flash.sh`
