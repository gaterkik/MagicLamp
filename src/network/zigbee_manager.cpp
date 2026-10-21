#include "zigbee_manager.h"
#include "wifi_manager.h"
#include "effects/effect_manager.h"
#include "ota_github.h"
#include <time.h>

ZigbeeManager::ZigbeeManager()
    : m_mode(RADIO_MODE_WIFI),
      m_activeRadio(RADIO_MODE_WIFI),
      m_matrix(nullptr),
      m_lastHttpActivityMs(0),
      m_wifiStayActiveUntilMs(0),
      m_nightlyCheckDone(false) {}

void ZigbeeManager::begin(LedMatrix* matrix) {
    m_matrix = matrix;

    Preferences prefs;
    prefs.begin("radio", true);
    m_mode = prefs.getUChar("mode", RADIO_MODE_WIFI);
    prefs.end();

    if (m_mode > 1) {
        m_mode = RADIO_MODE_WIFI;
    }

    // Always start with Wi-Fi active for initial 60 seconds (NTP GMT+2 sync, OTA check)
    m_activeRadio = RADIO_MODE_WIFI;
    m_lastHttpActivityMs = millis();
    m_wifiStayActiveUntilMs = millis() + BOOT_WIFI_WINDOW_MS;
    m_nightlyCheckDone = false;

    Serial.printf("[ZigbeeManager] Initialized. Configured Mode: %s (%d), Active Radio: Wi-Fi (60s boot window)\n",
                  getModeString(), m_mode);
}

void ZigbeeManager::noteHttpActivity() {
    m_lastHttpActivityMs = millis();
    // Extend Wi-Fi session by 10 minutes from now
    m_wifiStayActiveUntilMs = millis() + HTTP_WIFI_EXTENSION_MS;
}

void ZigbeeManager::requestWifiSession(uint32_t durationMs, WiFiManager* wifi, EffectManager* eff, bool playAnim) {
    m_wifiStayActiveUntilMs = millis() + durationMs;
    m_lastHttpActivityMs = millis();

    if (m_activeRadio != RADIO_MODE_WIFI) {
        Serial.printf("[ZigbeeManager] Transitioning radio from Zigbee to Wi-Fi for %u ms session...\n",
                      (unsigned int)durationMs);
        if (playAnim && eff && eff->isPoweredOn()) {
            playSwitchAnimation(RADIO_MODE_WIFI);
        }
        if (wifi) {
            wifi->begin();
        }
        m_activeRadio = RADIO_MODE_WIFI;
    }
}

bool ZigbeeManager::setMode(uint8_t mode, bool playAnim, EffectManager* eff, WiFiManager* wifi) {
    if (mode > 1) mode = RADIO_MODE_WIFI;

    bool changed = (m_mode != mode);
    m_mode = mode;

    Preferences prefs;
    prefs.begin("radio", false);
    prefs.putUChar("mode", m_mode);
    prefs.end();

    Serial.printf("[ZigbeeManager] Radio configured mode set to %s (%d)\n",
                  getModeString(), m_mode);

    if (m_mode == RADIO_MODE_WIFI) {
        requestWifiSession(HTTP_WIFI_EXTENSION_MS, wifi, eff, playAnim);
    } else {
        // Immediate switch to Zigbee if user explicitly selected Zigbee
        if (m_activeRadio == RADIO_MODE_WIFI) {
            if (playAnim && eff && eff->isPoweredOn()) {
                playSwitchAnimation(RADIO_MODE_ZIGBEE);
            }
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            m_activeRadio = RADIO_MODE_ZIGBEE;
            Serial.println(F("[ZigbeeManager] Switched active radio to Zigbee (802.15.4)."));
        }
    }

    return changed;
}

uint8_t ZigbeeManager::toggleMode(EffectManager* eff, WiFiManager* wifi) {
    uint8_t nextMode = (m_mode == RADIO_MODE_WIFI) ? RADIO_MODE_ZIGBEE : RADIO_MODE_WIFI;
    setMode(nextMode, true, eff, wifi);
    return m_mode;
}

void ZigbeeManager::update(WiFiManager* wifi, EffectManager* eff, LedMatrix* matrix, OtaGithub* ota) {
    uint32_t now = millis();

    // 1. Automatic transition to Zigbee if configured mode is Zigbee
    if (m_mode == RADIO_MODE_ZIGBEE && m_activeRadio == RADIO_MODE_WIFI) {
        if (now >= m_wifiStayActiveUntilMs) {
            Serial.println(F("[ZigbeeManager] 10 min inactivity / boot window ended: auto-switching to Zigbee mode."));
            if (eff && eff->isPoweredOn()) {
                playSwitchAnimation(RADIO_MODE_ZIGBEE);
            }
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            m_activeRadio = RADIO_MODE_ZIGBEE;
        }
    }

    // 2. Daily 04:00 GMT+2 window: wake Wi-Fi for 45s for OTA check in Stealth Mode
    if (m_mode == RADIO_MODE_ZIGBEE) {
        time_t rawtime;
        time(&rawtime);
        if (rawtime > 100000) {
            time_t local = rawtime + TIMEZONE_OFFSET_SEC;
            struct tm* ti = gmtime(&local);
            if (ti) {
                if (ti->tm_hour == 4 && ti->tm_min == 0) {
                    if (!m_nightlyCheckDone) {
                        m_nightlyCheckDone = true;
                        Serial.println(F("[ZigbeeManager] 04:00 GMT+2 window: enabling Wi-Fi for 45s (Stealth Mode OTA check)..."));
                        requestWifiSession(NIGHTLY_CHECK_WINDOW_MS, wifi, eff, false /* stealth - no animation */);
                        if (ota) {
                            ota->triggerCheck();
                        }
                    }
                } else if (ti->tm_hour != 4 || ti->tm_min != 0) {
                    m_nightlyCheckDone = false;
                }
            }
        }
    }
}

void ZigbeeManager::playSwitchAnimation(uint8_t mode) {
    if (!m_matrix) return;

    Serial.printf("[ZigbeeManager] Playing 3 waves for %s...\n",
                  (mode == RADIO_MODE_WIFI) ? "Wi-Fi (Cyan/Teal)" : "Zigbee (Purple/Magenta)");

    uint8_t prevBrightness = m_matrix->getBrightness();
    if (prevBrightness < 64) {
        m_matrix->setBrightness(128);
    }

    // 3 Teal/Cyan waves for Wi-Fi, 3 Purple/Magenta waves for Zigbee
    CRGB waveColor = (mode == RADIO_MODE_WIFI) 
                     ? CRGB(0, 220, 210)    // Teal / Cyan
                     : CRGB(200, 20, 220);  // Purple / Magenta

    const int numWaves = 3;
    const uint32_t waveDurationMs = 380;

    for (int w = 0; w < numWaves; w++) {
        uint32_t start = millis();
        while (millis() - start < waveDurationMs) {
            float phase = static_cast<float>(millis() - start) / static_cast<float>(waveDurationMs);
            float waveCenterY = phase * (MATRIX_HEIGHT + 4) - 2.0f;

            for (int y = 0; y < MATRIX_HEIGHT; y++) {
                float dist = fabsf(static_cast<float>(y) - waveCenterY);
                float factor = fmaxf(0.0f, 1.0f - dist * 0.32f);
                factor = factor * factor;

                CRGB rowCol = waveColor;
                rowCol.nscale8_video(static_cast<uint8_t>(factor * 255.0f));

                for (int x = 0; x < MATRIX_WIDTH; x++) {
                    m_matrix->setPixel(x, y, rowCol);
                }
            }
            m_matrix->show();
            delay(15);
        }
    }

    m_matrix->clear(true);
    m_matrix->setBrightness(prevBrightness);
}

// =============================================================================
// Zigbee / Alice Voice Commands Mapping
// =============================================================================

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

    // If saturation is low (< 35): user asked for white / warm white
    if (saturation < 35) {
        effectMgr.setEffect(0, matrix); // Nightlight
        return;
    }

    // Map 360-degree Alice color spectrum (0..255) to 16 built-in effects
    uint8_t targetEffect = 0;
    if (hue < 16) {
        targetEffect = 2;   // Red -> Fire
    } else if (hue < 32) {
        targetEffect = 1;   // Orange -> DawnDusk
    } else if (hue < 50) {
        targetEffect = 7;   // Amber/Gold -> SandHourglass
    } else if (hue < 70) {
        targetEffect = 9;   // Yellow -> Fireflies
    } else if (hue < 100) {
        targetEffect = 4;   // Lime/Green -> GameOfLife
    } else if (hue < 130) {
        targetEffect = 11;  // Emerald -> MatrixRain
    } else if (hue < 155) {
        targetEffect = 10;  // Cyan/Teal -> Aurora
    } else if (hue < 175) {
        targetEffect = 6;   // Sky Blue -> Metaballs
    } else if (hue < 200) {
        targetEffect = 12;  // Blue -> LavaLamp
    } else if (hue < 220) {
        targetEffect = 15;  // Violet -> CyberPulse
    } else if (hue < 235) {
        targetEffect = 13;  // Purple/Magenta -> Vortex
    } else if (hue < 248) {
        targetEffect = 14;  // Pink/Crimson -> Fireworks
    } else {
        targetEffect = 3;   // Red-Pink -> PerlinNoise
    }

    effectMgr.setEffect(targetEffect, matrix);
}

void ZigbeeManager::handleZigbeeColorTemp(uint16_t mireds, EffectManager& effectMgr, LedMatrix& matrix) {
    Serial.printf("[ZigbeeCommand] Color Temp: %u mireds\n", mireds);
    // mireds: 153 (6500K) .. 555 (1800K)
    // Nightlight temp: 0 = 1800K (555 mireds), 255 = 4000K (250 mireds)
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
