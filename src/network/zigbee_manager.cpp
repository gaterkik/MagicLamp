#include "zigbee_manager.h"

ZigbeeManager::ZigbeeManager()
    : m_mode(RADIO_MODE_WIFI),
      m_matrix(nullptr) {}

void ZigbeeManager::begin(LedMatrix* matrix) {
    m_matrix = matrix;

    Preferences prefs;
    prefs.begin("radio", true);
    m_mode = prefs.getUChar("mode", RADIO_MODE_WIFI);
    prefs.end();

    if (m_mode > 1) {
        m_mode = RADIO_MODE_WIFI;
    }

    Serial.printf("[ZigbeeManager] Initialized. Current Radio Mode: %s (%d)\n",
                  getModeString(), m_mode);
}

bool ZigbeeManager::setMode(uint8_t mode, bool playAnim) {
    if (mode > 1) mode = RADIO_MODE_WIFI;

    bool changed = (m_mode != mode);
    m_mode = mode;

    Preferences prefs;
    prefs.begin("radio", false);
    prefs.putUChar("mode", m_mode);
    prefs.end();

    Serial.printf("[ZigbeeManager] Radio mode set to %s (%d)\n",
                  getModeString(), m_mode);

    if (playAnim && m_matrix) {
        playSwitchAnimation(m_mode);
    }

    return changed;
}

uint8_t ZigbeeManager::toggleMode() {
    uint8_t nextMode = (m_mode == RADIO_MODE_WIFI) ? RADIO_MODE_ZIGBEE : RADIO_MODE_WIFI;
    setMode(nextMode, true);
    return m_mode;
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
