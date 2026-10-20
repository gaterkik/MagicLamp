# Системная Архитектура: MagicLamp (ESP32-C6)

## 1. Аппаратная спецификация и схема пинов (Hardware & Pinout)
- **Микроконтроллер:** ESP32-C6 Mini (RISC-V, 160 МГц, 4MB Flash).
- **Блок питания:** Внешний 5V / 3.0A. Программный лимит тока: строго **2500 мА (2.5 А)**.

### Таблица пинов (Изменять ЗАПРЕЩЕНО):
| Устройство | Линия | Пин ESP32-C6 | Примечание по реализации |
| :--- | :--- | :--- | :--- |
| **LED-матрица 16x16** | `DIN` | **GPIO 2** | WS2812B, 256 LED, вход снизу (`Y=0`), горизонтальная змейка, кольцо по `X`. |
| **Кнопка TTP223** | `I/O` | **GPIO 7** | HIGH при касании. Удержание от 700 мс шлет тики каждые 40 мс. Сброс — 5 кликов. |
| **Гироскоп GY-521** | `SDA` / `SCL` | **GPIO 6 / GPIO 5** | Шина I2C (100 кГц), адреса `0x68` / `0x69`. Принимать любой `WHO_AM_I` при наличии I2C ACK! |
| **Микрофон INMP441** | `SCK`/`WS`/`SD` | **GPIO 18 / 19 / 20** | Аппаратный I2S Standard Mode (`driver/i2s_std.h`), DMA + 128-pt FFT + AGC. |

## 2. Версионирование и конфигурация сети (`version.h` и `secrets.h`)
1. **`include/version.h`:** Содержит `#define FIRMWARE_VERSION "0.3.0"`.
2. **`include/secrets.h` (обязательно добавлен в `.gitignore`):**
   Содержит локальные учетные данные Wi-Fi (`WIFI_SSID`, `WIFI_PASSWORD`).
   Также создается шаблон `include/secrets.example.h` (с пустыми полями) для репозитория GitHub.
3. **Логика подключения `WiFiManager` (`src/network/wifi_manager.*`):**
   - При старте пытается подключиться к сети из `include/secrets.h` (или сохраненной в `Preferences` NVS).
   - Поднимает mDNS с именем `magiclamp` (`http://magiclamp.local`).
   - Если за 15 секунд подключиться к роутеру не удалось — автоматически поднимает точку доступа `MagicLamp-Setup` (с Captive Portal для ввода нового Wi-Fi).
   - Поддерживает фоновый `ArduinoOTA` для мгновенной заливки прошивки по локальной сети без кабеля.

## 3. Архитектура облачного обновления с GitHub (`src/network/ota_github.*`)
- **Эндпоинт проверки:** `https://api.github.com/repos/gaterkik/MagicLamp/releases/latest`
- **Транспорт:** `WiFiClientSecure` с `client.setInsecure()` (чтобы обновление работало безотказно без привязки к сроку действия корневых сертификатов) + `HTTPClient` с `setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS)` (так как GitHub отдает бинарники релиза через 302 редирект на `objects.githubusercontent.com`).
- **Заголовки запроса:** Обязателен заголовок `User-Agent: MagicLamp-ESP32C6-OTA`.
- **Алгоритм:**
  1. Парсинг JSON ответа GitHub API: извлечение `tag_name` (например, `"v0.3.1"` -> очистка от префикса `v` и сравнение версий с `FIRMWARE_VERSION`).
  2. Поиск в массиве `assets` файла с именем `firmware.bin` и извлечение его `browser_download_url`.
  3. Скачивание потока и запись во флеш через стандартную библиотеку `<Update.h>` (`Update.begin(contentLength, U_FLASH)` -> `Update.writeStream()` -> `Update.end(true)`).
  4. Перед стартом прошивки матрица гасится (или показывает спокойный индикатор), после успешного `Update.end(true)` вызывается `ESP.restart()`.

## 4. Архитектура Веб-интерфейса (`src/network/web_server.*`)
- Работает на базе стандартного `WebServer` (порт 80) из состава Arduino Core 3.x без тяжелых сторонних зависимостей.
- Фронтенд (HTML/CSS/JS) встроен во Flash как сжатая строка `PROGMEM` в `src/network/webui_html.h`, чтобы веб-интерфейс работал всегда, даже после OTA-обновления одного только `firmware.bin`.
- **REST API:**
  - `GET /api/status` — JSON со всеми метриками (температура чипа, RSSI, IP, версия прошивки, текущий эффект, яркость, статус GY-521, статус последней проверки GitHub OTA).
  - `POST /api/control` — изменение включения, яркости, номера эффекта (0..9), скорости и чувствительности микрофона.
  - `POST /api/ota/check` — запуск проверки и обновления с GitHub Releases.
  - `POST /api/ota/upload` — прямая загрузка файла `firmware.bin` из браузера через `Update`.

## 5. Структура папок проекта
- `platformio.ini`, `partitions.csv`, `.gitignore`
- `include/config.h`, `include/version.h`, `include/secrets.h`, `include/secrets.example.h`
- `src/main.cpp`
- `src/hal/` — `led_matrix`, `button`, `mic_i2s`, `imu_mpu6050`
- `src/effects/` — `effect_manager`, `core_effects`, `math_effects`, `audio_effect`
- `src/network/` — `wifi_manager`, `ota_github`, `web_server`, `webui_html.h`