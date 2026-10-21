# Доска задач проекта (Blackboard State)

## Правила статусов для агентов:
- `TODO` — задача ждет выполнения.
- `IN_PROGRESS` — задача в работе у Разработчика.
- `QUESTION` — у Разработчика возник вопрос, конвейер ждет ответа человека.
- `REVIEW_PENDING` — код написан и скомпилирован (`pio run` = SUCCESS), ждет проверки QA-Тестировщика.
- `FIX_REQUIRED` — QA-Тестировщик нашел ошибки и вернул задачу на доработку (см. поле `QA_Notes`).
- `DONE` — проверено QA-Тестировщиком и полностью готово.

> **Железное правило Диспетчера:** Редактировать `PRD.md`, `ARCHITECTURE.md` и `GEMINI.md` строго запрещено без прямой Директивы Штаба!

---

## Закрытые фазы:
- **Фаза 1 (Task 1.1 – 1.4):** Базовый скелет и драйверы — `DONE`
- **Фаза 2 (Task 2.1 – 2.4):** Движок 9 базовых эффектов и NVS — `DONE`
- **Фаза 3 (Task 3.1 – 3.4):** Калибровка железа, 10-й эффект, Wi-Fi, WebUI, Silent OTA — `DONE`
- **Фаза 4 (Task 4.1 – 4.7):** Магазин эффектов, EffectVM, v0.6.0 Профили эффектов в NVS, NTP Будильник Рассвет/Отбой, Аудио и Скорость — `DONE`
- **Фаза 5 (Task 5.1 – 5.6):** Apple UX, 15 встроенных эффектов, релизы v0.8.0 – v0.9.0 — `DONE`

---

## Очередь задач — Фаза 3: Калибровка железа, 10-й эффект, Wi-Fi, WebUI и GitHub Silent OTA

### [Task 3.1] Калибровка по результатам тестов на железе (Кнопка, GY-521, Медленный Костер, АРУ звука) и 10-й эффект «Светлячки»
- **Status:** `DONE`
- **Files:** `src/hal/button.h`, `src/hal/button.cpp`, `src/hal/imu_mpu6050.h`, `src/hal/imu_mpu6050.cpp`, `src/hal/mic_i2s.h`, `src/hal/mic_i2s.cpp`, `src/effects/core_effects.h`, `src/effects/core_effects.cpp`, `src/effects/audio_effect.cpp`, `src/effects/effect_manager.h`, `src/effects/effect_manager.cpp`, `src/main.cpp`
- **Description:**
  1. **Кнопка TTP223 (`button.*`, `effect_manager.*`):** Перенести сброс с 5-секундного удержания на 5-кратный клик (`FIVE_CLICKS`). Начинать регулировку яркости через 700 мс удержания кнопки и генерировать событие `LONG_PRESS_HOLD` каждые 40 мс, пока палец удерживает кнопку (HIGH), плавно меняя яркость в реальном времени. При каждом новом удержании менять направление регулировки (вверх <-> вниз).
  2. **Гироскоп GY-521 (`imu_mpu6050.*`):** Убрать отказ инициализации при несовпадении байта `WHO_AM_I` с `0x68` (клоны MPU-6050/6500 возвращают `0x70`, `0x72`, `0x98` и др.). Проверять адреса `0x68` и `0x69` на пинах `SDA=6, SCL=5` (100 кГц): если чип ответил ACK по I2C — будить его (`0x6B = 0x00`) и читать реальные регистры ускорения (`0x3B..0x40`). В режиме Fallback (если чип не ответил ACK) зафиксировать вектор гравитации строго вниз `(0, -1, 0)` без самопроизвольного переворачивания.
  3. **Эффект Костер (`Fire`):** Замедлить скорость подъема и остывания огня в 3 раза, добавить плавную линейную интерполяцию между кадрами тепловой карты для мягкого, тягучего движения на 60 FPS. Добавить параметр скорости эффектов в `EffectManager`.
  4. **Аудио АРУ (`mic_i2s.*`, `audio_effect.cpp`):** Добавить автоматическую регулировку усиления (AGC) с порогом тишины (Noise Gate) и настраиваемым коэффициентом чувствительности.
  5. **10-й эффект «Светлячки» (`Fireflies`):** Добавить 10-й режим в `EffectManager` — 15 светлячков с дробными координатами (`float x, y`), субпиксельной билинейной отрисовкой на бесшовном цилиндре и плавной фазой разгорания/затухания.
- **Acceptance Criteria (Критерии для QA):**
  1. Событие `LONG_PRESS_HOLD` генерируется циклически каждые 40 мс во время удержания кнопки после 700 мс.
  2. Драйвер `ImuMpu6050` работает с любым значением `WHO_AM_I` при наличии ответа по I2C на `0x68`/`0x69`, а в Fallback не вращает гравитацию сам по себе.
  3. Добавлен 10-й эффект `Fireflies`, общее число базовых эффектов равно 10.
  4. Сборка `pio run` завершается со статусом `SUCCESS`.
- **QA_Notes:** Проверено QA: замечаний нет.

---

### [Task 3.2] Менеджер Wi-Fi (`secrets.h` + Captive Portal + mDNS) и модуль облачного автообновления с GitHub (`ota_github`)
- **Status:** `DONE`
- **Files:** `.gitignore`, `include/version.h`, `include/secrets.example.h`, `include/secrets.h`, `src/network/wifi_manager.h`, `src/network/wifi_manager.cpp`, `src/network/ota_github.h`, `src/network/ota_github.cpp`, `src/main.cpp`
- **Description:**
  1. Создать `.gitignore` (исключить `.pio/`, `include/secrets.h`, `.DS_Store`). Создать `include/version.h` (`FIRMWARE_VERSION "0.3.0"`), шаблон `include/secrets.example.h` и рабочий `include/secrets.h` (если отсутствует).
  2. Реализовать неблокирующий `WiFiManager`: подключение к роутеру из `secrets.h` / NVS, запуск mDNS (`http://magiclamp.local`) и `ArduinoOTA`, а при отсутствии связи с роутером — поднятие точки доступа `MagicLamp-Setup`.
  3. Реализовать `OtaGithub` (`src/network/ota_github.*`) согласно разделу 3 `ARCHITECTURE.md`: проверка `https://api.github.com/repos/gaterkik/MagicLamp/releases/latest` с `WiFiClientSecure` (`setInsecure()`) и `setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS)`, сравнение версий, скачивание `firmware.bin` через `<Update.h>` и перезагрузка. Привязать вызов проверки к тройному клику кнопки, таймеру (через 30 сек после старта и раз в 30 мин).
- **Acceptance Criteria (Критерии для QA):**
  1. Файл `include/secrets.h` внесен в `.gitignore`.
  2. В `OtaGithub` включена поддержка HTTP 302 редиректов (`HTTPC_STRICT_FOLLOW_REDIRECTS`), обязательная для скачивания бинарников с GitHub Releases, и заголовок `User-Agent`.
  3. Работа Wi-Fi не блокирует отрисовку матрицы 60 FPS, когда проверка OTA не идет.
  4. Сборка `pio run` завершается со статусом `SUCCESS`.
- **QA_Notes:** Проверено QA: замечаний нет.

---

### [Task 3.3] Встроенный Веб-интерфейс (`WebUI`) с пультом управления, телеметрией и кнопкой обновления OTA
- **Status:** `DONE`
- **Files:** `src/network/webui_html.h`, `src/network/web_server.h`, `src/network/web_server.cpp`, `src/main.cpp`
- **Description:**
  Реализовать встроенный веб-сервер на порту 80 и одностраничное веб-приложение в `webui_html.h` (PROGMEM) согласно разделу 4 `ARCHITECTURE.md`:
  - Отображение телеметрии: температура процессора ESP32-C6 (°C), Wi-Fi RSSI (dBm), IP-адрес, Uptime, свободная память, статус датчика GY-521 и текущая версия `FIRMWARE_VERSION`.
  - Управление: кнопка Вкл/Выкл, ползунки яркости, скорости эффекта и чувствительности микрофона, кнопки выбора каждого из 10 эффектов по названию.
  - Блок OTA: кнопка «Проверить и обновить с GitHub» (`POST /api/ota/check`) со статусом проверки + форма прямой загрузки файла `firmware.bin` из браузера (`POST /api/ota/upload`).
- **Acceptance Criteria (Критерии для QA):**
  1. Все эндпоинты (`/`, `/api/status`, `/api/control`, `/api/ota/check`, `/api/ota/upload`) реализованы и связаны с `EffectManager`, `MicI2S` и `OtaGithub`.
  2. Интерфейс полностью автономный (без внешних CDN-ссылок, работает в локальной сети).
  3. Сборка `pio run` завершается со статусом `SUCCESS`.
- **QA_Notes:** Проверено QA: замечаний нет.

---

### [Task 3.4] Доводка v0.4.0: Индикация границ яркости, Костер 3/4 с искрами, Авто-поиск пинов GY-521 (SDA/SCL) и подготовка релиза
- **Status:** `DONE`
- **Files:** `include/version.h`, `src/effects/effect_manager.h`, `src/effects/effect_manager.cpp`, `src/effects/core_effects.h`, `src/effects/core_effects.cpp`, `src/hal/imu_mpu6050.h`, `src/hal/imu_mpu6050.cpp`, `src/network/web_server.cpp`, `src/network/ota_github.cpp`
- **Description:**
  1. **Версия прошивки (`include/version.h`):** Поднять `FIRMWARE_VERSION` до `"0.4.0"`.
  2. **Четкое управление яркостью (`effect_manager.*`):**
     - Во время одного удержания кнопки (`LONG_PRESS_HOLD`) яркость должна изменяться строго в одном направлении до упора (`MIN = 12`, `MAX = 255`) и НЕ разворачиваться обратно, пока пользователь не отпустит кнопку.
     - Добавить гамма-коррекцию/нелинейный шаг изменения яркости (на малых значениях шаг 2–3 ед., на высоких — 6–8 ед.), чтобы глаз видел равномерную скорость изменения.
     - **Визуальная отсечка предела:** В момент достижения `MAX (255)` верхнее кольцо цилиндра (`Y=15`) должно дважды коротко вспыхнуть ярко-белым цветом. В момент достижения `MIN (12)` нижнее кольцо (`Y=0`) должно дважды коротко вспыхнуть бирюзовым цветом.
  3. **Эффект «Костер» — высота 3/4 и взлетающие искры (`core_effects.*`):**
     - Вернуть высокий коэффициент остывания (`cooling`), чтобы основное тело пламени занимало только нижние 2/3–3/4 высоты лампы (`Y = 0..10`), а верхняя часть (`Y = 11..15`) оставалась темной (выключенной).
     - Добавить систему одиночных ярких искр (Sparks), которые периодически отрываются от верхушек пламени, пролетают вверх сквозь темную зону `Y = 11..15` и плавно гаснут.
     - Сохранить умеренную, неторопливую скорость движения огня.
  4. **Авто-определение пинов I2C для GY-521 (`imu_mpu6050.*`, `web_server.cpp`):**
     - При инициализации `ImuMpu6050::begin()` сначала сканировать адреса `0x68` и `0x69` на стандартной паре пинов `SDA = GPIO 6, SCL = GPIO 5` (100 кГц, с подтяжкой `pinMode(..., INPUT_PULLUP)`).
     - Если ни одно устройство не ответило ACK — автоматически выполнить `Wire.end()` и просканировать зеркальную пару пинов `SDA = GPIO 5, SCL = GPIO 6` (на случай, если провода SDA и SCL перепутаны местами при пайке).
     - Если датчик не найден ни на одной паре, периодически (раз в 5 секунд) повторять попытку инициализации в фоне и выводить в `/api/status` (и в WebUI) подробную диагностику: `Online (SDA:6, SCL:5, addr:0x68, who:0x..)` или `Not Found on GPIO 5/6`.
  5. **Понятный статус GitHub 404 (`ota_github.cpp`):** Если GitHub API вернул HTTP 404, выводить в статус: `"Релизов на GitHub пока нет (HTTP 404)"`.
- **Acceptance Criteria (Критерии для QA):**
  1. `FIRMWARE_VERSION` равна `"0.4.0"`.
  2. При удержании кнопки яркость останавливается на границах 12 и 255 и отрисовывает визуальную индикацию на кольцах `Y=0` / `Y=15`.
  3. В эффекте `Fire` верхняя четверть цилиндра темная с пролетающими вверх одиночными искрами.
  4. `ImuMpu6050` проверяет обе комбинации пинов `(SDA=6, SCL=5)` и `(SDA=5, SCL=6)`.
  5. Сборка `pio run` завершается со статусом `SUCCESS`.
- **QA_Notes:** Проверено QA: замечаний нет.

---

## Фаза 4 (v0.5.0): Магазин эффектов на GitHub + LittleFS + EffectVM

---

### [Task 4.1] Файловая система LittleFS, движок подгружаемых эффектов `EffectVM` (.eff) и динамическая регистрация в `EffectManager`
- **Status:** `DONE`
- **Files:** `include/version.h`, `src/effects/effect_vm.h`, `src/effects/effect_vm.cpp`, `src/effects/effect_manager.h`, `src/effects/effect_manager.cpp`, `src/main.cpp`
- **Description:**
  1. Поднять `FIRMWARE_VERSION` в `include/version.h` до `"0.5.0"`.
  2. НЕ менять `partitions.csv` (использовать существующий раздел littlefs).
  3. Реализовать `src/effects/effect_vm.h` и `src/effects/effect_vm.cpp` с поддержкой чтения компактных `.eff` файлов из LittleFS (`/effects/<id>.eff`), настраиваемых палитр и 6 параметрических/шейдерных режимов рендеринга на цилиндре 16x16:
     - `aurora`: Северное сияние — волнообразные вертикальные ленты с шумом Перлина
     - `matrix_rain`: Цифровой дождь «Матрица» с бегущими вертикальными шлейфами
     - `lava_lamp`: Лава-лампа — медленная тепловая конвекция крупных восковых капель
     - `vortex`: Галактическая спираль — вращающиеся спиральные рукава вокруг цилиндра
     - `fireworks`: Фейерверк — пуск ракет снизу вверх и кольцевой разлет искр с гравитацией
     - `cyber_pulse`: Киберпанк-пульс — неоновые кольца и математический волновой шейдер с реакцией на звук/время
  4. Расширить `EffectManager`: помимо 10 базовых эффектов (0..9), поддерживать динамический список загруженных `.eff` эффектов из `/effects/*.eff` (индексы 10..N, до 24 эффектов суммарно) с переключением двойным кликом кнопки и из WebUI.
- **Acceptance Criteria (Критерии для QA):**
  1. `FIRMWARE_VERSION` в `include/version.h` установлена в `"0.5.0"`.
  2. Файл `partitions.csv` не изменен (сохранен исходный раздел littlefs).
  3. `EffectVM` поддерживает разбор параметров `.eff` и все 6 алгоритмов (`aurora`, `matrix_rain`, `lava_lamp`, `vortex`, `fireworks`, `cyber_pulse`).
  4. `EffectManager` поддерживает базовые индексы 0..9 и динамические индексы 10..N с безопасным циклическим переключением.
  5. Сборка `pio run` завершается со статусом `SUCCESS`.
- **QA_Notes:** Проверено QA: замечаний нет.


---

### [Task 4.2] Модуль `StoreManager`, REST API магазина и секция «Магазин эффектов» в `WebUI`
- **Status:** `DONE`
- **Files:** `src/network/store_manager.h`, `src/network/store_manager.cpp`, `src/network/web_server.h`, `src/network/web_server.cpp`, `src/network/webui_html.h`, `src/main.cpp`
- **Description:**
  1. Реализовать `src/network/store_manager.h` и `src/network/store_manager.cpp` (инициализация `LittleFS.begin(true)`, сканирование `/effects/*.eff` и регистрация в `EffectManager`, скачивание `.eff` по HTTPS с GitHub Raw в `/effects/<id>.eff` с `HTTPC_STRICT_FOLLOW_REDIRECTS`, удаление эффектов).
  2. Добавить в `web_server.*` эндпоинты `/api/store/installed`, `/api/store/catalog`, `/api/store/install`, `/api/store/delete` и обновить `/api/status` (передавать полный список доступных в лампе эффектов: 10 базовых + установленные из LittleFS).
  3. Обновить `webui_html.h`: динамическая отрисовка кнопок всех установленных эффектов в пульте управления + новая секция «Магазин эффектов (GitHub Effect Store)» с индикатором памяти LittleFS, кнопкой «Обновить витрину», карточками эффектов из `manifest.json` и кнопками «Скачать в лампу», «Включить», «Удалить».
- **Acceptance Criteria (Критерии для QA):**
  1. `StoreManager` корректно монтирует LittleFS, управляет установкой/удалением `.eff` файлов и динамической регистрацией в `EffectManager`.
  2. Реализованы все эндпоинты `/api/store/*` и обновлен `/api/status`.
  3. В WebUI отображается индикатор памяти LittleFS, каталог с GitHub и кнопки действий.
  4. Сборка `pio run` завершается со статусом `SUCCESS`.
- **QA_Notes:** Проверено QA: замечаний нет.


---

### [Task 4.3] Создание витрины магазина в репозитории (`store/manifest.json` и 6 эффектов `.eff`)
- **Status:** `DONE`
- **Files:** `store/manifest.json`, `store/effects/aurora.eff`, `store/effects/matrix_rain.eff`, `store/effects/lava_lamp.eff`, `store/effects/vortex.eff`, `store/effects/fireworks.eff`, `store/effects/cyber_pulse.eff`
- **Description:**
  1. Создать в корне проекта папку `store/effects/` и 6 файлов эффектов с параметрами: `aurora.eff`, `matrix_rain.eff`, `lava_lamp.eff`, `vortex.eff`, `fireworks.eff`, `cyber_pulse.eff`.
  2. Создать каталог `store/manifest.json` с метаданными всех 6 эффектов (`id`, `name`, `description` на русском языке, `version`, `author`, `size`, `url` на `https://raw.githubusercontent.com/gaterkik/MagicLamp/main/store/effects/<id>.eff`).
- **Acceptance Criteria (Критерии для QA):**
  1. Папка `store/effects/` содержит все 6 файлов `.eff`.
  2. `store/manifest.json` является валидным JSON и содержит полные метаданные всех 6 эффектов.
  3. Формат `.eff` файлов совместим с парсером `EffectVM`.
- **QA_Notes:** Проверено QA: замечаний нет.

---

### [Task 4.4] Стабилизация TLS/HTTPS для GitHub OTA (исправление ошибки -1) и глубокая диагностика шины GY-521 (v0.5.1)
- **Status:** `DONE`
- **Files:** `include/version.h`, `src/network/wifi_manager.cpp`, `src/network/ota_github.h`, `src/network/ota_github.cpp`, `src/network/store_manager.h`, `src/network/store_manager.cpp`, `src/hal/imu_mpu6050.h`, `src/hal/imu_mpu6050.cpp`, `src/network/web_server.cpp`
- **Description:**
  1. **Версия прошивки (`include/version.h`):** Поднять `FIRMWARE_VERSION` до `"0.5.1"`.
  2. **Стабильность HTTPS/TLS (`wifi_manager.cpp`, `ota_github.cpp`, `store_manager.cpp`):**
     - В `wifi_manager.cpp`: вызывать `WiFi.setSleep(false)` для отключения энергосбережения радиомодуля Wi-Fi и отключать точку доступа AP (`WiFi.enableAP(false)`) при успешном подключении к STA.
     - В `ota_github.cpp` и `store_manager.cpp`: установить таймаут TLS-рукопожатия и сокета 15 с (`setTimeout(15000)` / `setHandshakeTimeout(15)`), реализовать до 3 автоматических повторов с задержкой при коде соединения `< 0` (например, ошибка `-1`).
     - В `ota_github.cpp`: при скачивании бинарника релиза (HTTP 302 редирект с `github.com` на `objects.githubusercontent.com`) производить чистое разделение TLS-сессий — перехватывать заголовок `Location`, закрывать соединение с `github.com` и создавать новый чистый экземпляр `WiFiClientSecure` с рукопожатием непосредственно к целевому хосту `objects.githubusercontent.com` для скачивания `firmware.bin` во флеш.
  3. **Глубокая диагностика I2C и авто-поиск GY-521 (`imu_mpu6050.h`, `imu_mpu6050.cpp`, `web_server.cpp`):**
     - Проверка электрических уровней линий перед инициализацией I2C (`digitalRead` на пинах в режиме `INPUT` до вызова `Wire.begin`): на линиях должен быть логический `HIGH`. Если зафиксирован `LOW`, формировать точный диагностический статус: `"No pull-up / Check 3.3V on GY-521"`.
     - Авто-сканер пар пинов среди свободных GPIO (`GPIO 4, 5, 6, 8, 9`) на адресах `0x68` и `0x69` на случай сдвига маркировки платы ESP32-C6.
     - Вывод детального статуса в `/api/status` и WebUI (например, `Online (SDA:6, SCL:5, addr:0x68, who:0x..)` или `No pull-up / Check 3.3V on GY-521 (SDA:LOW, SCL:HIGH)` или `Not Found (SDA/SCL HIGH, no ACK on GPIO 4,5,6,8,9)`).
- **Acceptance Criteria (Критерии для QA):**
  1. `FIRMWARE_VERSION` в `include/version.h` равна `"0.5.1"`.
  2. `WiFi.setSleep(false)` вызывается при подключении Wi-Fi, радиомодуль не засыпает, AP отключается в STA режиме.
  3. `OtaGithub` и `StoreManager` имеют таймаут 15 с, 3 повтора при коде соединения `< 0`, а `OtaGithub` корректно разделяет TLS-сессии при HTTP 302 редиректе на `objects.githubusercontent.com`.
  4. `ImuMpu6050` проверяет электрические уровни SDA/SCL до старта I2C и сканирует комбинации свободных пинов (`GPIO 4, 5, 6, 8, 9`) с выдачей понятной диагностики в `/api/status` и WebUI.
  5. Сборка `pio run` завершается со статусом `SUCCESS`.
- **QA_Notes:** Проверено QA: замечаний нет.

---

### [Task 4.5] Релиз v0.5.2: Монтирование LittleFS с NVS-резервом для Магазина эффектов, потоковый парсер GitHub OTA, Bus Recovery для GY-521 и вертикальные языки пламени Костра
- **Status:** `DONE`
- **Files:** `include/version.h`, `src/network/store_manager.h`, `src/network/store_manager.cpp`, `src/network/ota_github.cpp`, `src/hal/imu_mpu6050.h`, `src/hal/imu_mpu6050.cpp`, `src/effects/core_effects.cpp`
- **Description:**
  1. **Версия прошивки (`include/version.h`):** Поднять `FIRMWARE_VERSION` до `"0.5.2"`.
  2. **Монтирование LittleFS и NVS-резерв (`src/network/store_manager.*`):**
     - Монтировать LittleFS с явным указанием метки: `LittleFS.begin(true, "/littlefs", 10, "littlefs")`, а при неудаче делать резервную попытку `LittleFS.begin(true, "/littlefs", 10, "spiffs")`.
     - Если раздел LittleFS на текущей разметке Flash не монтируется — автоматически переключать хранение скачанных `.eff` файлов в энергонезависимую память `Preferences` (NVS, namespace `"eff_store"`) и RAM-кэш, чтобы скачивание, сохранение, удаление и запуск `.eff` эффектов из Магазина работали безотказно даже без раздела LittleFS.
  3. **Потоковый парсер GitHub OTA (`src/network/ota_github.cpp`):**
     - Устранить ошибку `Parse error (no firmware.bin)`: извлекать `tag_name` потоковым поиском или фильтром без усечения крупного JSON.
     - При совпадении с `FIRMWARE_VERSION` сразу возвращать статус `"Установлена актуальная версия (v0.5.2)"`.
     - Для загрузки использовать найденный `browser_download_url` или прямой детерминированный URL `https://github.com/gaterkik/MagicLamp/releases/latest/download/firmware.bin` (если поле `browser_download_url` не попало в буфер).
  4. **I2C Bus Recovery и подбор пинов GY-521 (`src/hal/imu_mpu6050.*`):**
     - Убрать блокировку при `SCL:LOW`.
     - Перед стартом `Wire.begin()` выполнять аппаратную процедуру разблокировки шины (9 тактовых импульсов на пине `SCL` в режиме `OUTPUT`), переводить в `INPUT_PULLUP`.
     - Выполнять полный перебор пар пинов `SDA/SCL` из набора `{4, 5, 6, 3}` через `Wire.begin()`.
     - В статус выводить уровни всех проверенных пинов (`GPIO4:.., GPIO5:.., GPIO6:..`).
  5. **Эффект «Костер» (`src/effects/core_effects.cpp`):**
     - Убрать избыточное горизонтальное размытие («рябь на воде»).
     - Ввести неоднородную вертикальную маску тяги и остывания по столбцам `X (0..15)` на основе шума Перлина, чтобы формировались отдельные выразительные вертикальные языки пламени разной высоты (`Y=0..12`).
     - Количество искр снизить в 3 раза (не более 1–2 одновременно), цвет искр — строго красно-оранжевый/алый уголек (`CRGB(220, 35, 0)` -> `CRGB(100, 5, 0)`), без белого цвета.
- **Acceptance Criteria (Критерии для QA):**
  1. `FIRMWARE_VERSION` в `include/version.h` равна `"0.5.2"`.
  2. `StoreManager` монтирует LittleFS с меткой раздела `"littlefs"` (и fallback `"spiffs"`), а при невозможности монтирования прозрачно сохраняет/читает/удаляет `.eff` файлы через NVS `Preferences` (`"eff_store"`) и RAM-кэш.
  3. `OtaGithub` потоково извлекает `tag_name`, сообщает об актуальной версии при совпадении и использует прямой GitHub URL для `firmware.bin` как fallback.
  4. `ImuMpu6050` реализует I2C Bus Recovery (9 импульсов на SCL), не блокируется при чтении уровней, перебирает пары из `{4, 5, 6, 3}` и выводит уровни пинов в статус.
  5. `Fire` формирует вертикальные языки пламени без горизонтальной ряби, искр не более 1–2 цвета тлеющих алых углей без белого цвета.
  6. Сборка `pio run` завершается со статусом `SUCCESS`.
- **QA_Notes:** Проверено QA: замечаний нет.

---

### [Task 4.6] Релиз v0.5.3: Программный Open-Drain I2C драйвер для GY-521 (без Wire), мягкий подсев в «Игре Жизнь», буфер UTF-8 статуса OTA и Favicon в WebUI
- **Status:** `DONE`
- **Files:** `include/version.h`, `src/hal/imu_mpu6050.h`, `src/hal/imu_mpu6050.cpp`, `src/effects/math_effects.h`, `src/effects/math_effects.cpp`, `src/network/ota_github.h`, `src/network/ota_github.cpp`, `src/network/webui_html.h`, `src/network/web_server.cpp`
- **Description:**
  1. **Версия прошивки (`include/version.h`):** Поднять `FIRMWARE_VERSION` до `"0.5.3"`.
  2. **Программный Open-Drain I2C драйвер для GY-521 (`src/hal/imu_mpu6050.*`):**
     - Полностью отказаться от библиотеки `<Wire.h>` из-за конфликта менеджера пинов `esp32-hal-periman` в Arduino Core 3.x.
     - Реализовать чистый программный Open-Drain Bit-Bang I2C драйвер с полупериодом `delayMicroseconds(5)`, где лог. `1` устанавливается переводом пина в `pinMode(pin, INPUT_PULLUP)` (высокоимпедансное состояние с подтяжкой), а лог. `0` — через `pinMode(pin, OUTPUT)` + `digitalWrite(pin, LOW)`.
     - Сканировать все адреса `0x08..0x77` сначала на стандартной паре `(SDA=6, SCL=5)`. Если ACK не получен ни от одного устройства — сканировать на зеркальной паре `(SDA=5, SCL=6)`.
     - При обнаружении ответа (на `0x68`, `0x69` или любом другом I2C-адресе): выводить точный адрес и регистр `WHO_AM_I` (0x75) в статус `Online (addr:0x.., who:0x.., SDA:.., SCL:..)`, будить чип записью `0x00` в регистр `0x6B` (PWR_MGMT_1) и читать регистры ускорения `0x3B..0x40`.
     - При отсутствии ACK на обеих парах выводить точный статус с уровнями линий: `No I2C ACK on 5/6 (SDA:HIGH, SCL:HIGH)` (или фактические уровни SDA/SCL).
  3. **Мягкий органический подсев в «Игре Жизнь» (`src/effects/math_effects.*`):**
     - Убрать резкую принудительную очистку всего экрана по таймеру поколений.
     - При детекции стагнации (вымирание популяции, отсутствие изменений сетки или цикл периода 2 в течение > 4 секунд) НЕ стирать выжившие клетки, а выполнять мягкий локальный подсев новых фигур (планер / glider, r-pentomino или компактный кластер) в свободной области цилиндра со свежим оттенком и плавным разгоранием.
  4. **Буфер статуса UTF-8 для OTA (`src/network/ota_github.*`):**
     - Расширить размер буфера `m_status` с 64 до 192 байт, чтобы кириллические статусы (например, `"Установлена актуальная версия (v0.5.3)"`) никогда не усекались.
  5. **Векторный Favicon и маршрут /favicon.ico (`src/network/webui_html.h`, `src/network/web_server.cpp`):**
     - Добавить векторную SVG-иконку светящейся неоновой лампы в `<head>` через `<link rel="icon" type="image/svg+xml" href="data:image/svg+xml,...">`.
     - Добавить в `web_server.cpp` обработчик маршрута `GET /favicon.ico`, отдающий SVG Favicon со статусом 200 и типом `image/svg+xml`.
- **Acceptance Criteria (Критерии для QA):**
  1. `FIRMWARE_VERSION` равна `"0.5.3"`.
  2. В `imu_mpu6050.cpp` не используется `<Wire.h>`, реализован чистый Open-Drain Bit-Bang I2C со сканированием `0x08..0x77` на `(6,5)` и `(5,6)`.
  3. В `GameOfLife` стагнация разрешается локальным подсевом без обнуления всего экрана.
  4. Буфер `m_status` в `OtaGithub` >= 192 байт, в `webui_html.h` присутствует `<link rel="icon"`.
  5. Сборка `pio run` завершается со статусом `SUCCESS`.
- **QA_Notes:** Проверено QA: замечаний нет.

---

### [Task 4.7] Релиз v0.6.0: Индивидуальные профили настроек эффектов в NVS, работа ползунка скорости во всех эффектах, восстановление взрывного Аудио-режима, NTP-расписание Рассвет/Отбой и устранение микро-фризов Костра
- **Status:** `DONE`
- **Files:** `include/version.h`, `src/effects/effect_manager.h`, `src/effects/effect_manager.cpp`, `src/effects/core_effects.h`, `src/effects/core_effects.cpp`, `src/effects/math_effects.h`, `src/effects/math_effects.cpp`, `src/effects/audio_effect.cpp`, `src/effects/effect_vm.h`, `src/effects/effect_vm.cpp`, `src/hal/mic_i2s.h`, `src/hal/mic_i2s.cpp`, `src/hal/imu_mpu6050.h`, `src/hal/imu_mpu6050.cpp`, `src/network/webui_html.h`, `src/network/web_server.h`, `src/network/web_server.cpp`, `src/network/wifi_manager.h`, `src/network/wifi_manager.cpp`, `src/main.cpp`
- **Description:**
  1. **Версия прошивки (`include/version.h`):** Поднять `FIRMWARE_VERSION` до `"0.6.0"`.
  2. **Индивидуальные профили настроек эффектов в NVS и адаптивный WebUI (`effect_manager.*`, `webui_html.h`, `web_server.*`):**
     - Реализовать хранение персональных профилей настроек (`brightness`, `speed`, `sensitivity`) для каждого эффекта (по индексу эффекта) с отложенным сохранением в NVS Preferences (debounce 2 с).
     - При переключении эффекта (кнопкой или через WebUI) автоматически загружать его индивидуальные параметры.
     - В WebUI динамически показывать только те регуляторы, которые применимы к активному эффекту:
       * Ползунок «Скорость» показывается для эффектов: `Fire` (Костер), `PerlinNoise` (Шум Перлина), `GameOfLife` (Игра Жизнь), `LorenzAttractor` (Аттрактор Лоренца), `Metaballs` (Метаболы), `Fireflies` (Светлячки), а также для подгружаемых эффектов `aurora`, `matrix_rain`, `lava_lamp`, `vortex`, `cyber_pulse`.
       * Для эффектов `Nightlight` (Ночник), `SandHourglass` (Песочные часы), `fireworks` (Фейерверк), `DawnDusk` (Рассвет/Закат) и `AudioReactive` (Реакция на звук) ползунок «Скорость» в WebUI скрывается.
       * Ползунок «Чувствительность микрофона» отображается только при выборе аудио-эффекта (`AudioReactive`).
  3. **Регулировка скорости во всех эффектах (`core_effects.*`, `math_effects.*`, `effect_vm.*`):**
     - Привязать параметр `speed` (1..255) к `PerlinNoise` (шаг фазы Z/t), `GameOfLife` (задержка поколения от 400 мс на min до 40 мс на max), `LorenzAttractor` (шаг интегрирования `dt` и число подшагов), `Metaballs` (скорость движения сфер), `EffectVM` (`lava_lamp`, `matrix_rain`, `aurora`, `vortex`, `cyber_pulse` — множитель `dt`).
  4. **Эффект Ночник (`Nightlight` в `core_effects.cpp`):**
     - Пересчитать кривую яркости и распределение мощности так, чтобы визуальная яркость равномерно нарастала во всем диапазоне ползунка от 1% до 100% без заклинивания на 50% в плато лимитера тока 2500 мА.
  5. **Восстановление взрывного Аудио-режима (`mic_i2s.*`, `audio_effect.cpp`):**
     - Устранить баг «оглушения» после хлопка/громкого звука: убрать накапливающийся адаптивный `noiseFloor`.
     - Применить фиксированный низкий Noise Gate (только для фильтрации фонового шума тракта), быструю симметричную релаксацию АРУ и прямое масштабирование по ползунку `sensitivity`.
     - Обеспечить живой отклик столбиков спектра на всю высоту цилиндра (0..15) и пробивание яркой световой волны снизу до самого верха при хлопке или басе.
  6. **NTP-синхронизация и расписание Рассвет / Отбой (`wifi_manager.*`, `core_effects.*`, `web_server.*`, `webui_html.h`):**
     - Синхронизировать реальное время через NTP (`pool.ntp.org`, часовой пояс `GMT+2` / `UTC+2`, смещение +7200 с) при подключении Wi-Fi.
     - Сохранять в NVS настройки расписания: флаг активности, время рассвета (будильника) `HH:MM` (плавный подъем за 5-15 минут до назначенного времени от темно-янтарного к теплому солнечному) и время отбоя `HH:MM` (включение янтарного света 2000K и плавное затухание в течение 30 минут до полного выключения). Вне расписания ручной режим `DawnDusk` работает как спокойный закатный светильник.
  7. **Устранение микро-фризов Костра (`imu_mpu6050.*`):**
     - Проверять физический контакт линий SDA/SCL через кратковременный `pinMode(pin, INPUT_PULLDOWN)` на 50 мкс: так как на модуле GY-521 есть внешние подтяжки 2.2 кОм к 3.3V, реальный контакт дает `HIGH`, а висящий в воздухе пин — `LOW`.
     - В фоновом ретрае Fallback проверять ТОЛЬКО адреса `0x68` и `0x69` (не более 0.3 мс), исключив сканирование 112 адресов и вызываемые им микро-подвисания анимации Костра.
- **Acceptance Criteria (Критерии для QA):**
  1. `FIRMWARE_VERSION` в `include/version.h` равна `"0.6.0"`.
  2. Персональные профили `brightness`, `speed`, `sensitivity` сохраняются в NVS и загружаются при выборе эффекта.
  3. Ползунок скорости скрывается для Nightlight, SandHourglass, fireworks, DawnDusk, AudioReactive и отображается для остальных; ползунок чувствительности отображается только для AudioReactive.
  4. Регулировка скорости работает в PerlinNoise, GameOfLife, LorenzAttractor, Metaballs и EffectVM.
  5. В Nightlight яркость плавно нарастает от 1% до 100% без плато на 50%.
  6. В AudioReactive устранено глушение после хлопков, столбики активны во всем диапазоне 0..15, волна пробивает цилиндр до верха.
  7. NTP синхронизирует время по GMT+2, расписание Dawn/Dusk настраивается в WebUI и отрабатывает сценарии рассвета/сна.
  8. Фоновый опрос GY-521 проверяет только 0x68/0x69 и тестирует подтяжку pulldown-ом без фризов в Fire.
  9. Сборка `pio run` завершается со статусом `SUCCESS`.
- **QA_Notes:** Проверено QA: замечаний нет.

---

## Фаза 5 (v0.7.0): Apple UX, 16 встроенных эффектов, карточки-аккордеоны с галочками в WebUI, аппаратный Wire для GY-521 и Zigbee-модуль

### [Task 5.1] Релиз v0.7.0 (Ядро и UX): 16 встроенных эффектов (отказ от LittleFS), кроссфейд 500 мс, янтарный Отбой без синего, шкала прогресса OTA на матрице и аппаратный Wire для GY-521
- **Status:** `DONE`
- **Files:** `include/version.h`, `src/hal/led_matrix.h`, `src/hal/led_matrix.cpp`, `src/hal/imu_mpu6050.h`, `src/hal/imu_mpu6050.cpp`, `src/effects/effect_manager.h`, `src/effects/effect_manager.cpp`, `src/effects/core_effects.cpp`, `src/effects/extra_effects.h`, `src/effects/extra_effects.cpp`, `src/network/ota_github.cpp`, `src/network/web_server.cpp`, `src/main.cpp`
- **Description:**
  1. **Версия прошивки (`include/version.h`):** Поднять `FIRMWARE_VERSION` до `"0.7.0"`.
  2. **16 встроенных эффектов в прошивке (`effect_manager.*`, `extra_effects.*`):**
     - Полностью убрать `StoreManager` и зависимость от `LittleFS` в логике эффектов.
     - Перенести 6 эффектов из `EffectVM` (`aurora`, `matrix_rain`, `lava_lamp`, `vortex`, `fireworks`, `cyber_pulse`) в постоянные нативные эффекты C++ (например, `src/effects/extra_effects.*`), зарегистрировав их в `EffectManager` как индексы `10..15` (всего 16 эффектов: `0..15`).
     - Сохранять индивидуальные профили в NVS (`brightness`, `speed`, `sensitivity`, `inCycle`) для всех 16 эффектов.
     - Поддерживать битовую маску `cycle_mask` (uint16_t, по умолчанию `0xFFFF`): двойной клик кнопки переключает только те эффекты, у которых включен флаг `inCycle` (бит `cycle_mask`).
  3. **Спектр режима «Отбой» (`DawnDusk` в `core_effects.cpp`):**
     - В режиме «Отбой» (затухание перед сном) и закатном свечении категорически запретить синюю и фиолетовую компоненту (`B = 0` строго на всем протяжении!).
     - Использовать исключительно теплый янтарный/свечной спектр `CRGB(r, g, 0)`, где `g = r * 0.28` (цветовая температура около 1900K), безопасный для мелатонина.
  4. **Apple UX анимации матрицы (`led_matrix.*`, `effect_manager.*`):**
     - Плавный кроссфейд (500 мс) между предыдущим и новым эффектом при любом переключении (кнопкой или через WebUI). Хранить буфер предыдущего кадра `m_prevBuffer[256]` и выполнять `CRGB::blend()`.
     - Анимация включения лампы (волна света нарастает снизу вверх `Y=0..15` за 600 мс) и выключения (плавное затухание сверху вниз `Y=15..0` за 600 мс).
     - Шкала OTA на матрице (`LedMatrix::showOtaProgress(uint8_t percent, bool success, bool error)`):
       * При скачивании 0..100% плавное заполнение снизу вверх бирюзово-синим светом с мягкой «дышащей» кромкой. Прямой вызов из коллбэка `Update.onProgress`.
       * При 100% перед перезагрузкой отрисовать 100% заполнение и 1.2-секундную изумрудно-зеленую волну успешного завершения.
       * При ошибке — 3 мягких красных импульса матрицы и возврат к эффекту.
  5. **Аппаратный Wire и диагностика GY-521 (`imu_mpu6050.*`, `web_server.cpp`):**
     - Первой попыткой выполнять чистый аппаратный `Wire.begin(6, 5, 100000)` БЕЗ предварительных вызовов `pinMode()`.
     - При отсутствии ACK переключаться на резервный Open-Drain сканер.
     - Реализовать метод `runDiagnostics()` и эндпоинт `GET /api/imu/diag`, проверяющий шину на 100 кГц и 10 кГц с подробным отчетом.
- **Acceptance Criteria (Критерии для QA):**
  1. `FIRMWARE_VERSION` в `include/version.h` равна `"0.7.0"`.
  2. В прошивке доступно ровно 16 встроенных эффектов (индексы 0..15), `StoreManager` удален, профили каждого эффекта и битовая маска `inCycle` сохраняются в NVS.
  3. В режиме «Отбой» эффекта `DawnDusk` синий канал строго равен 0 (`B = 0`), спектр теплый янтарный.
  4. Кроссфейд при смене эффектов длится 500 мс, анимация включения (0..15 снизу вверх) и выключения (15..0 сверху вниз) длится 600 мс.
  5. `LedMatrix::showOtaProgress` визуализирует заполнение 0..100%, изумрудно-зеленую волну при успехе и красные импульсы при ошибке.
  6. `ImuMpu6050` использует чистый `Wire.begin(6, 5, 100000)` без `pinMode`, эндпоинт `GET /api/imu/diag` возвращает диагностику.
  7. Сборка `pio run` завершается со статусом `SUCCESS`.
- **QA_Notes:** Проверено QA: замечаний нет

---

### [Task 5.2] Релиз v0.7.0 (WebUI & Zigbee): Интерфейс карточек-аккордеонов с галочками цикла кнопки и модуль переключения радио Wi-Fi / Zigbee (4 клика)
- **Status:** `DONE`
- **Files:** `src/hal/button.h`, `src/hal/button.cpp`, `src/network/zigbee_manager.h`, `src/network/zigbee_manager.cpp`, `src/network/webui_html.h`, `src/network/web_server.h`, `src/network/web_server.cpp`, `src/main.cpp`
- **Description:**
  1. **Интерфейс карточек-аккордеонов в WebUI (`webui_html.h`, `web_server.*`):**
     - Убрать общие ползунки и блок расписания из шапки страницы.
     - Реализовать список из 16 карточек эффектов (Accordion UI). При клике на карточку активируется выбранный эффект и раскрывается панель:
       * Индивидуальный ползунок «Яркость».
       * Индивидуальный ползунок «Скорость» (только для динамических эффектов).
       * Индивидуальный ползунок «Чувствительность микрофона» (только для `AudioReactive` и `CyberPulse`).
       * Блок «Расписание (Будильник Рассвет и Таймер Отбой)» (раскрывается ТОЛЬКО внутри карточки `DawnDusk`).
       * Чекбокс «В цикле кнопки» (`inCycle`) на каждой карточке (сохраняется в битовую маску NVS).
     - Добавить кнопку «Тест датчика» в блоке телеметрии с вызовом `GET /api/imu/diag` и отображением результатов.
     - Добавить элемент переключения режима радио `Wi-Fi` <-> `Zigbee`.
  2. **Модуль Zigbee и переключение радио (`button.*`, `zigbee_manager.*`, `main.cpp`):**
     - Добавить событие `BUTTON_EVENT_FOUR_CLICKS` (4 клика подряд) в `Button`.
     - Создать модуль `src/network/zigbee_manager.h` и `src/network/zigbee_manager.cpp` для управления режимом радиомодуля ESP32-C6 (флаг режима в NVS, переключение по 4 кликам кнопки или по `POST /api/radio/switch`).
     - При переключении проигрывать анимацию на матрице: 3 бирюзовые волны для Wi-Fi, 3 фиолетовые волны для Zigbee.
- **Acceptance Criteria (Критерии для QA):**
  1. В `webui_html.h` реализован аккордеон из 16 карточек эффектов с индивидуальными настройками и чекбоксами `inCycle`.
  2. Блок расписания доступен только в карточке `DawnDusk`.
  3. Кнопка «Тест датчика» опрашивает `/api/imu/diag` и выводит результат в WebUI.
  4. Кнопка TTP223 генерирует `BUTTON_EVENT_FOUR_CLICKS` при 4 кликах, переключая режим радио с анимацией 3 волн на матрице.
  5. Эндпоинт `POST /api/radio/switch` переключает режим радио и сохраняет его в NVS.
  6. Сборка `pio run` завершается со статусом `SUCCESS`.
- **QA_Notes:** Проверено QA: замечаний нет.

---

### [Task 5.3] Релиз v0.8.0: Регулировка цветовой температуры Ночника, аудио-реактивность CyberPulse, Бесшумный ночной OTA (Stealth Mode) и авто-переключение Wi-Fi / Zigbee
- **Status:** `DONE`
- **Files:** `include/version.h`, `src/effects/effect_manager.h`, `src/effects/effect_manager.cpp`, `src/effects/core_effects.h`, `src/effects/core_effects.cpp`, `src/effects/extra_effects.h`, `src/effects/extra_effects.cpp`, `src/hal/imu_mpu6050.h`, `src/hal/imu_mpu6050.cpp`, `src/network/ota_github.h`, `src/network/ota_github.cpp`, `src/network/webui_html.h`, `src/network/web_server.h`, `src/network/web_server.cpp`, `src/network/zigbee_manager.h`, `src/network/zigbee_manager.cpp`, `src/network/wifi_manager.h`, `src/network/wifi_manager.cpp`, `src/main.cpp`
- **Description:**
  1. **Версия прошивки (`include/version.h`):** Поднять `FIRMWARE_VERSION` до `"0.8.0"`.
  2. **Регулировка температуры света Ночника (`Nightlight` в `effect_manager.*`, `core_effects.*`, `webui_html.h`):**
     - Добавить поле `uint8_t param1` в `EffectProfile` (для Ночника это `colorTemp`, по умолчанию `90` ~ 2700K).
     - Сохранять `param1` в NVS для профилей эффектов, отдавать в `GET /api/status` и принимать в `POST /api/control`.
     - В `EffectNightlight`: плавно менять оттенок от янтарно-свечного (1800K, `B=0`) через теплый (2700K) до мягкого тепло-белого (4000K) на основе `colorTemp`.
     - В `webui_html.h`: в раскрывающейся карточке «Ночник» отображать ползунок «Температура света (1800K — 4000K)».
  3. **Реальная аудио-реактивность Киберпанк-пульс (`CyberPulse` в `extra_effects.*`, `effect_manager.*`):**
     - Подключить `MicI2S` к рендеру `CyberPulse`: в тишине — плавное движение неоновой сетки (пурпурный/циан/фиолетовый); при наличии звука 16 полос FFT (`getBand(0..15)`) динамически модулируют высоту и свечение столбцов по окружности цилиндра, а детектор бита (`isBeat()`) запускает от центра (`Y=7..8`) к краям (`0` и `15`) яркие расходящиеся ударные кольца с учетом ползунка `sensitivity`.
  4. **Бесшумный ночной OTA (Stealth OTA при выключенной лампе):**
     - Сохранять состояние питания лампы (`isPoweredOn`) в NVS при любых изменениях и восстанавливать в `EffectManager::begin()`.
     - Если лампа выключена (`!isPoweredOn()`), во время OTA-обновления матрица остается ПОЛНОСТЬЮ темной (0% свечения, без шкал и вспышек), а после перезагрузки лампа запускается в выключенном состоянии.
     - Если лампа включена (`isPoweredOn()`), воспроизводить полноценную анимацию прогресса (0..100%) и волну успеха.
  5. **Умная автоматика Wi-Fi <-> Zigbee (`zigbee_manager.*`, `wifi_manager.*`, `web_server.*`):**
     - При включении лампа поднимает Wi-Fi на 60 секунд (NTP GMT+2, OTA check).
     - Любое обращение к WebServer (открытый браузер) продлевает сеанс Wi-Fi на 10 минут (`m_lastHttpActivityMs`).
     - Если режим Zigbee активирован и нет веб-активности 10 минут — автоматический переход в Zigbee.
     - Раз в сутки в 04:00 GMT+2 авто-включение Wi-Fi на 45 секунд (проверка обновлений в Stealth Mode при выключенной лампе) и возврат в Zigbee.
     - 4 клика кнопки принудительно включают Wi-Fi на 10 минут.
     - `ZigbeeColorDimmableLight`: управление вкл/выкл, яркостью и маппинг цветов/оттенков Алисы на 16 эффектов.
  6. **Внутренние подтяжки и диагностика GY-521 (`imu_mpu6050.*`):**
     - Принудительно включать `gpio_set_pull_mode(GPIO_NUM_5, GPIO_PULLUP_ONLY)` и `gpio_set_pull_mode(GPIO_NUM_6, GPIO_PULLUP_ONLY)` перед стартом I2C.
     - В `/api/imu/diag` выводить точные поля проверки подтяжек: `gpio5_ext_pullup` и `gpio6_ext_pullup`.
- **Acceptance Criteria (Критерии для QA):**
  1. `FIRMWARE_VERSION` в `include/version.h` равна `"0.8.0"`.
  2. В карточке «Ночник» в WebUI есть ползунок «Температура света (1800K — 4000K)», меняющий оттенок от свечного до мягкого белого с сохранением в NVS (`param1`).
  3. В `CyberPulse` задействованы все 16 полос FFT `getBand(0..15)` и детектор бита `isBeat()`, чувствительность масштабируется ползунком `sensitivity`.
  4. При выключенной лампе OTA проходит в Stealth Mode (матрица 0% свечения, после рестарта лампа выключена).
  5. Автоматика Wi-Fi/Zigbee отрабатывает таймер 10 мин активности WebUI, стартовое окно 60 с, ночное окно 04:00 и 4 клика кнопки; команды Алисы маппятся на эффекты.
  6. В `imu_mpu6050.cpp` включены подтяжки `gpio_set_pull_mode` для пинов 5 и 6, эндпоинт `/api/imu/diag` возвращает `gpio5_ext_pullup` и `gpio6_ext_pullup`.
  7. Сборка `pio run` завершается со статусом `SUCCESS`.
- **QA_Notes:** Проверено QA: замечаний нет.

---

### [Task 5.4] Релиз v0.8.1: Потокобезопасный OTA без коллизии RMT (исправление обрыва Flashing), независимый системный Будильник Рассвет/Отбой, индикация радара при 3 кликах и новая геометрия индикации Wi-Fi/Zigbee
- **Status:** `DONE`
- **Files:** `include/version.h`, `src/network/ota_github.h`, `src/network/ota_github.cpp`, `src/effects/effect_manager.h`, `src/effects/effect_manager.cpp`, `src/effects/core_effects.h`, `src/effects/core_effects.cpp`, `src/network/webui_html.h`, `src/network/web_server.h`, `src/network/web_server.cpp`, `src/network/zigbee_manager.h`, `src/network/zigbee_manager.cpp`, `src/hal/button.cpp`, `src/main.cpp`
- **Description:**
  1. **Версия прошивки (`include/version.h`):** Поднять `FIRMWARE_VERSION` до `"0.8.1"`.
  2. **Потокобезопасный OTA без аппаратной коллизии RMT (`ota_github.*`, `effect_manager.*`, `web_server.cpp`):**
     - Полностью убрать прямые вызовы `FastLED.show()` и `LedMatrix::showOtaProgress()` из фоновой FreeRTOS задачи `otaTask` и коллбэка `Update.onProgress`.
     - В `EffectManager` ввести потокобезопасные поля состояния: `m_otaState` (`OTA_STATE_IDLE`, `OTA_STATE_CHECKING`, `OTA_STATE_DOWNLOADING`, `OTA_STATE_UP_TO_DATE`, `OTA_STATE_SUCCESS`, `OTA_STATE_ERROR`) и `m_otaProgress` (0..100).
     - Задача `otaTask` и коллбэк `Update.onProgress` только обновляют эти поля и вызывают `vTaskDelay(1)`.
     - Всю физическую отрисовку на матрице (`FastLED.show()`) выполнять ИСКЛЮЧИТЕЛЬНО из основного потока `loop()` в `EffectManager::update()`. При `m_otaState != OTA_STATE_IDLE` блокировать рендер обычных эффектов:
       * Если лампа выключена (`!isPoweredOn()`) — держать матрицу черной (Stealth Mode).
       * Если лампа включена:
         - `CHECKING` (3 клика / ручная проверка): по верхнему кольцу `Y=15` быстро вращается яркая бело-голубая точка-радар, пока идет запрос к серверу.
         - `UP_TO_DATE` (обновление не требуется): два верхних кольца `Y=14..15` дважды плавно мигают изумрудно-зеленым цветом (1.2 с) и лампа возвращается к эффекту.
         - `DOWNLOADING`: плавное заполнение снизу вверх `0%..100%` бирюзово-синим светом с дыханием кромки.
         - `SUCCESS`: 1.2-секундная изумрудно-зеленая волна по всему цилиндру перед `ESP.restart()`.
         - `ERROR`: два верхних кольца дважды мигают красно-оранжевым цветом с возвратом к эффекту.
     - В `WebServer` (`/api/ota/upload`): при приеме загрузки `firmware.bin` через браузер переводить `EffectManager` в режим паузы эффектов (`OTA_STATE_DOWNLOADING`) и кормить Watchdog (`yield()`).
  3. **Независимый системный Будильник Рассвет и Таймер Отбой (`effect_manager.*`, `webui_html.h`, `web_server.*`, `core_effects.*`):**
     - Вынести настройки «Рассвет» и «Отбой» из карточки отдельного эффекта в независимый глобальный планировщик внутри `EffectManager`.
     - В WebUI вынести блок расписания в отдельную независимую карточку-панель, доступную всегда.
     - Логика Рассвета: даже если лампа выключена (после любого вечернего эффекта), за 5 минут до назначенного времени будильника (например, в 06:55 при будильнике на 07:00) лампа автоматически включает питание в режиме Рассвета и плавно разгорается от тлеющих углей до теплого солнца к назначенному времени.
     - Логика Отбоя: в назначенное время отбоя (например, 23:00), независимо от того, какой эффект был включен, лампа автоматически переключается в безопасное для сна янтарное свечение (1900K, строго `B = 0`) и плавно гаснет в течение 30 минут до полного выключения (`powerOn = false`) в 23:30.
  4. **Контрастная геометрическая индикация переключения Wi-Fi / Zigbee (`zigbee_manager.*`, `effect_manager.*`):**
     - При переключении в `Wi-Fi`: горизонтальное вращение ярко-зеленого/лазурного луча-маяка по двум верхним кольцам (`Y=14..15`) в течение 1.8 с.
     - При переключении в `Zigbee`: вертикальное схождение двух колец (сверху `Y=15` и снизу `Y=0`) навстречу друг другу к экватору лампы (`Y=7..8`) со вспышкой теплого пурпурно-золотого цвета в центре. В режиме активного поиска хаба Яндекс Станции центральное кольцо (`Y=7..8`) мягко «дышит» фиолетовым цветом Алисы.
- **Acceptance Criteria (Критерии для QA):**
  1. `FIRMWARE_VERSION` в `include/version.h` равна `"0.8.1"`.
  2. В задаче `otaTask` и `Update.onProgress` нет ни одного вызова `FastLED.show()`; рендер всех стадий OTA (`CHECKING`, `DOWNLOADING`, `SUCCESS`, `UP_TO_DATE`, `ERROR`) выполняется строго в `EffectManager::update()` в основном потоке `loop()`.
  3. При 3 кликах отображается анимация радара на кольце `Y=15`; при совпадении версий — двойное зеленое мигание колец `Y=14..15`; при ошибке сети — красно-оранжевое мигание `Y=14..15`.
  4. Расписание Рассвет/Отбой вынесено в независимый системный модуль: Будильник включает лампу из `OFF` с плавным разгоранием, Отбой переводит любой эффект в 30-минутное янтарное затухание (`B=0`) с выключением.
  5. В WebUI блок «Будильник Рассвет и Таймер Отбой» оформлен как независимый системный блок.
  6. Индикация Wi-Fi и Zigbee имеет разную геометрию (горизонтальный маяк сверху для Wi-Fi, вертикальное схождение колец к экватору для Zigbee).
  7. Сборка `pio run` завершается со статусом `SUCCESS`.
- **QA_Notes:** Проверено QA: замечаний нет.

---

### [Task 5.5] Релиз v0.8.2: Исправление алгоритма окна Рассвета и Отбоя, кнопка «Заснуть сейчас», часы в WebUI и драйвер GY-6500 (MPU-6500) с Clock Stretching без INPUT_PULLDOWN
- **Status:** `DONE`
- **Files:** `include/version.h`, `src/effects/effect_manager.h`, `src/effects/effect_manager.cpp`, `src/network/webui_html.h`, `src/network/web_server.h`, `src/network/web_server.cpp`, `src/hal/imu_mpu6050.h`, `src/hal/imu_mpu6050.cpp`, `src/main.cpp`
- **Description:**
  1. **Версия прошивки (`include/version.h`):** Поднять `FIRMWARE_VERSION` до `"0.8.2"`.
  2. **Интуитивный расчет интервалов Рассвета и Отбоя (`effect_manager.*`, `web_server.*`):**
     - Целевое время задает момент **завершения** сценария:
       * «Рассвет»: включается ровно за `dawnDurationMin` минут ДО указанного времени будильника (`5, 10, 15, 30` мин, по умолчанию `5`), плавно набирая яркость до 100% к моменту `dawnHour:dawnMinute`.
       * «Отбой»: за `duskDurationMin` минут ДО указанного времени отбоя (`15, 30, 45` мин, по умолчанию `30`) или немедленно, если время отбоя установлено в пределах `[now .. now + duskDurationMin]`, лампа перехватывает управление у текущего эффекта, включает янтарный свет (1900K, строго `B = 0`) и плавно снижает яркость до полного выключения (`powerOn = false`) ровно в целевую минуту `duskHour:duskMinute`.
     * Поддержка быстрого действия «Заснуть сейчас» (`POST /api/schedule` с параметром `sleepNow: true` или `POST /api/schedule/sleep_now`): немедленный запуск янтарного затухания на `duskDurationMin` минут с последующим выключением.
     * Сохранение настроек `dawnDurationMin`, `duskDurationMin` в NVS Preferences.
  3. **Интерфейс расписания в WebUI (`webui_html.h`, `web_server.*`):**
     - Отображение текущего системного времени лампы (`ЧЧ:ММ:СС GMT+2`).
     - Селекторы длительности: Рассвет (`5, 10, 15, 30 мин`), Отбой (`15, 30, 45 мин`).
     - Кнопка быстрого действия «Заснуть сейчас (Таймер сна)».
     - Индикатор активной фазы (например: `Рассвет активен (осталось X мин)` / `Таймер сна активен (осталось X мин)` / `Ожидание расписания`).
  4. **Драйвер GY-6500 (MPU-6500) с Clock Stretching без INPUT_PULLDOWN (`imu_mpu6050.*`):**
     - Полностью исключить `INPUT_PULLDOWN` и вызовы `Wire.end()` с повторным аппаратным захватом пинов, приводящие к притягиванию GPIO 6 к GND.
     - Единый программный Open-Drain Bit-Bang I2C драйвер (50 кГц, полупериод 10 мкс) с поддержкой I2C Clock Stretching (ожидание освобождения SCL чипом до 500 мкс).
     - Поддержка MPU-6500 (`WHO_AM_I = 0x70`) и MPU-6050 (`WHO_AM_I = 0x68`): сброс (`0x6B = 0x80`), пауза 50 мс, пробуждение (`0x6B = 0x01`), включение всех осей (`0x6C = 0x00`), диапазон ±2g (`0x1C = 0x00`).
     - В `/api/imu/diag` выводить `raw_ax, raw_ay, raw_az`, `who_am_i`, уровни пинов в покое `GPIO5_idle`, `GPIO6_idle` при `INPUT_PULLUP` и рабочую пару пинов.
- **Acceptance Criteria (Критерии для QA):**
  1. `FIRMWARE_VERSION` в `include/version.h` равна `"0.8.2"`.
  2. Будильник Рассвет включается ровно за `dawnDurationMin` ДО времени будильника и набирает 100% яркость к целевому моменту.
  3. Таймер Отбой гаснет за `duskDurationMin` ДО целевого времени (или запускается немедленно, если время внутри ближайшего окна затухания) с полным выключением лампы в целевой момент; кнопка «Заснуть сейчас» мгновенно запускает затухание на `duskDurationMin`.
  4. В WebUI отображаются текущее время лампы, селекторы длительности, кнопка «Заснуть сейчас» и индикация активной фазы.
  5. В коде `imu_mpu6050` нет `INPUT_PULLDOWN` и `Wire.end()`, реализован I2C Clock Stretching, инициализация MPU-6500 (регистры `0x6B = 0x01`, `0x6C = 0x00`), а `/api/imu/diag` возвращает `raw_ax/ay/az`, `who_am_i`, `GPIO5_idle`, `GPIO6_idle`.
  6. Сборка `pio run` завершается со статусом `SUCCESS`.
- **QA_Notes:** Проверено QA: замечаний нет.

---

### [Task 5.6] Релиз v0.9.0: Полное удаление IMU и эффекта Песочные часы (15 эффектов), возврат исходного CyberPulse без микрофона и перенос сброса Wi-Fi на 7 кликов
- **Status:** `DONE`
- **Files:** `include/version.h`, `include/config.h`, `src/hal/button.h`, `src/hal/button.cpp`, `src/effects/effect_manager.h`, `src/effects/effect_manager.cpp`, `src/effects/math_effects.h`, `src/effects/math_effects.cpp`, `src/effects/extra_effects.h`, `src/effects/extra_effects.cpp`, `src/network/webui_html.h`, `src/network/web_server.h`, `src/network/web_server.cpp`, `src/main.cpp`
- **Description:**
  1. Поднять `FIRMWARE_VERSION` в `include/version.h` до `"0.9.0"`.
  2. Удалить файлы `src/hal/imu_mpu6050.h` и `src/hal/imu_mpu6050.cpp`, очистить `include/config.h`, `src/main.cpp`, `src/effects/math_effects.*`, `src/effects/effect_manager.*` от всех упоминаний IMU и эффекта `SandHourglass` (оставить 15 эффектов, индексы 0..14).
  3. Вернуть эффект `CyberPulse` к исходному виду без привязки к `MicI2S` и скрыть для него ползунок микрофона в `webui_html.h` (оставить только ползунки Яркость и Скорость).
  4. В `src/hal/button.*` и `src/main.cpp` перенести событие сброса Wi-Fi с 5 кликов на 7 кликов (`BUTTON_EVENT_SEVEN_CLICKS`).
  5. Удалить из `src/network/web_server.*` и `src/network/webui_html.h` маршрут `/api/imu/diag`, кнопку «Тест датчика» и строку статуса гироскопа.
- **Acceptance Criteria (Критерии для QA):**
  1. `FIRMWARE_VERSION` равна `"0.9.0"`.
  2. В проекте отсутствуют файлы `imu_mpu6050.*`, эффект `SandHourglass` и эндпоинт `/api/imu/diag`; общее число эффектов равно 15.
  3. Эффект `CyberPulse` не использует микрофон и регулируется ползунками Яркость и Скорость.
  4. Сброс Wi-Fi срабатывает строго по 7 кликам (`BUTTON_EVENT_SEVEN_CLICKS`).
  5. Сборка `pio run` завершается со статусом `SUCCESS`.
- **QA_Notes:** Проверено QA: замечаний нет.