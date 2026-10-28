#include "zigbee_manager.h"
#include "wifi_manager.h"
#include "effects/effect_manager.h"
#include "ota_github.h"
#include "esp_zigbee_cluster.h"
#include "esp_mac.h"
#include "esp_log.h"
#include <LittleFS.h>
#include <time.h>

class MagicColorLight : public ZigbeeColorDimmableLight {
public:
    MagicColorLight(uint8_t ep = ZIGBEE_LIGHT_ENDPOINT) : ZigbeeColorDimmableLight(ep) {}
    void setupBasicAttributes() {
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
};

class MagicBasicLight : public ZigbeeLight {
public:
    MagicBasicLight(uint8_t ep = ZIGBEE_LIGHT_ENDPOINT) : ZigbeeLight(ep) {}
    void setupBasicAttributes() {
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
};

class MagicDimmableLight : public ZigbeeDimmableLight {
public:
    MagicDimmableLight(uint8_t ep = ZIGBEE_LIGHT_ENDPOINT) : ZigbeeDimmableLight(ep) {}
    void setupBasicAttributes() {
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
};

static MagicColorLight* s_zbColorLight = nullptr;
static MagicBasicLight* s_zbBasicLight = nullptr;
static MagicDimmableLight* s_zbDimmerLight = nullptr;

static EffectManager* s_effMgr = nullptr;
static LedMatrix* s_ledMatrix = nullptr;
static ZigbeeManager* s_zbManager = nullptr;
static String s_logBuffer = "";

static vprintf_like_t s_origVprintf = nullptr;
static bool s_inLogVprintf = false;

static int zigbeeLogVprintf(const char *fmt, va_list args) {
    if (s_inLogVprintf) {
        return (s_origVprintf) ? s_origVprintf(fmt, args) : vprintf(fmt, args);
    }
    s_inLogVprintf = true;
    va_list args_copy;
    va_copy(args_copy, args);
    int ret = 0;
    if (s_origVprintf) {
        ret = s_origVprintf(fmt, args);
    } else {
        ret = vprintf(fmt, args);
    }

    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args_copy);
    va_end(args_copy);

    if (strstr(buf, "ZB") || strstr(buf, "zb") || strstr(buf, "Zigbee") || strstr(buf, "IEEE") || strstr(buf, "zboss")) {
        size_t len = strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
            buf[len - 1] = '\0';
            len--;
        }
        if (len > 0) {
            ZigbeeManager::appendLog(buf, false);
        }
    }
    s_inLogVprintf = false;
    return ret;
}

static void onZigbeeIdentify(uint16_t time) {
    char buf[128];
    snprintf(buf, sizeof(buf), "[Identify] Команда Identify (%u с)", time);
    ZigbeeManager::appendLog(buf);
    if (s_effMgr) {
        s_effMgr->triggerSuccessWave();
    }
}

static void onZigbeeLightChangeBasic(bool state) {
    if (!s_effMgr) return;
    char logBuf[128];
    snprintf(logBuf, sizeof(logBuf), "[Профиль 3] %s", state ? "Вкл" : "Выкл");
    ZigbeeManager::appendLog(logBuf);
    if (!state) {
        s_effMgr->powerOff();
    } else {
        s_effMgr->powerOn();
    }
}

static void onZigbeeLightChangeDimmable(bool state, uint8_t level) {
    if (!s_effMgr) return;
    char logBuf[128];
    snprintf(logBuf, sizeof(logBuf), "[Профиль 4] %s, ярк: %d", state ? "Вкл" : "Выкл", level);
    ZigbeeManager::appendLog(logBuf);
    if (!state) {
        s_effMgr->powerOff();
    } else {
        s_effMgr->powerOn();
        s_effMgr->setBrightness(level);
    }
}

static void onZigbeeLightChangeRgb(bool state, uint8_t red, uint8_t green, uint8_t blue, uint8_t level) {
    if (!s_effMgr) return;
    uint8_t profId = s_zbManager ? (s_zbManager->getProfile() + 1) : 1;
    char logBuf[128];
    if (!state) {
        snprintf(logBuf, sizeof(logBuf), "[Профиль %d] Выкл", profId);
        ZigbeeManager::appendLog(logBuf);
        s_effMgr->powerOff();
        return;
    }
    s_effMgr->powerOn();
    s_effMgr->setBrightness(level);

    CRGB col = CRGB(red, green, blue);
    CHSV hsv = rgb2hsv_approximate(col);
    snprintf(logBuf, sizeof(logBuf), "[Профиль %d] Вкл, ярк: %d, цвет: Hue %d, Sat %d", profId, level, hsv.hue, hsv.sat);
    ZigbeeManager::appendLog(logBuf);

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
        ZigbeeManager::appendLog(logBuf);
        s_effMgr->powerOff();
        return;
    }
    s_effMgr->powerOn();
    s_effMgr->setBrightness(value);

    snprintf(logBuf, sizeof(logBuf), "[Профиль %d] Вкл, ярк: %d, цвет: Hue %d", profId, value, hue);
    ZigbeeManager::appendLog(logBuf);

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
        ZigbeeManager::appendLog(logBuf);
        s_effMgr->powerOff();
        return;
    }
    s_effMgr->powerOn();
    s_effMgr->setBrightness(level);

    uint8_t p = (color_temperature <= 153) ? 255 : (color_temperature >= 500 ? 0 : (uint8_t)map(color_temperature, 153, 500, 255, 0));
    snprintf(logBuf, sizeof(logBuf), "[Профиль %d] Вкл, ярк: %d, ЦТ: %u mireds (param1=%u)", profId, level, color_temperature, p);
    ZigbeeManager::appendLog(logBuf);

    if (s_ledMatrix) {
        EffectProfile prof = s_effMgr->getProfile(0);
        prof.param1 = p;
        s_effMgr->setProfile(0, prof);
        s_effMgr->setEffect(0, *s_ledMatrix);
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
    prefs.putBool("force_pair", true);
    uint8_t seq = prefs.getUChar("pair_seq", 0);
    seq++;
    prefs.putUChar("pair_seq", seq);
    prefs.end();
    Serial.printf("[ZigbeeManager] Profile set to %d, force_pair=true, pair_seq=%u\n", prof, seq);
}

void ZigbeeManager::appendLog(const char* msg, bool printSerial) {
    if (!msg) return;

    uint32_t ms = millis();
    uint32_t totalSec = ms / 1000;
    uint32_t mm = totalSec / 60;
    uint32_t ss = totalSec % 60;
    uint32_t mmm = ms % 1000;

    char line[320];
    snprintf(line, sizeof(line), "[%02u:%02u.%03u] %s\n", (unsigned int)mm, (unsigned int)ss, (unsigned int)mmm, msg);

    if (printSerial) {
        Serial.print(line);
    }

    // 1. Append to LittleFS file /zigbee.log
    File f = LittleFS.open("/zigbee.log", FILE_APPEND);
    if (f) {
        f.print(line);
        f.close();
    }

    // 2. Append to RAM buffer (up to 3.5 KB)
    s_logBuffer += line;
    if (s_logBuffer.length() > 3500) {
        int cutIdx = s_logBuffer.length() - 3000;
        int nextNl = s_logBuffer.indexOf('\n', cutIdx);
        if (nextNl >= 0) {
            s_logBuffer = s_logBuffer.substring(nextNl + 1);
        } else {
            s_logBuffer = s_logBuffer.substring(cutIdx);
        }
    }

    // 3. Save to NVS
    Preferences prefs;
    if (prefs.begin("sys_log", false)) {
        prefs.putString("zb_log", s_logBuffer);
        prefs.end();
    }
}

void ZigbeeManager::clearLog() {
    s_logBuffer = "";
    if (LittleFS.exists("/zigbee.log")) {
        LittleFS.remove("/zigbee.log");
    }
    Preferences prefs;
    if (prefs.begin("sys_log", false)) {
        prefs.remove("zb_log");
        prefs.remove("last_zb_log");
        prefs.end();
    }
    Serial.println(F("[ZigbeeManager] Session log cleared."));
}

String ZigbeeManager::getLog() {
    if (s_logBuffer.isEmpty()) {
        Preferences prefs;
        if (prefs.begin("sys_log", true)) {
            s_logBuffer = prefs.getString("zb_log", "");
            prefs.end();
        }
    }
    if (!s_logBuffer.isEmpty()) {
        return s_logBuffer;
    }
    if (LittleFS.exists("/zigbee.log")) {
        File f = LittleFS.open("/zigbee.log", FILE_READ);
        if (f) {
            size_t sz = f.size();
            if (sz > 3500) {
                f.seek(sz - 3500);
            }
            s_logBuffer = f.readString();
            f.close();
            return s_logBuffer;
        }
    }
    return "Нет записей о сессиях Zigbee";
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
    uint8_t pairSeq = prefs.getUChar("pair_seq", 0);
    prefs.end();

    if (m_mode > 1) {
        m_mode = RADIO_MODE_WIFI;
    }
    if (m_zbProfile > 3) {
        m_zbProfile = 0;
    }

    if (m_mode == RADIO_MODE_ZIGBEE) {
        m_activeRadio = RADIO_MODE_ZIGBEE;

        // 1. Shift IEEE 802.15.4 MAC address by profile & pair_seq to invalidate stale coordinator cache
        uint8_t mac8[8] = {0};
        esp_read_mac(mac8, ESP_MAC_IEEE802154);
        mac8[7] ^= ((m_zbProfile + 1) * 31 + pairSeq) & 0xFF;
        esp_iface_mac_addr_set(mac8, ESP_MAC_IEEE802154);
        Serial.printf("[ZigbeeManager] EUI-64 MAC: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X (pair_seq=%u)\n",
                      mac8[0], mac8[1], mac8[2], mac8[3], mac8[4], mac8[5], mac8[6], mac8[7], pairSeq);

        // 2. Intercept ESP-IDF system logs for Zigbee events
        if (!s_origVprintf) {
            s_origVprintf = esp_log_set_vprintf(zigbeeLogVprintf);
        }

        // 3. Register corresponding endpoint according to active profile
        const char* modelName = "";
        switch (m_zbProfile) {
            case 0: { // VlasovLabTech RGB+CCT 0x0102
                s_zbColorLight = new MagicColorLight(ZIGBEE_LIGHT_ENDPOINT);
                s_zbColorLight->setManufacturerAndModel("VlasovLabTech", "ZD-C6-2D1ARGB-v2");
                s_zbColorLight->setLightColorCapabilities(ZIGBEE_COLOR_CAPABILITY_X_Y | ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP);
                s_zbColorLight->setLightColorTemperatureRange(153, 500);
                s_zbColorLight->setupBasicAttributes();
                s_zbColorLight->onIdentify(onZigbeeIdentify);
                s_zbColorLight->onLightChangeRgb(onZigbeeLightChangeRgb);
                s_zbColorLight->onLightChangeHsv(onZigbeeLightChangeHsv);
                s_zbColorLight->onLightChangeTemp(onZigbeeLightChangeTemp);
                Zigbee.addEndpoint(s_zbColorLight);
                modelName = "ZD-C6-2D1ARGB-v2 (RGB+CCT 0x0102)";
                break;
            }
            case 1: { // MagicLamp RGB+CCT 0x0102
                s_zbColorLight = new MagicColorLight(ZIGBEE_LIGHT_ENDPOINT);
                s_zbColorLight->setManufacturerAndModel("MagicLamp", "MagicLamp-C6");
                s_zbColorLight->setLightColorCapabilities(ZIGBEE_COLOR_CAPABILITY_X_Y | ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP);
                s_zbColorLight->setLightColorTemperatureRange(153, 500);
                s_zbColorLight->setupBasicAttributes();
                s_zbColorLight->onIdentify(onZigbeeIdentify);
                s_zbColorLight->onLightChangeRgb(onZigbeeLightChangeRgb);
                s_zbColorLight->onLightChangeHsv(onZigbeeLightChangeHsv);
                s_zbColorLight->onLightChangeTemp(onZigbeeLightChangeTemp);
                Zigbee.addEndpoint(s_zbColorLight);
                modelName = "MagicLamp-C6 (RGB+CCT 0x0102)";
                break;
            }
            case 2: { // Базовый Свет Вкл/Выкл 0x0100 — VlasovLabTech
                s_zbBasicLight = new MagicBasicLight(ZIGBEE_LIGHT_ENDPOINT);
                s_zbBasicLight->setManufacturerAndModel("VlasovLabTech", "NanoC6-OnOff-Light");
                s_zbBasicLight->setupBasicAttributes();
                s_zbBasicLight->onIdentify(onZigbeeIdentify);
                s_zbBasicLight->onLightChange(onZigbeeLightChangeBasic);
                Zigbee.addEndpoint(s_zbBasicLight);
                modelName = "NanoC6-OnOff-Light (OnOff 0x0100)";
                break;
            }
            case 3: { // Диммируемый Свет 0x0101 — MagicLamp
                s_zbDimmerLight = new MagicDimmableLight(ZIGBEE_LIGHT_ENDPOINT);
                s_zbDimmerLight->setManufacturerAndModel("MagicLamp", "MagicLamp-Dimmer");
                s_zbDimmerLight->setupBasicAttributes();
                s_zbDimmerLight->onIdentify(onZigbeeIdentify);
                s_zbDimmerLight->onLightChange(onZigbeeLightChangeDimmable);
                Zigbee.addEndpoint(s_zbDimmerLight);
                modelName = "MagicLamp-Dimmer (Dimmer 0x0101)";
                break;
            }
        }

        Serial.printf("[ZigbeeManager] Starting hardware Zigbee 3.0 stack (Profile %d: %s, EP %d)...\n",
                      m_zbProfile + 1, modelName, ZIGBEE_LIGHT_ENDPOINT);

        if (forcePairing) {
            Serial.println(F("[ZigbeeManager] force_pairing is true: erasing NVRAM via esp_zb_nvram_erase_at_start(true)..."));
            esp_zb_nvram_erase_at_start(true);
        }

        if (!Zigbee.begin(ZIGBEE_END_DEVICE)) {
            Serial.println(F("[ZigbeeManager] ERROR: Zigbee.begin() failed! Returning to Wi-Fi..."));
            appendLog("Ошибка инициализации Zigbee.begin(ZIGBEE_END_DEVICE)");
            switchToWifi("Ошибка старта Zigbee стека", eff);
            return;
        }

        // Open network search (Network Steering) for 180 seconds
        Zigbee.setRebootOpenNetwork(180);
        m_pairingActive = true;
        m_pairingStartMs = millis();
        m_connectedSuccessDone = false;

        char logBuf[128];
        snprintf(logBuf, sizeof(logBuf), "[Профиль %d] Zigbee 3.0 End Device активен (%s). Поиск сети (до 180 с)...",
                 m_zbProfile + 1, modelName);
        appendLog(logBuf);

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
    appendLog(logBuf);

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
    appendLog(logMsg.c_str());

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
            appendLog(logBuf);

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
    appendLog(logBuf);
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
    appendLog(logBuf);
    Serial.printf("[ZigbeeCommand] Brightness: %d\n", brightness);
    effectMgr.setBrightness(brightness);
}

void ZigbeeManager::handleZigbeeColor(uint8_t hue, uint8_t saturation, EffectManager& effectMgr, LedMatrix& matrix) {
    uint8_t profId = s_zbManager ? (s_zbManager->getProfile() + 1) : 1;
    char logBuf[128];
    snprintf(logBuf, sizeof(logBuf), "[Профиль %d] Цвет: Hue %d, Sat %d", profId, hue, saturation);
    appendLog(logBuf);
    Serial.printf("[ZigbeeCommand] Color Hue: %d, Sat: %d\n", hue, saturation);

    // Low saturation: user requested warm white / nightlight
    if (saturation < 35) {
        effectMgr.setEffect(0, matrix); // Nightlight
        return;
    }

    // Map Alice color spectrum (0..255) to 15 built-in effects (0..14)
    uint8_t targetEffect = 0;
    if (hue < 17) {
        targetEffect = 2;   // Red -> Fire
    } else if (hue < 35) {
        targetEffect = 1;   // Orange -> DawnDusk
    } else if (hue < 53) {
        targetEffect = 8;   // Amber/Gold -> Fireflies
    } else if (hue < 71) {
        targetEffect = 5;   // Yellow -> LorenzAttractor
    } else if (hue < 90) {
        targetEffect = 7;   // Lime-Yellow -> AudioReactive
    } else if (hue < 108) {
        targetEffect = 4;   // Green -> GameOfLife
    } else if (hue < 126) {
        targetEffect = 10;  // Emerald -> MatrixRain
    } else if (hue < 144) {
        targetEffect = 9;   // Cyan/Teal -> Aurora
    } else if (hue < 162) {
        targetEffect = 6;   // Sky Blue -> Metaballs
    } else if (hue < 180) {
        targetEffect = 11;  // Blue -> LavaLamp
    } else if (hue < 198) {
        targetEffect = 14;  // Violet -> CyberPulse
    } else if (hue < 216) {
        targetEffect = 12;  // Purple/Magenta -> Vortex
    } else if (hue < 234) {
        targetEffect = 13;  // Pink/Crimson -> Fireworks
    } else {
        targetEffect = 3;   // Red-Pink -> PerlinNoise
    }

    effectMgr.setEffect(targetEffect, matrix);
}

void ZigbeeManager::handleZigbeeColorTemp(uint16_t mireds, EffectManager& effectMgr, LedMatrix& matrix) {
    uint8_t profId = s_zbManager ? (s_zbManager->getProfile() + 1) : 1;
    uint8_t p = (mireds <= 153) ? 255 : (mireds >= 500 ? 0 : (uint8_t)map(mireds, 153, 500, 255, 0));
    char logBuf[128];
    snprintf(logBuf, sizeof(logBuf), "[Профиль %d] ЦТ: %u mireds (param1=%u)", profId, mireds, p);
    appendLog(logBuf);
    Serial.printf("[ZigbeeCommand] Color Temp: %u mireds -> param1=%u\n", mireds, p);

    EffectProfile prof = effectMgr.getProfile(0);
    prof.param1 = p;
    effectMgr.setProfile(0, prof);
    effectMgr.setEffect(0, matrix);
}
