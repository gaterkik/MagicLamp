# Системная Архитектура: MagicLamp (ESP32-C6)

## 1. Аппаратная спецификация и схема пинов (Hardware & Pinout)
- **Микроконтроллер:** ESP32-C6 Mini (RISC-V, 160 МГц, 4MB Flash).
- **Блок питания:** Внешний 5V / 3.0A. Программный лимит тока: строго **2500 мА (2.5 А)**.

### Таблица пинов (Изменять ЗАПРЕЩЕНО):
| Устройство | Линия | Пин ESP32-C6 | Примечание по реализации |
| :--- | :--- | :--- | :--- |
| **LED-матрица 16x16** | `DIN` | **GPIO 2** | WS2812B, 256 LED, вход снизу (`Y=0`), горизонтальная змейка, кольцо по `X`. Кроссфейд 500 мс, OTA анимация, шторки 600 мс. |
| **Кнопка TTP223** | `I/O` | **GPIO 7** | HIGH при касании. Окно мульти-клика 420 мс (`BUTTON_CLICK_TIMEOUT_MS = 420`). 1 клик = Вкл/Выкл; 2 клика = смена эффекта (в цикле `inCycle`); удержание от 700 мс = яркость; 3 клика = OTA; 4 клика = переключение Wi-Fi/Zigbee; 7 кликов = сброс настроек Wi-Fi (`BUTTON_EVENT_SEVEN_CLICKS`). |
| **Микрофон INMP441** | `SCK`/`WS`/`SD` | **GPIO 18 / 19 / 20** | Аппаратный I2S Standard Mode (`driver/i2s_std.h`), DMA + 128-pt FFT + AGC. |

## 2. Версионирование и конфигурация сети (`version.h` и `secrets.h`)
1. **`include/version.h`:** Содержит `#define FIRMWARE_VERSION "0.8.0"`.
2. **`include/secrets.h` (обязательно добавлен в `.gitignore`):**
   Содержит локальные учетные данные Wi-Fi (`WIFI_SSID`, `WIFI_PASSWORD`).
   Также создается шаблон `include/secrets.example.h` (с пустыми полями) для репозитория GitHub.
3. **Логика подключения `WiFiManager` (`src/network/wifi_manager.*`):**
   - При старте пытается подключиться к сети из `include/secrets.h` (или сохраненной в `Preferences` NVS).
   - Для максимальной стабильности радиоканала отключает энергосбережение Wi-Fi: `WiFi.setSleep(false)` и полностью выключает AP (`WiFi.enableAP(false)`) при успешном подключении к STA.
   - Поднимает mDNS с именем `magiclamp` (`http://magiclamp.local`).
   - Если за 15 секунд подключиться к роутеру не удалось — автоматически поднимает точку доступа `MagicLamp-Setup` (с Captive Portal для ввода нового Wi-Fi).
   - Поддерживает фоновый `ArduinoOTA` для мгновенной заливки прошивки по локальной сети без кабеля.
   - Поддерживает автоматический таймер неактивности WebUI (10 минут) и синхронизацию времени NTP (GMT+2).

## 3. Архитектура облачного обновления с GitHub (`src/network/ota_github.*`)
- **Эндпоинт проверки:** `https://api.github.com/repos/gaterkik/MagicLamp/releases/latest`
- **Транспорт и стабильность HTTPS на ESP32-C6:**
   - Обязательное отключение энергосбережения Wi-Fi: `WiFi.setSleep(false)`.
   - Использование `WiFiClientSecure` с `client.setInsecure()`.
   - Установка таймаута TLS рукопожатия и сокета: 15 секунд.
   - До 3 автоматических повторов с паузой при ошибке соединения (код возврата `< 0`, например `-1` connection failed).
   - **Разделение TLS-сессий при HTTP 302 редиректе:** GitHub перенаправляет скачивание релиза с `github.com` на CDN `objects.githubusercontent.com`. Во избежание сбоев TLS-сессии на ESP32-C6 перенаправление обрабатывается с явным закрытием предыдущего соединения и созданием нового чистого экземпляра `WiFiClientSecure` для целевого URL скачивания `firmware.bin`.
- **Заголовки запроса:** Обязателен заголовок `User-Agent: MagicLamp-ESP32C6-OTA`.
- **Алгоритм:**
   1. Парсинг ответа GitHub API `/releases/latest`: во избежание проблем с усечением крупного JSON извлекать `"tag_name"` потоковым поиском/фильтром. Сразу производить нормализацию (отсекать префикс `v`) и сравнение с `FIRMWARE_VERSION`. Если версии совпадают — немедленно переводить статус в `UP_TO_DATE` и завершать проверку.
   2. Определение URL для загрузки `firmware.bin`: извлекать `browser_download_url` из `assets` или гарантированно использовать детерминированный URL прямой загрузки `https://github.com/gaterkik/MagicLamp/releases/latest/download/firmware.bin` (если поле `browser_download_url` не попало в буфер или было обрезано).
   3. Скачивание потока и запись во флеш через стандартную библиотеку `<Update.h>` (`Update.begin(contentLength, U_FLASH)` -> `Update.writeStream()` -> `Update.end(true)`).
   4. **КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ СРЫВА OTA И ПОТОКОБЕЗОПАСНЫЙ РЕНДЕР (`OtaGithub` + `EffectManager`):**
      - **Причина срыва прошивки 0.8.0 на этапе Flashing:** фоновая задача `otaTask` (FreeRTOS) вызывала `FastLED.show()` напрямую из коллбэка `Update.onProgress` одновременно с вызовом `FastLED.show()` в основном цикле `loop()`. Это приводило к аппаратной коллизии RMT-драйвера ESP32-C6, повреждению буфера DMA и перезагрузке микроконтроллера по панике/Watchdog до завершения `Update.end(true)`.
      - **Потокобезопасная архитектура:**
        * Прямой вызов `FastLED.show()` из задачи `otaTask` и коллбэка `Update.onProgress` **СТРОГО ЗАПРЕЩЕН**!
        * Задача `otaTask` и `Update.onProgress` ТОЛЬКО атомарно обновляют состояние в `EffectManager`: статус `m_otaState` (`OTA_STATE_IDLE`, `OTA_STATE_CHECKING`, `OTA_STATE_DOWNLOADING`, `OTA_STATE_UP_TO_DATE`, `OTA_STATE_SUCCESS`, `OTA_STATE_ERROR`) и процент прогресса `m_otaProgress` (0..100%), а также вызывают `vTaskDelay(1)` для сброса аппаратного Watchdog.
        * Вся физическая отрисовка матрицы и вызов `FastLED.show()` выполняются **ИСКЛЮЧИТЕЛЬНО из основного потока `loop()`** в `EffectManager::update()`.
        * Когда `m_otaState != OTA_STATE_IDLE`, `EffectManager::update()` приостанавливает рендер текущих эффектов и отрисовывает кадр OTA:
          - При `isPoweredOn() == false` (Stealth Mode) матрица удерживается полностью черной (0% свечения).
          - При `isPoweredOn() == true`:
            * `CHECKING` (3 клика кнопки / запрос в фоне): по верхнему кольцу `Y=15` быстро вращается бело-голубая точка-радар.
            * `UP_TO_DATE` (версия актуальна): два верхних кольца `Y=14..15` дважды плавно мигают изумрудно-зеленым светом (1.2 с) с возвратом к эффекту.
            * `DOWNLOADING`: плавное заполнение снизу вверх `0%..100%` бирюзово-синим светом с дыханием кромки.
            * `SUCCESS`: 1.2-секундная изумрудно-зеленая волна по всему цилиндру перед безопасным `ESP.restart()`.
            * `ERROR`: два верхних кольца дважды мигают красно-оранжевым цветом с возвратом к эффекту.
- **Буфер статуса UTF-8:** Размер буфера статуса `m_status` равен `192` байтам, чтобы кириллические строки в UTF-8 никогда не обрезались.

## 4. Архитектура Веб-интерфейса (`src/network/web_server.*`)
- Работает на базе стандартного `WebServer` (порт 80) из состава Arduino Core 3.x без тяжелых сторонних зависимостей.
- Фронтенд (HTML/CSS/JS) встроен во Flash как сжатая строка `PROGMEM` в `src/network/webui_html.h`, чтобы веб-интерфейс работал всегда, даже после OTA-обновления одного только `firmware.bin`.
- **Векторный Favicon:** Добавить векторную SVG-иконку вкладки браузера (Favicon в виде светящейся неоновой лампы) через `<link rel="icon" type="image/svg+xml" href="data:image/svg+xml,...">` в `webui_html.h` и маршрут `GET /favicon.ico` в `WebServer`.
- **Независимый глобальный блок расписания и интерфейс карточек-аккордеонов (Accordion UI):**
  - Шапка освобождена от общих ползунков.
  - **Глобальный блок «Будильник Рассвет и Таймер Отбой»:** отдельный независимый модуль расписания в WebUI, управляющий системным таймером рассвета и сна поверх любого эффекта и при выключенной лампе.
  - Список из 15 карточек эффектов. При клике на карточку эффект активируется, плавно раскрывая панель индивидуальных настроек:
    - Индивидуальная яркость (`brightness` 1..255).
    - Ползунок «Температура света (1800K — 4000K)» (`param1` / `colorTemp` 0..255, отображается только для карточки «Ночник»).
    - Индивидуальная скорость (`speed` 1..255, отображается только для динамических эффектов).
    - Чувствительность микрофона (`sensitivity` 1..255, только для `AudioReactive`).
    - Чекбокс «В цикле кнопки» (`inCycle`) на каждой карточке (сохраняется в битовую маску `cycle_mask` в NVS).
- **Потокобезопасный ручной OTA через браузер (`POST /api/ota/upload`):**
  - При загрузке `firmware.bin` через браузер `WebServer` переводит `EffectManager` в режим паузы отрисовки эффектов (`m_otaState = OTA_STATE_DOWNLOADING`), гарантируя отсутствие коллизий RMT и кормит Watchdog (`yield()`), чтобы ручная прошивка и GitHub OTA работали со 100% надежностью.
- **REST API:**
  - `GET /api/status` — JSON с полной телеметрией (без полей IMU), активным эффектом, индивидуальными параметрами (`brightness`, `speed`, `sensitivity`, `param1` / `colorTemp`), состоянием глобального расписания и битовой маской `cycle_mask`. Любой запрос обновляет таймер активности браузера `m_lastHttpActivityMs`.
  - `GET /favicon.ico` — отдача SVG-иконки лампы для браузеров.
  - `POST /api/control` — изменение питания, активного эффекта, персональных настроек (`brightness`, `speed`, `sensitivity`, `param1` / `colorTemp`), глобального расписания Рассвет/Отбой и маски `cycle_mask`. Обновляет таймер активности браузера.
  - `POST /api/radio/switch` — переключение радиомодуля Wi-Fi <-> Zigbee.
  - `POST /api/ota/check` — запуск проверки и обновления с GitHub Releases.
  - `POST /api/ota/upload` — прямая загрузка файла `firmware.bin` из браузера через `Update`.
  - `GET /api/log/download` — выгрузка полного накопительного журнала сессий Zigbee в виде текстового файла `magiclamp_zigbee_log.txt`.
  - `POST /api/log/clear` — полная очистка журнала сессий (`/zigbee.log` в LittleFS и буфера в NVS).

## 5. Движок эффектов: 15 встроенных эффектов и Apple-like UX (`EffectManager`, `LedMatrix`)
- **Полный отказ от внешних файлов LittleFS и StoreManager:**
  - Все 15 эффектов скомпилированы непосредственно в бинарник прошивки и зарегистрированы в `EffectManager` (индексы `0..14`, `TOTAL_EFFECTS = 15`):
    `0: Nightlight`, `1: DawnDusk`, `2: Fire`, `3: PerlinNoise`, `4: GameOfLife`, `5: LorenzAttractor`, `6: Metaballs`, `7: AudioReactive`, `8: Fireflies`, `9: Aurora`, `10: MatrixRain`, `11: LavaLamp`, `12: Vortex`, `13: Fireworks`, `14: CyberPulse`.
  - Полное удаление `SandHourglass` и исключение передачи `ImuMpu6050` в конструкторы и методы эффектов (включая `GameOfLife`).
- **Восстановление состояния питания (Stealth Recovery):**
  - При старте `EffectManager::begin()` состояние питания (`power_on`) загружается из NVS. Если лампа до перезагрузки была выключена (`power_on == false`), матрица остается полностью погашенной без свечения.
- **Плавный кроссфейд (500 мс):**
  - Буфер предыдущего кадра `m_prevBuffer[256]` в `LedMatrix` / `EffectManager`.
  - При смене текущего эффекта запускается кроссфейд: `CRGB::blend(m_prevBuffer[i], newFrame[i], blendFactor)`.
- **Анимация включения и выключения (600 мс):**
  - Включение: мягкая волна света нарастает снизу вверх (`Y=0..15`) за 600 мс.
  - Выключение: плавное угасание сверху вниз (`Y=15..0`) за 600 мс.
- **Индивидуальные NVS-профили:**
  - Массив структур `EffectProfile` (15 элементов) в NVS `Preferences`:
    `struct EffectProfile { uint8_t brightness; uint8_t speed; uint8_t sensitivity; bool inCycle; uint8_t param1; };`
    где `param1` для `Nightlight` хранит цветовую температуру `colorTemp` (по умолчанию `90` ~ 2700K).
  - Битовая маска `cycle_mask` (uint16_t, по умолчанию `0x7FFF` — все 15 включены): при двойном клике кнопки выбирается следующий эффект, у которого бит `(cycle_mask & (1 << next)) != 0`.
- **Эффект Ночник (`Nightlight`):**
  - Поддержка настройки `colorTemp` (`param1` 0..255): плавное изменение оттенка от янтарно-свечного (1800K, `B=0`) через теплый (2700K) до мягкого тепло-белого (4000K).
- **Эффект Киберпанк-пульс (`CyberPulse`):**
  - Исходная самодостаточная генеративная анимация без микрофона: неоновая кибер-сетка и математические волны, управляемые индивидуальными ползунками `brightness` и `speed`.
- **Спектр режима «Отбой» (`DawnDusk`):**
  - Строгий запрет синего канала (`B = 0` на всем протяжении затухания). Спектр `CRGB(r, g, 0)` с `g = r * 0.28` (~1900K теплый свечной/янтарный).
- **Регулировка скорости (`speed` 1..255):**
  - `Fire`: скорость горения и фаза пламени.
  - `PerlinNoise`: скорость фазового смещения Z/t и масштабирования шума.
  - `GameOfLife`: интервал смены поколений от 400 мс до 40 мс.
  - `LorenzAttractor`: шаг численного интегрирования `dt`.
  - `Metaballs`: скорость перемещения сфер.
  - `Fireflies`: темп перемещения светлячков.
  - `Aurora`, `MatrixRain`, `LavaLamp`, `Vortex`, `CyberPulse`: множитель времени анимации.

## 6. Реальный аппаратный стек Zigbee 3.0 и чистый Boot-Switch (`src/network/zigbee_manager.*`, `EffectManager`)
- **Аппаратный стек IEEE 802.15.4 (Спецификация VlasovLabTech / Яндекс Станция Миди):**
  * В `zigbee_manager.h` установлен стандартный **`Endpoint 1`**: `#define ZIGBEE_LIGHT_ENDPOINT 1`.
  * **Динамический выбор класса конечной точки без нарушения структуры кластеров:**
    - Для Профилей 1 и 2 используется `ZigbeeColorDimmableLight(1)` с `app_device_id = 0x0102` (`ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID`), маской возможностей строго `ZIGBEE_COLOR_CAPABILITY_X_Y | ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP` (`0x0018`, без `HUE_SAT`), диапазоном `setLightColorTemperatureRange(153, 500)` и коллбэком идентификации `onIdentify`.
    - Для Профиля 3 используется чистый `ZigbeeLight(1)` (`0x0100`, базовый On/Off без кластера Color Control).
    - Для Профиля 4 используется чистый `ZigbeeDimmableLight(1)` (`0x0101`, диммер яркости без кластера Color Control).
  * **Профили совместимости Zigbee (Алиса):**
    - Профиль 1 (По умолчанию — VlasovLabTech RGB+CCT `0x0102`): `ZigbeeColorDimmableLight(1)`, `Manufacturer = "VlasovLabTech"`, `Model = "ZD-C6-2D1ARGB-v2"`, `capabilities = X_Y | COLOR_TEMP (0x0018)`, `tempRange(153, 500)`, `onIdentify`.
    - Профиль 2 (MagicLamp RGB+CCT `0x0102`): `ZigbeeColorDimmableLight(1)`, `Manufacturer = "MagicLamp"`, `Model = "MagicLamp-C6"`, `capabilities = X_Y | COLOR_TEMP (0x0018)`, `tempRange(153, 500)`, `onIdentify`.
    - Профиль 3 (Базовый Свет Вкл/Выкл `0x0100` — VlasovLabTech): чистый класс `ZigbeeLight(1)`, `Manufacturer = "VlasovLabTech"`, `Model = "NanoC6-OnOff-Light"`, `onIdentify`.
    - Профиль 4 (Диммируемый Свет `0x0101`): чистый класс `ZigbeeDimmableLight(1)`, `Manufacturer = "MagicLamp"`, `Model = "MagicLamp-Dimmer"`, `onIdentify`.
  * **Ротация IEEE 802.15.4 MAC-адреса и обход кэша координатора:**
    - При смене профиля в WebUI или новом сопряжении флаг `force_pair` выставляется в `true`, а в NVS инкрементируется счетчик `pair_seq`.
    - Перед вызовом `Zigbee.begin()` читается базовый MAC через `esp_read_mac(mac, ESP_MAC_IEEE802154)`, к младшему байту прибавляется смещение `pair_seq` (с учетом выбранного профиля) и задается через `esp_iface_mac_addr_set(mac, ESP_MAC_IEEE802154)`. Это предотвращает отказ сопряжения Яндекс Станцией из-за кэширования старого описания устройства по его MAC.
  * **Регистрация конечной точки и запуск стека:**
    - Добавление активной конечной точки в Zigbee Core через `Zigbee.addEndpoint()`.
    - Если `force_pair == true`: вызов `esp_zb_nvram_erase_at_start(true)` перед `Zigbee.begin()` для полной очистки раздела `zb_storage`.
    - `Zigbee.begin(ZIGBEE_END_DEVICE);`
    - Вызов `Zigbee.setRebootOpenNetwork(180)` для открытия Network Steering на 180 секунд.
    - Флаг `force_pair` сбрасывается в `false` в NVS только после `Zigbee.connected() == true`.
- **Интеграция коллбэков с `EffectManager` и обработка команд Алисы:**
  * Коллбэк идентификации `zbLight.onIdentify(onZigbeeIdentify)`: запускает изумрудную волну `triggerSuccessWave()` и логирует вызов.
  * Коллбэк `onZigbeeLightChangeRgb`:
    - `state == false` -> `effectMgr.powerOff()`.
    - `state == true` -> `effectMgr.powerOn()`, `effectMgr.setBrightness(level)`.
    - Переключение эффектов: оттенок Hue (0..255), полученный от Яндекс Станции (преобразованный из XY координат ZCL), переключает один из 15 встроенных эффектов.
  * Коллбэк `onZigbeeLightChangeTemp`:
    - Включает уютный режим «Ночник» (`effect 0`) и плавно транслирует цветовую температуру `153..500 mireds` в параметр `param1` (255..0).
- **Чистый Boot-Switch через NVS и быструю перезагрузку (`ESP.restart()`):**
  * Из-за совместного использования радиотракта 2.4 ГГц и ограниченного объема памяти одновременная работа Wi-Fi и стека Zigbee 802.15.4 исключена.
  * Режим сохраняется в NVS `Preferences "radio"` (`mode`: 0 = Wi-Fi, 1 = Zigbee; `zb_prof`: 0..3; `force_pair`: bool; `pair_seq`: uint8_t).
  * При переключении в Zigbee лампа проигрывает 2-секундную анимацию схождения пурпурно-золотых колец к экватору, сохраняет `mode = 1`, `force_pair = true` и вызывает `ESP.restart()`.
  * При старте в режиме Zigbee Wi-Fi и WebServer не инициализируются, 100% RAM и радио отданы стеку Zigbee.
  * Индикация поиска сети: на темном фоне в центре (`Y=7..8`) «дышит» фиолетовый поясок Алисы.
  * Индикация успешного сопряжения: как только `Zigbee.connected() == true`, лампа сбрасывает `force_pair = false`, проигрывает 1.5-секундную изумрудно-зеленую волну успеха и переходит к рабочему эффекту.
  * При возврате в Wi-Fi (по 4 кликам кнопки или по таймауту 180 с без сопряжения) в журнал дописывается причина выхода, проигрывается зеленый маяк сверху и выполняется `ESP.restart()` в чистый режим Wi-Fi.
- **Накопительный многострочный логгер (`LittleFS` `/zigbee.log` + NVS multi-line ring buffer):**
  * Все события пишутся методом `appendLog(msg)` с таймштампом `[MM:SS.mmm]` в файл `/zigbee.log` в разделе `LittleFS` (при отказе файловой системы — в кольцевой строковый буфер NVS `Preferences "sys_log"` до 3.5 КБ).
  * Системные события стека перехватываются через `esp_log_set_vprintf`.
  * При выходе в Wi-Fi история лога НЕ затирается!
  * Предоставляются HTTP-эндпоинты: `GET /api/log/download` (скачивание `magiclamp_zigbee_log.txt`) и `POST /api/log/clear` (очистка файла и буфера).
- **Ограничения Flash-памяти:** размер свободного пространства в OTA-разделе составляет около 80 КБ. Любые изменения пишутся максимально компактно без дублирования строковых литералов.

## 7. Конфигурация сборки и оптимизация размера (`platformio.ini`)
- **Размещение в OTA-разделе `0x1C0000` (1 835 008 байт):**
  * Для гарантированного размещения бинарника со стеками Wi-Fi/HTTPS и реального Zigbee (`libzboss` + `libesp_zigbee`) в `platformio.ini` установлены флаги оптимизации:
    ```ini
    build_flags =
        -DFL_NO_EXCEPT=
        -DZIGBEE_MODE_ED=1
        -Os
        -DCORE_DEBUG_LEVEL=0
        -ffunction-sections
        -fdata-sections
        -Wl,--gc-sections
    ```
- **Структура проекта:**
  - `platformio.ini`, `partitions.csv`, `.gitignore`
  - `include/config.h`, `include/version.h`, `include/secrets.h`, `include/secrets.example.h`
  - `src/main.cpp`
  - `src/hal/` — `led_matrix`, `button`, `mic_i2s`
  - `src/effects/` — `effect_manager`, `core_effects`, `math_effects`, `audio_effect`, `extra_effects`
  - `src/network/` — `wifi_manager`, `ota_github`, `web_server`, `webui_html.h`, `zigbee_manager`