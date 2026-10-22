#include "zigbee_manager.h"
#include "wifi_manager.h"
#include "effects/effect_manager.h"
#include "ota_github.h"
#include <time.h>

static ZigbeeColorDimmableLight zbLight(ZIGBEE_LIGHT_ENDPOINT);
static EffectManager* s_effMgr = nullptr;
static LedMatrix* s_ledMatrix = nullptr;

static void onZigbeeLightChangeRgb(bool state, uint8_t red, uint8_t green, uint8_t blue, uint8_t level) {
    if (!s_effMgr) return;
    if (!state) {
        s_effMgr->powerOff();
        return;
    }
    s_effMgr->powerOn();
    s_effMgr->setBrightness(level);

    CRGB col = CRGB(red, green, blue);
    CHSV hsv = rgb2hsv_approximate(col);
    if (s_ledMatrix) {
        ZigbeeManager::handleZigbeeColor(hsv.hue, hsv.sat, *s_effMgr, *s_ledMatrix);
    }
}

static void onZigbeeLightChangeHsv(bool state, uint8_t hue, uint8_t saturation, uint8_t value) {
    if (!s_effMgr) return;
    if (!state) {
        s_effMgr->powerOff();
        return;
    }
    s_effMgr->powerOn();
    s_effMgr->setBrightness(value);

    if (s_ledMatrix) {
        ZigbeeManager::handleZigbeeColor(hue, saturation, *s_effMgr, *s_ledMatrix);
    }
}

static void onZigbeeLightChangeTemp(bool state, uint8_t level, uint16_t color_temperature) {
    if (!s_effMgr) return;
    if (!state) {
        s_effMgr->powerOff();
        return;
    }
    s_effMgr->powerOn();
    s_effMgr->setBrightness(level);

    if (s_ledMatrix) {
        ZigbeeManager::handleZigbeeColorTemp(color_temperature, *s_effMgr, *s_ledMatrix);
    }
}

ZigbeeManager::ZigbeeManager()
    : m_mode(RADIO_MODE_WIFI),
      m_activeRadio(RADIO_MODE_WIFI),
      m_matrix(nullptr),
      m_lastHttpActivityMs(0),
      m_wifiStayActiveUntilMs(0),
      m_nightlyCheckDone(false),
      m_pairingActive(false),
      m_pairingStartMs(0),
      m_connectedSuccessDone(false) {}

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

    Preferences prefs;
    prefs.begin("radio", true);
    m_mode = prefs.getUChar("mode", RADIO_MODE_WIFI);
    prefs.end();

    if (m_mode > 1) {
        m_mode = RADIO_MODE_WIFI;
    }

    // Configure Zigbee endpoint attributes and callbacks
    zbLight.setManufacturerAndModel("MagicLamp", "MagicLamp-C6");
    uint16_t capabilities = ZIGBEE_COLOR_CAPABILITY_HUE_SATURATION | ZIGBEE_COLOR_CAPABILITY_X_Y | ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP;
    zbLight.setLightColorCapabilities(capabilities);
    zbLight.onLightChangeRgb(onZigbeeLightChangeRgb);
    zbLight.onLightChangeHsv(onZigbeeLightChangeHsv);
    zbLight.onLightChangeTemp(onZigbeeLightChangeTemp);

    // Register Endpoint 10 in Zigbee Core
    Zigbee.addEndpoint(&zbLight);

    if (m_mode == RADIO_MODE_ZIGBEE) {
        m_activeRadio = RADIO_MODE_ZIGBEE;
        Serial.println(F("[ZigbeeManager] Starting hardware Zigbee 3.0 stack (IEEE 802.15.4)..."));

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

        saveLog("Zigbee 3.0 End Device активен (MagicLamp-C6, EP 10). Поиск сети (до 180 с)...");

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
    saveLog("Инициирован переход в Zigbee 3.0 (чистый рестарт)...");

    Preferences prefs;
    prefs.begin("radio", false);
    prefs.putUChar("mode", RADIO_MODE_ZIGBEE);
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
            saveLog("Подключено к координатору Zigbee (Яндекс Станция)! Сеть активна.");

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
    Serial.printf("[ZigbeeCommand] Power: %s\n", on ? "ON" : "OFF");
    if (on) {
        effectMgr.powerOn();
    } else {
        effectMgr.powerOff();
    }
}

void ZigbeeManager::handleZigbeeBrightness(uint8_t brightness, EffectManager& effectMgr) {
    Serial.printf("[ZigbeeCommand] Brightness: %d\n", brightness);
    effectMgr.setBrightness(brightness);
}

void ZigbeeManager::handleZigbeeColor(uint8_t hue, uint8_t saturation, EffectManager& effectMgr, LedMatrix& matrix) {
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
