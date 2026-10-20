#include "effect_manager.h"
#include "core_effects.h"
#include "math_effects.h"
#include "audio_effect.h"
#include <math.h>

// =============================================================================
// EffectManager Implementation
// =============================================================================

EffectManager::EffectManager()
    : m_powerState(true),
      m_currentEffectIndex(0),
      m_targetBrightness(DEFAULT_BRIGHTNESS),
      m_currentFadeBrightness(DEFAULT_BRIGHTNESS),
      m_speed(128),
      m_micSensitivity(128),
      m_dimmerDirUp(false),
      m_lastDimmerStepTime(0),
      m_limitFlashType(LimitFlashType::NONE),
      m_limitFlashStartTime(0),
      m_nvsDirty(false),
      m_nvsDirtyTime(0),
      m_lastFrameTime(0),
      m_totalEffects(NUM_CORE_EFFECTS) {
    // Instantiate all 10 core effect slots
    m_effects[0] = new EffectNightlight();
    m_effects[1] = new EffectDawnDusk();
    m_effects[2] = new EffectFire();
    m_effects[3] = new EffectPerlinNoise();
    m_effects[4] = new EffectGameOfLife();
    m_effects[5] = new EffectLorenzAttractor();
    m_effects[6] = new EffectMetaballs();
    m_effects[7] = new EffectSandHourglass();
    m_effects[8] = new EffectAudioReactive();
    m_effects[9] = new EffectFireflies();

    for (int i = NUM_CORE_EFFECTS; i < MAX_TOTAL_EFFECTS; i++) {
        m_effects[i] = nullptr;
    }
}

EffectManager::~EffectManager() {
    for (int i = 0; i < m_totalEffects; i++) {
        delete m_effects[i];
        m_effects[i] = nullptr;
    }
}

void EffectManager::begin(LedMatrix& matrix) {
    // Open NVS namespace "magiclamp" in read-write mode
    m_prefs.begin("magiclamp", false);

    m_powerState = m_prefs.getBool("power", true);
    m_currentEffectIndex = m_prefs.getUChar("effect", 0);
    if (m_currentEffectIndex >= m_totalEffects) {
        m_currentEffectIndex = 0;
    }

    m_targetBrightness = m_prefs.getUChar("bright", DEFAULT_BRIGHTNESS);
    if (m_targetBrightness < 12) {
        m_targetBrightness = DEFAULT_BRIGHTNESS;
    }

    m_speed = m_prefs.getUChar("speed", 128);
    if (m_speed < 1) m_speed = 128;

    m_micSensitivity = m_prefs.getUChar("micsens", 128);
    if (m_micSensitivity < 1) m_micSensitivity = 128;

    for (int i = 0; i < NUM_CORE_EFFECTS; i++) {
        if (m_effects[i]) {
            m_effects[i]->setSpeed(m_speed);
        }
    }

    m_currentFadeBrightness = m_powerState ? static_cast<float>(m_targetBrightness) : 0.0f;
    matrix.setBrightness(static_cast<uint8_t>(m_currentFadeBrightness));

    Serial.printf("[EffectManager] NVS Loaded: Power=%s, Effect=%d (%s), Brightness=%d, Speed=%d, MicSens=%d\n",
                  m_powerState ? "ON" : "OFF",
                  m_currentEffectIndex,
                  getCurrentEffectName(),
                  m_targetBrightness,
                  m_speed,
                  m_micSensitivity);

    // Initialize the restored effect
    if (m_effects[m_currentEffectIndex]) {
        m_effects[m_currentEffectIndex]->init(matrix);
    }

    if (!m_powerState) {
        matrix.clear(true);
    }
}

void EffectManager::update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) {
    // Check pending delayed NVS write
    updateNvs();

    // Synchronize microphone sensitivity
    if (const_cast<MicI2S&>(mic).getSensitivity() != m_micSensitivity) {
        const_cast<MicI2S&>(mic).setSensitivity(m_micSensitivity);
    }

    // Smooth power fade management
    updateFade(matrix);

    // If lamp is completely OFF and fade has settled to 0, save CPU/RMT cycles
    if (!m_powerState && m_currentFadeBrightness <= 0.5f) {
        return;
    }

    uint32_t now = millis();
    constexpr uint32_t frameIntervalMs = 1000 / MATRIX_TARGET_FPS; // ~16 ms for 60 FPS
    if (now - m_lastFrameTime < frameIntervalMs) {
        return;
    }
    m_lastFrameTime = now;

    // Render active effect
    if (m_effects[m_currentEffectIndex]) {
        m_effects[m_currentEffectIndex]->update(matrix, imu, mic);
        renderLimitFlash(matrix);
        matrix.show();
    }
}

void EffectManager::updateFade(LedMatrix& matrix) {
    float target = m_powerState ? static_cast<float>(m_targetBrightness) : 0.0f;
    if (fabsf(m_currentFadeBrightness - target) > 0.5f) {
        // Smooth fade step: complete transition in ~FADE_DURATION_MS ms
        float step = 255.0f / (static_cast<float>(FADE_DURATION_MS) / 16.0f);
        if (m_currentFadeBrightness < target) {
            m_currentFadeBrightness = fminf(target, m_currentFadeBrightness + step);
        } else {
            m_currentFadeBrightness = fmaxf(target, m_currentFadeBrightness - step);
        }
        matrix.setBrightness(static_cast<uint8_t>(roundf(m_currentFadeBrightness)));

        if (!m_powerState && m_currentFadeBrightness <= 0.5f) {
            matrix.clear(true);
        }
    } else {
        m_currentFadeBrightness = target;
        matrix.setBrightness(static_cast<uint8_t>(roundf(m_currentFadeBrightness)));
        if (!m_powerState) {
            matrix.clear(true);
        }
    }
}

void EffectManager::nextEffect(LedMatrix& matrix) {
    if (m_totalEffects == 0) return;
    setEffect((m_currentEffectIndex + 1) % m_totalEffects, matrix);
}

void EffectManager::prevEffect(LedMatrix& matrix) {
    if (m_totalEffects == 0) return;
    setEffect((m_currentEffectIndex + m_totalEffects - 1) % m_totalEffects, matrix);
}

void EffectManager::setEffect(uint8_t index, LedMatrix& matrix) {
    if (index >= m_totalEffects) {
        index = 0;
    }

    m_currentEffectIndex = index;
    if (m_effects[m_currentEffectIndex]) {
        m_effects[m_currentEffectIndex]->init(matrix);
    }

    Serial.printf("[EffectManager] Switched to effect [%d]: %s\n",
                  m_currentEffectIndex,
                  getCurrentEffectName());

    markNvsDirty();
}

void EffectManager::togglePower() {
    if (m_powerState) {
        powerOff();
    } else {
        powerOn();
    }
}

void EffectManager::powerOn() {
    if (!m_powerState) {
        m_powerState = true;
        Serial.println(F("[EffectManager] Power ON"));
        markNvsDirty();
    }
}

void EffectManager::powerOff() {
    if (m_powerState) {
        m_powerState = false;
        Serial.println(F("[EffectManager] Power OFF"));
        markNvsDirty();
    }
}

void EffectManager::setBrightness(uint8_t brightness) {
    if (brightness < 12) brightness = 12;
    m_targetBrightness = brightness;
    if (m_powerState) {
        m_currentFadeBrightness = static_cast<float>(m_targetBrightness);
    }
    markNvsDirty();
}

void EffectManager::setSpeed(uint8_t speed) {
    if (speed < 1) speed = 1;
    m_speed = speed;
    for (int i = 0; i < NUM_CORE_EFFECTS; i++) {
        if (m_effects[i]) {
            m_effects[i]->setSpeed(m_speed);
        }
    }
    markNvsDirty();
}

void EffectManager::setMicSensitivity(uint8_t sensitivity) {
    if (sensitivity < 1) sensitivity = 1;
    m_micSensitivity = sensitivity;
    markNvsDirty();
}

void EffectManager::resetSettings(LedMatrix& matrix) {
    m_prefs.clear();
    m_powerState = true;
    m_currentEffectIndex = 0;
    m_targetBrightness = DEFAULT_BRIGHTNESS;
    m_speed = 128;
    m_micSensitivity = 128;
    for (int i = 0; i < NUM_CORE_EFFECTS; i++) {
        if (m_effects[i]) {
            m_effects[i]->setSpeed(m_speed);
        }
    }
    m_currentFadeBrightness = DEFAULT_BRIGHTNESS;
    matrix.setBrightness(DEFAULT_BRIGHTNESS);
    if (m_effects[0]) {
        m_effects[0]->init(matrix);
    }
    Serial.println(F("[EffectManager] Factory Reset: NVS cleared and defaults restored."));
}

void EffectManager::stepDimmer(LedMatrix& matrix) {
    uint32_t now = millis();
    if (now - m_lastDimmerStepTime < 25) {
        return;
    }
    m_lastDimmerStepTime = now;

    // Non-linear gamma-adapted step: 2-3 at low values, 6-8 at high values
    uint8_t step = 2;
    if (m_targetBrightness >= 180) {
        step = 7;
    } else if (m_targetBrightness >= 120) {
        step = 5;
    } else if (m_targetBrightness >= 50) {
        step = 3;
    } else {
        step = 2;
    }

    constexpr uint8_t MIN_BRIGHTNESS = 12;
    constexpr uint8_t MAX_BRIGHTNESS = 255;

    if (m_dimmerDirUp) {
        if (m_targetBrightness < MAX_BRIGHTNESS) {
            uint16_t nextBri = static_cast<uint16_t>(m_targetBrightness) + step;
            if (nextBri >= MAX_BRIGHTNESS) {
                m_targetBrightness = MAX_BRIGHTNESS;
                triggerLimitFlash(LimitFlashType::MAX_LIMIT);
            } else {
                m_targetBrightness = static_cast<uint8_t>(nextBri);
            }
        }
        // Do NOT reverse direction while holding! Stays at MAX until released
    } else {
        if (m_targetBrightness > MIN_BRIGHTNESS) {
            if (m_targetBrightness <= MIN_BRIGHTNESS + step) {
                m_targetBrightness = MIN_BRIGHTNESS;
                triggerLimitFlash(LimitFlashType::MIN_LIMIT);
            } else {
                m_targetBrightness -= step;
            }
        }
        // Do NOT reverse direction while holding! Stays at MIN until released
    }

    if (m_powerState) {
        m_currentFadeBrightness = static_cast<float>(m_targetBrightness);
        matrix.setBrightness(m_targetBrightness);
    }

    markNvsDirty();
}

void EffectManager::toggleDimmerDirection() {
    if (m_targetBrightness >= 255) {
        m_dimmerDirUp = false;
    } else if (m_targetBrightness <= 12) {
        m_dimmerDirUp = true;
    } else {
        m_dimmerDirUp = !m_dimmerDirUp;
    }
}

void EffectManager::triggerLimitFlash(LimitFlashType type) {
    m_limitFlashType = type;
    m_limitFlashStartTime = millis();
}

void EffectManager::renderLimitFlash(LedMatrix& matrix) {
    if (m_limitFlashType == LimitFlashType::NONE) {
        return;
    }

    uint32_t elapsed = millis() - m_limitFlashStartTime;
    // Two short flashes:
    // Flash 1: 0..70 ms
    // Gap:     70..140 ms
    // Flash 2: 140..210 ms
    // Finished: >= 210 ms
    if (elapsed >= 210) {
        m_limitFlashType = LimitFlashType::NONE;
        return;
    }

    bool flashOn = (elapsed < 70) || (elapsed >= 140 && elapsed < 210);
    uint8_t y = (m_limitFlashType == LimitFlashType::MAX_LIMIT) ? (MATRIX_HEIGHT - 1) : 0;
    CRGB color = (m_limitFlashType == LimitFlashType::MAX_LIMIT) ? CRGB(255, 255, 255) : CRGB(0, 255, 200);

    for (int x = 0; x < MATRIX_WIDTH; x++) {
        matrix.setPixel(x, y, flashOn ? color : CRGB::Black);
    }
}

const char* EffectManager::getCurrentEffectName() const {
    if (m_effects[m_currentEffectIndex]) {
        return m_effects[m_currentEffectIndex]->getName();
    }
    return "Unknown";
}

void EffectManager::markNvsDirty() {
    m_nvsDirty = true;
    m_nvsDirtyTime = millis();
}

void EffectManager::updateNvs() {
    if (m_nvsDirty && (millis() - m_nvsDirtyTime >= NVS_SAVE_DELAY_MS)) {
        m_nvsDirty = false;
        m_prefs.putBool("power", m_powerState);
        m_prefs.putUChar("effect", m_currentEffectIndex);
        m_prefs.putUChar("bright", m_targetBrightness);
        m_prefs.putUChar("speed", m_speed);
        m_prefs.putUChar("micsens", m_micSensitivity);

        Serial.printf("[NVS] Delayed write flushed: Power=%s, Effect=%d (%s), Brightness=%d, Speed=%d, MicSens=%d\n",
                      m_powerState ? "ON" : "OFF",
                      m_currentEffectIndex,
                      getCurrentEffectName(),
                      m_targetBrightness,
                      m_speed,
                      m_micSensitivity);
    }
}

bool EffectManager::registerCustomEffect(Effect* effect) {
    if (!effect || m_totalEffects >= MAX_TOTAL_EFFECTS) {
        return false;
    }
    m_effects[m_totalEffects] = effect;
    m_totalEffects++;
    Serial.printf("[EffectManager] Registered custom effect [%d]: %s (total: %d)\n",
                  m_totalEffects - 1, effect->getName(), m_totalEffects);
    return true;
}

void EffectManager::clearCustomEffects() {
    for (int i = NUM_CORE_EFFECTS; i < m_totalEffects; i++) {
        delete m_effects[i];
        m_effects[i] = nullptr;
    }
    m_totalEffects = NUM_CORE_EFFECTS;
    if (m_currentEffectIndex >= m_totalEffects) {
        m_currentEffectIndex = 0;
    }
}

Effect* EffectManager::getEffect(uint8_t index) const {
    if (index < m_totalEffects) {
        return m_effects[index];
    }
    return nullptr;
}

const char* EffectManager::getEffectName(uint8_t index) const {
    if (index < m_totalEffects && m_effects[index]) {
        return m_effects[index]->getName();
    }
    return "Unknown";
}

