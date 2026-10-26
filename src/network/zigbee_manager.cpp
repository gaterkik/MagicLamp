#include "zigbee_manager.h"
#include "wifi_manager.h"
#include "effects/effect_manager.h"
#include "ota_github.h"
#include "esp_zigbee_cluster.h"
#include <time.h>

struct ZbProfileDef {
    uint16_t device_id;
    const char* manufacturer;
    const char* model;
    uint16_t capabilities;
};

static const ZbProfileDef PROFILES[4] = {
    { 0x010D, "_TZ3210_jd3z4yig", "TS0505B", ZIGBEE_COLOR_CAPABILITY_HUE_SATURATION | ZIGBEE_COLOR_CAPABILITY_X_Y | ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP },
    { 0x010D, "Yandex", "YNDX-00531", ZIGBEE_COLOR_CAPABILITY_HUE_SATURATION | ZIGBEE_COLOR_CAPABILITY_X_Y | ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP },
    { 0x0102, "SIGNIFY", "LCT015", ZIGBEE_COLOR_CAPABILITY_HUE_SATURATION | ZIGBEE_COLOR_CAPABILITY_X_Y },
    { 0x0101, "IKEA of Sweden", "TRADFRI bulb E27 W opal 1000lm", 0 }
};

static MagicZigbeeLight zbLight(ZIGBEE_LIGHT_ENDPOINT);
static EffectManager* s_effMgr = nullptr;
static LedMatrix* s_ledMatrix = nullptr;
static ZigbeeManager* s_zbManager = nullptr;

MagicZigbeeLight::MagicZigbeeLight(uint8_t endpoint) : ZigbeeColorDimmableLight(endpoint) {}

void MagicZigbeeLight::setAppDeviceId(uint16_t deviceId) {
    _ep_config.app_device_id = deviceId;
    _device_id = (esp_zb_ha_standard_devices_t)deviceId;
}

void MagicZigbeeLight::setupBasicClusterAttributes() {
    esp_zb_attribute_list_t *basic_cluster = esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_BASIC, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    if (basic_cluster) {
        uint8_t zcl_ver = 8;
        uint8_t app_ver = 1;
        uint8_t stack_ver = 2;
        uint8_t hw_ver = 1;
        if (esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID, &zcl_ver) != ESP_OK) {
            esp_zb_cluster_update_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID, &zcl_ver);
        }
        if (esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID, &app_ver) != ESP_OK) {
            esp_zb_cluster_update_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID, &app_ver);
        }
        if (esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID, &stack_ver) != ESP_OK) {
            esp_zb_cluster_update_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID, &stack_ver);
        }
        if (esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_HW_VERSION_ID, &hw_ver) != ESP_OK) {
            esp_zb_cluster_update_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_HW_VERSION_ID, &hw_ver);
        }
    }
    setPowerSource(ZB_POWER_SOURCE_MAINS);
}

static void onZigbeeLightChangeRgb(bool state, uint8_t red, uint8_t green, uint8_t blue, uint8_t level) {
    if (!s_effMgr) return;
    uint8_t profId = s_zbManager ? (s_zbManager->getProfile() + 1) : 1;
    char logBuf[128];
    if (!state) {
        snprintf(logBuf, sizeof(logBuf), "[Профиль %d] Выкл", profId);
        ZigbeeManager::saveLog(logBuf);
        s_effMgr->powerOff();
        return;
    }
    s_effMgr->powerOn();
    s_effMgr->setBrightness(level);

    CRGB col = CRGB(red, green, blue);
    CHSV hsv = rgb2hsv_approximate(col);
    snprintf(logBuf, sizeof(logBuf), "[Профиль %d] Вкл, ярк: %d, цвет: Hue %d, Sat %d", profId, level, hsv.hue, hsv.sat);
    ZigbeeManager::saveLog(logBuf);

    if (s_ledMatrix) {
        ZigbeeManager::handleZigbeeColor(hsv.hue, hsv.sat, *s_effMgr, *s_ledMatrix);
    }
}

static void onZigbeeLightChangeHsv(bool state, uint8_t hue, uint8_t saturation, uint8_t value) {
    if (!s_effMgr) return;
    uint8_t profId = s_zbManager ? (s_zbManager->getProfile() + 1) : 1;
    char logBuf[128];
    if (!state) {
        snprintf(logBuf, sizeof(logBuf), "[Профиль %d] Выкл", profId);
        ZigbeeManager::saveLog(logBuf);
        s_effMgr->powerOff();
        return;
    }
    s_effMgr->powerOn();
    s_effMgr->setBrightness(value);

    snprintf(logBuf, sizeof(logBuf), "[Профиль %d] Вкл, ярк: %d, цвет: Hue %d", profId, value, hue);
    ZigbeeManager::saveLog(logBuf);

    if (s_ledMatrix) {
        ZigbeeManager::handleZigbeeColor(hue, saturation, *s_effMgr, *s_ledMatrix);
    }
}

static void onZigbeeLightChangeTemp(bool state, uint8_t level, uint16_t color_temperature) {
    if (!s_effMgr) return;
    uint8_t profId = s_zbManager ? (s_zbManager->getProfile() + 1) : 1;
    char logBuf[128];
    if (!state) {
        snprintf(logBuf, sizeof(logBuf), "[Профиль %d] Выкл", profId);
        ZigbeeManager::saveLog(logBuf);
        s_effMgr->powerOff();
        return;
    }
    s_effMgr->powerOn();
    s_effMgr->setBrightness(level);

    snprintf(logBuf, sizeof(logBuf), "[Профиль %d] Вкл, ярк: %d, ЦТ: %u mireds", profId, level, color_temperature);
    ZigbeeManager::saveLog(logBuf);

    if (s_ledMatrix) {
        ZigbeeManager::handleZigbeeColorTemp(color_temperature, *s_effMgr, *s_ledMatrix);
    }
}

ZigbeeManager::ZigbeeManager()
    : m_mode(RADIO_MODE_WIFI),
      m_activeRadio(RADIO_MODE_WIFI),
      m_zbProfile(0),
      m_matrix(nullptr),
      m_lastHttpActivityMs(0),
      m_wifiStayActiveUntilMs(0),
      m_nightlyCheckDone(false),
      m_pairingActive(false),
      m_pairingStartMs(0),
      m_connectedSuccessDone(false) {}

void ZigbeeManager::setProfile(uint8_t prof) {
    if (prof > 3) prof = 0;
    m_zbProfile = prof;
    Preferences prefs;
    prefs.begin("radio", false);
    prefs.putUChar("zb_prof", prof);
    prefs.end();
    Serial.printf("[ZigbeeManager] Profile set to %d (%s / %s)\n", prof, PROFILES[prof].manufacturer, PROFILES[prof].model);
}

void ZigbeeManager::saveLog(const char* logMsg) {
    if (!logMsg) return;
    Preferences prefs;
    prefs.begin("sys_log", false);
    prefs.putString("last_zb_log", logMsg);
    prefs.end();
    Serial.printf("[ZigbeeLog] %s\n", logMsg);
}

String ZigbeeManager::getLog() {
    Preferences prefs;
    prefs.begin("sys_log", true);
    String res = prefs.getString("last_zb_log", "Нет записей о сессиях Zigbee");
    prefs.end();
    return res;
}

void ZigbeeManager::begin(LedMatrix* matrix, EffectManager* eff) {
    m_matrix = matrix;
    s_ledMatrix = matrix;
    s_effMgr = eff;
    s_zbManager = this;

    Preferences prefs;
    prefs.begin("radio", true);
    m_mode = prefs.getUChar("mode", RADIO_MODE_WIFI);
    m_zbProfile = prefs.getUChar("zb_prof", 0);
    bool forcePairing = prefs.getBool("force_pair", false);
    prefs.end();

    if (m_mode > 1) {
        m_mode = RADIO_MODE_WIFI;
    }
    if (m_zbProfile > 3) {
        m_zbProfile = 0;
    }

    // Configure Zigbee endpoint attributes and callbacks according to active profile
    const ZbProfileDef& p = PROFILES[m_zbProfile];
    zbLight.setAppDeviceId(p.device_id);
    zbLight.setManufacturerAndModel(p.manufacturer, p.model);
    zbLight.setLightColorCapabilities(p.capabilities);
    zbLight.setupBasicClusterAttributes();

    zbLight.onLightChangeRgb(onZigbeeLightChangeRgb);
    zbLight.onLightChangeHsv(onZigbeeLightChangeHsv);
    zbLight.onLightChangeTemp(onZigbeeLightChangeTemp);

    // Register Endpoint 1 in Zigbee Core
    Zigbee.addEndpoint(&zbLight);

    if (m_mode == RADIO_MODE_ZIGBEE) {
        m_activeRadio = RADIO_MODE_ZIGBEE;
        Serial.printf("[ZigbeeManager] Starting hardware Zigbee 3.0 stack (Profile %d: %s / %s, EP %d)...\n",
                      m_zbProfile + 1, p.manufacturer, p.model, ZIGBEE_LIGHT_ENDPOINT);

        if (forcePairing) {
            Serial.println(F("[ZigbeeManager] force_pairing is true: erasing NVRAM via esp_zb_nvram_erase_at_start(true)..."));
            esp_zb_nvram_erase_at_start(true);
        }

        if (!Zigbee.begin(ZIGBEE_END_DEVICE)) {
            Serial.println(F("[ZigbeeManager] ERROR: Zigbee.begin() failed! Returning to Wi-Fi..."));
            saveLog("Ошибка инициализации Zigbee.begin(ZIGBEE_END_DEVICE)");
            switchToWifi("Ошибка старта Zigbee стека", eff);
            return;
        }

        // Open network search (Network Steering) for 180 seconds
        Zigbee.setRebootOpenNetwork(180);
        m_pairingActive = true;
        m_pairingStartMs = millis();
        m_connectedSuccessDone = false;

        char logBuf[128];
        snprintf(logBuf, sizeof(logBuf), "[Профиль %d] Zigbee 3.0 End Device активен (%s, EP %d). Поиск сети (до 180 с)...",
                 m_zbProfile + 1, p.model, ZIGBEE_LIGHT_ENDPOINT);
        saveLog(logBuf);

        if (eff) {
            eff->setZigbeePairingMode(true);
        }
        Serial.println(F("[ZigbeeManager] Zigbee stack active. Steering open for 180s. Purple belt breathing."));
    } else {
        // Wi-Fi mode
        m_activeRadio = RADIO_MODE_WIFI;
        m_lastHttpActivityMs = millis();
        m_wifiStayActiveUntilMs = millis() + BOOT_WIFI_WINDOW_MS;
        m_nightlyCheckDone = false;
        m_pairingActive = false;
        m_connectedSuccessDone = false;
        Serial.println(F("[ZigbeeManager] Configured for Wi-Fi mode. Zigbee hardware stack dormant."));
    }
}

void ZigbeeManager::switchToZigbee(EffectManager* eff) {
    Serial.println(F("[ZigbeeManager] Switching to Zigbee 3.0: saving NVS and restarting..."));
    char logBuf[128];
    snprintf(logBuf, sizeof(logBuf), "[Профиль %d] Инициирован переход в Zigbee 3.0 (чистый рестарт)...", m_zbProfile + 1);
    saveLog(logBuf);

    Preferences prefs;
    prefs.begin("radio", false);
    prefs.putUChar("mode", RADIO_MODE_ZIGBEE);
    prefs.putBool("force_pair", true);
    prefs.end();

    if (eff && m_matrix) {
        eff->triggerRadioModeAnimation(true); // Purple-gold rings converging to equator (2s)
        uint32_t t0 = millis();
        while (millis() - t0 < 2000) {
            eff->renderRadioModeAnimation(*m_matrix, millis() - t0);
            m_matrix->show();
            delay(16);
        }
    } else {
        delay(500);
    }

    ESP.restart();
}

void ZigbeeManager::switchToWifi(const char* reason, EffectManager* eff) {
    Serial.printf("[ZigbeeManager] Switching to Wi-Fi mode (%s): saving NVS and restarting...\n", reason);
    String logMsg = String("Возврат в Wi-Fi: ") + reason;
    saveLog(logMsg.c_str());

    Preferences prefs;
    prefs.begin("radio", false);
    prefs.putUChar("mode", RADIO_MODE_WIFI);
    prefs.end();

    if (eff && m_matrix) {
        eff->triggerRadioModeAnimation(false); // Rotating green beacon on top rings (2s)
        uint32_t t0 = millis();
        while (millis() - t0 < 2000) {
            eff->renderRadioModeAnimation(*m_matrix, millis() - t0);
            m_matrix->show();
            delay(16);
        }
    } else {
        delay(500);
    }

    ESP.restart();
}

uint8_t ZigbeeManager::toggleMode(EffectManager* eff) {
    if (m_mode == RADIO_MODE_ZIGBEE) {
        switchToWifi("Ручное переключение по 4 кликам", eff);
        return RADIO_MODE_WIFI;
    } else {
        switchToZigbee(eff);
        return RADIO_MODE_ZIGBEE;
    }
}

void ZigbeeManager::noteHttpActivity() {
    m_lastHttpActivityMs = millis();
    m_wifiStayActiveUntilMs = millis() + HTTP_WIFI_EXTENSION_MS;
}

void ZigbeeManager::requestWifiSession(uint32_t durationMs, WiFiManager* wifi, EffectManager* eff, bool playAnim) {
    m_wifiStayActiveUntilMs = millis() + durationMs;
    m_lastHttpActivityMs = millis();
}

void ZigbeeManager::update(WiFiManager* wifi, EffectManager* eff, LedMatrix* matrix, OtaGithub* ota) {
    uint32_t now = millis();

    if (m_mode == RADIO_MODE_ZIGBEE) {
        // 1. Check Zigbee connection status
        bool connected = Zigbee.connected();
        if (connected && !m_connectedSuccessDone) {
            m_connectedSuccessDone = true;
            m_pairingActive = false;
            Serial.println(F("[ZigbeeManager] SUCCESS: Connected to Zigbee Coordinator (Alice / Yandex Station)!"));

            // Clear force_pairing flag in NVS
            Preferences rPrefs;
            rPrefs.begin("radio", false);
            rPrefs.putBool("force_pair", false);
            rPrefs.end();

            char logBuf[128];
            snprintf(logBuf, sizeof(logBuf), "[Профиль %d] Подключено к координатору Zigbee (Яндекс Станция)! Сеть активна.", m_zbProfile + 1);
            saveLog(logBuf);

            if (eff) {
                eff->setZigbeePairingMode(false);
                eff->triggerSuccessWave(); // 1.5s emerald-green wave across whole cylinder
            }
        }

        // 2. Check 180s fail-safe timeout
        if (m_pairingActive && !connected) {
            if (now - m_pairingStartMs >= ZIGBEE_PAIRING_TIMEOUT_MS) {
                Serial.println(F("[ZigbeeManager] 180s pairing window elapsed without coordinator. Fail-safe: switching to Wi-Fi!"));
                switchToWifi("Таймаут сопряжения 180 с без координатора", eff);
                return;
            }
        }
    }
}

void ZigbeeManager::handleZigbeeOnOff(bool on, EffectManager& effectMgr) {
    uint8_t profId = s_zbManager ? (s_zbManager->getProfile() + 1) : 1;
    char logBuf[128];
    snprintf(logBuf, sizeof(logBuf), "[Профиль %d] %s", profId, on ? "Вкл" : "Выкл");
    saveLog(logBuf);
    Serial.printf("[ZigbeeCommand] Power: %s\n", on ? "ON" : "OFF");
    if (on) {
        effectMgr.powerOn();
    } else {
        effectMgr.powerOff();
    }
}

void ZigbeeManager::handleZigbeeBrightness(uint8_t brightness, EffectManager& effectMgr) {
    uint8_t profId = s_zbManager ? (s_zbManager->getProfile() + 1) : 1;
    char logBuf[128];
    snprintf(logBuf, sizeof(logBuf), "[Профиль %d] Яркость: %d", profId, brightness);
    saveLog(logBuf);
    Serial.printf("[ZigbeeCommand] Brightness: %d\n", brightness);
    effectMgr.setBrightness(brightness);
}

void ZigbeeManager::handleZigbeeColor(uint8_t hue, uint8_t saturation, EffectManager& effectMgr, LedMatrix& matrix) {
    uint8_t profId = s_zbManager ? (s_zbManager->getProfile() + 1) : 1;
    char logBuf[128];
    snprintf(logBuf, sizeof(logBuf), "[Профиль %d] Цвет: Hue %d, Sat %d", profId, hue, saturation);
    saveLog(logBuf);
    Serial.printf("[ZigbeeCommand] Color Hue: %d, Sat: %d\n", hue, saturation);

    // Low saturation: user requested warm white / nightlight
    if (saturation < 35) {
        effectMgr.setEffect(0, matrix); // Nightlight
        return;
    }

    // Map Alice color spectrum (0..255) to 15 built-in effects
    uint8_t targetEffect = 0;
    if (hue < 16) {
        targetEffect = 2;   // Red -> Fire
    } else if (hue < 32) {
        targetEffect = 1;   // Orange -> DawnDusk
    } else if (hue < 50) {
        targetEffect = 8;   // Amber/Gold -> Fireflies
    } else if (hue < 70) {
        targetEffect = 5;   // Yellow -> LorenzAttractor
    } else if (hue < 100) {
        targetEffect = 4;   // Lime/Green -> GameOfLife
    } else if (hue < 130) {
        targetEffect = 10;  // Emerald -> MatrixRain
    } else if (hue < 155) {
        targetEffect = 9;   // Cyan/Teal -> Aurora
    } else if (hue < 175) {
        targetEffect = 6;   // Sky Blue -> Metaballs
    } else if (hue < 200) {
        targetEffect = 11;  // Blue -> LavaLamp
    } else if (hue < 220) {
        targetEffect = 14;  // Violet -> CyberPulse
    } else if (hue < 235) {
        targetEffect = 12;  // Purple/Magenta -> Vortex
    } else if (hue < 248) {
        targetEffect = 13;  // Pink/Crimson -> Fireworks
    } else {
        targetEffect = 3;   // Red-Pink -> PerlinNoise
    }

    effectMgr.setEffect(targetEffect, matrix);
}

void ZigbeeManager::handleZigbeeColorTemp(uint16_t mireds, EffectManager& effectMgr, LedMatrix& matrix) {
    uint8_t profId = s_zbManager ? (s_zbManager->getProfile() + 1) : 1;
    char logBuf[128];
    snprintf(logBuf, sizeof(logBuf), "[Профиль %d] ЦТ: %u mireds", profId, mireds);
    saveLog(logBuf);
    Serial.printf("[ZigbeeCommand] Color Temp: %u mireds\n", mireds);
    uint8_t temp = 90;
    if (mireds >= 555) {
        temp = 0;
    } else if (mireds <= 250) {
        temp = 255;
    } else {
        temp = static_cast<uint8_t>(map(mireds, 555, 250, 0, 255));
    }

    EffectProfile prof = effectMgr.getProfile(0);
    prof.param1 = temp;
    effectMgr.setProfile(0, prof);
    effectMgr.setEffect(0, matrix);
}
