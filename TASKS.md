# Доска задач проекта (Blackboard State)

## Правила статусов для агентов:
- `TODO` — задача ждет выполнения.
- `IN_PROGRESS` — задача в работе у Разработчика.
- `QUESTION` — у Разработчика возник вопрос, конвейер ждет ответа человека.
- `REVIEW_PENDING` — код написан и скомпилирован (`pio run` = SUCCESS), ждет проверки QA-Тестировщика.
- `FIX_REQUIRED` — QA-Тестировщик нашел ошибки и вернул задачу на доработку (см. поле `QA_Notes`).
- `DONE` — проверено QA-Тестировщиком и полностью готово.

> **Железное правило Диспетчера:** Задачи Фазы 1 и Фазы 2 полностью закрыты (`DONE`). Выполняй задачи **Фазы 3** в режиме Фазового Автопилота (`Task 3.1` -> `Task 3.2` -> `Task 3.3`). Редактировать `PRD.md`, `ARCHITECTURE.md` и `GEMINI.md` строго запрещено!

---

## Закрытые фазы:
- **Фаза 1 (Task 1.1 – 1.4):** Базовый скелет и драйверы — `DONE`
- **Фаза 2 (Task 2.1 – 2.4):** Движок 9 базовых эффектов и NVS — `DONE`

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