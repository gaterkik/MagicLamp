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
      m_profilesDirty(false),
      m_profileSaveTime(0),
      m_lastScheduleCheckTime(0),
      m_dimmerDirUp(false),
      m_lastDimmerStepTime(0),
      m_limitFlashType(LimitFlashType::NONE),
      m_limitFlashStartTime(0),
      m_nvsDirty(false),
      m_nvsDirtyTime(0),
      m_lastFrameTime(0),
      m_totalEffects(NUM_CORE_EFFECTS) {
    // Initialize default profiles for all effects
    for (int i = 0; i < MAX_TOTAL_EFFECTS; i++) {
        m_profiles[i].brightness = 128;
        m_profiles[i].speed = 128;
        m_profiles[i].sensitivity = 128;
    }
    // Nightlight default warm brightness
    m_profiles[0].brightness = 80;

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

    static_cast<EffectDawnDusk*>(m_effects[1])->setSchedule(&m_schedule);

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
    // 1. Open NVS namespace "magiclamp" in read-write mode
    m_prefs.begin("magiclamp", false);

    m_powerState = m_prefs.getBool("power", true);
    m_currentEffectIndex = m_prefs.getUChar("effect", 0);
    if (m_currentEffectIndex >= m_totalEffects) {
        m_currentEffectIndex = 0;
    }

    // 2. Load individual effect profiles from "eff_prof" namespace
    Preferences profPrefs;
    profPrefs.begin("eff_prof", false);
    if (profPrefs.isKey("profiles")) {
        profPrefs.getBytes("profiles", m_profiles, sizeof(m_profiles));
    } else {
        profPrefs.putBytes("profiles", m_profiles, sizeof(m_profiles));
    }
    profPrefs.end();

    // Sanitize loaded profiles
    for (int i = 0; i < MAX_TOTAL_EFFECTS; i++) {
        if (m_profiles[i].brightness < 10) m_profiles[i].brightness = (i == 0 ? 80 : 128);
        if (m_profiles[i].speed < 1) m_profiles[i].speed = 128;
        if (m_profiles[i].sensitivity < 1) m_profiles[i].sensitivity = 128;
    }

    // 3. Load Dawn/Dusk schedule from "schedule" namespace
    Preferences schPrefs;
    schPrefs.begin("schedule", false);
    m_schedule.enabled = schPrefs.getBool("en", false);
    m_schedule.dawnHour = schPrefs.getUChar("dh", 7);
    m_schedule.dawnMinute = schPrefs.getUChar("dm", 0);
    m_schedule.dawnDurationMin = schPrefs.getUChar("dd", 10);
    m_schedule.duskHour = schPrefs.getUChar("sh", 23);
    m_schedule.duskMinute = schPrefs.getUChar("sm", 0);
    m_schedule.duskDurationMin = schPrefs.getUChar("sd", 30);
    schPrefs.end();

    // Apply active effect's individual profile
    m_targetBrightness = m_profiles[m_currentEffectIndex].brightness;
    m_speed = m_profiles[m_currentEffectIndex].speed;
    m_micSensitivity = m_profiles[m_currentEffectIndex].sensitivity;

    for (int i = 0; i < NUM_CORE_EFFECTS; i++) {
        if (m_effects[i]) {
            m_effects[i]->setSpeed(m_profiles[i].speed);
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
    // Check pending delayed NVS writes
    updateNvs();
    updateProfilesNvs();

    // Check automated Dawn/Dusk schedule
    updateSchedule(matrix);

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

    // Load individual profile parameters for selected effect
    m_targetBrightness = m_profiles[index].brightness;
    m_speed = m_profiles[index].speed;
    m_micSensitivity = m_profiles[index].sensitivity;

    if (m_effects[m_currentEffectIndex]) {
        m_effects[m_currentEffectIndex]->setSpeed(m_speed);
        m_effects[m_currentEffectIndex]->init(matrix);
    }

    if (m_powerState) {
        m_currentFadeBrightness = static_cast<float>(m_targetBrightness);
        matrix.setBrightness(m_targetBrightness);
    }

    Serial.printf("[EffectManager] Switched to effect [%d]: %s (Bri: %d, Spd: %d, Sens: %d)\n",
                  m_currentEffectIndex,
                  getCurrentEffectName(),
                  m_targetBrightness,
                  m_speed,
                  m_micSensitivity);

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
    if (brightness < 10) brightness = 10;
    m_targetBrightness = brightness;
    m_profiles[m_currentEffectIndex].brightness = brightness;
    if (m_powerState) {
        m_currentFadeBrightness = static_cast<float>(m_targetBrightness);
        FastLED.setBrightness(brightness);
    }
    markProfilesDirty();
    markNvsDirty();
}

void EffectManager::setSpeed(uint8_t speed) {
    if (speed < 1) speed = 1;
    m_speed = speed;
    m_profiles[m_currentEffectIndex].speed = speed;
    if (m_effects[m_currentEffectIndex]) {
        m_effects[m_currentEffectIndex]->setSpeed(m_speed);
    }
    markProfilesDirty();
    markNvsDirty();
}

void EffectManager::setMicSensitivity(uint8_t sensitivity) {
    if (sensitivity < 1) sensitivity = 1;
    m_micSensitivity = sensitivity;
    m_profiles[m_currentEffectIndex].sensitivity = sensitivity;
    markProfilesDirty();
    markNvsDirty();
}

void EffectManager::resetSettings(LedMatrix& matrix) {
    m_prefs.clear();

    Preferences profPrefs;
    profPrefs.begin("eff_prof", false);
    profPrefs.clear();
    profPrefs.end();

    Preferences schPrefs;
    schPrefs.begin("schedule", false);
    schPrefs.clear();
    schPrefs.end();

    m_powerState = true;
    m_currentEffectIndex = 0;
    m_targetBrightness = DEFAULT_BRIGHTNESS;
    m_speed = 128;
    m_micSensitivity = 128;

    for (int i = 0; i < MAX_TOTAL_EFFECTS; i++) {
        m_profiles[i].brightness = 128;
        m_profiles[i].speed = 128;
        m_profiles[i].sensitivity = 128;
    }
    m_profiles[0].brightness = 80;

    m_schedule = ScheduleConfig();

    for (int i = 0; i < NUM_CORE_EFFECTS; i++) {
        if (m_effects[i]) {
            m_effects[i]->setSpeed(m_speed);
        }
    }
    m_currentFadeBrightness = m_profiles[0].brightness;
    matrix.setBrightness(m_profiles[0].brightness);
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

    m_profiles[m_currentEffectIndex].brightness = m_targetBrightness;
    markProfilesDirty();
    markNvsDirty();
}

EffectProfile EffectManager::getProfile(uint8_t index) const {
    if (index < MAX_TOTAL_EFFECTS) {
        return m_profiles[index];
    }
    return EffectProfile();
}

void EffectManager::setProfile(uint8_t index, const EffectProfile& prof) {
    if (index < MAX_TOTAL_EFFECTS) {
        m_profiles[index] = prof;
        if (index == m_currentEffectIndex) {
            m_targetBrightness = prof.brightness;
            m_speed = prof.speed;
            m_micSensitivity = prof.sensitivity;
            if (m_effects[index]) {
                m_effects[index]->setSpeed(m_speed);
            }
        }
        markProfilesDirty();
    }
}

void EffectManager::setSchedule(const ScheduleConfig& cfg) {
    m_schedule = cfg;
    Preferences schPrefs;
    schPrefs.begin("schedule", false);
    schPrefs.putBool("en", m_schedule.enabled);
    schPrefs.putUChar("dh", m_schedule.dawnHour);
    schPrefs.putUChar("dm", m_schedule.dawnMinute);
    schPrefs.putUChar("dd", m_schedule.dawnDurationMin);
    schPrefs.putUChar("sh", m_schedule.duskHour);
    schPrefs.putUChar("sm", m_schedule.duskMinute);
    schPrefs.putUChar("sd", m_schedule.duskDurationMin);
    schPrefs.end();

    Serial.printf("[Schedule] Saved: Enabled=%s, Dawn=%02d:%02d (%d min), Dusk=%02d:%02d (%d min)\n",
                  m_schedule.enabled ? "YES" : "NO",
                  m_schedule.dawnHour, m_schedule.dawnMinute, m_schedule.dawnDurationMin,
                  m_schedule.duskHour, m_schedule.duskMinute, m_schedule.duskDurationMin);
}

void EffectManager::markProfilesDirty() {
    m_profilesDirty = true;
    m_profileSaveTime = millis() + 2000;
}

void EffectManager::updateProfilesNvs() {
    if (m_profilesDirty && (millis() >= m_profileSaveTime)) {
        m_profilesDirty = false;
        Preferences profPrefs;
        profPrefs.begin("eff_prof", false);
        profPrefs.putBytes("profiles", m_profiles, sizeof(m_profiles));
        profPrefs.end();
        Serial.println(F("[EffectManager] Saved all effect profiles to NVS (debounce)."));
    }
}

void EffectManager::updateSchedule(LedMatrix& matrix) {
    if (!m_schedule.enabled) return;

    uint32_t now = millis();
    if (now - m_lastScheduleCheckTime < 1000) return;
    m_lastScheduleCheckTime = now;

    time_t rawtime;
    time(&rawtime);
    if (rawtime <= 100000) return;

    time_t local = rawtime + TIMEZONE_OFFSET_SEC;
    struct tm* ti = gmtime(&local);
    if (!ti) return;

    int curSecOfDay = ti->tm_hour * 3600 + ti->tm_min * 60 + ti->tm_sec;

    // Dawn window
    int dawnTargetMin = m_schedule.dawnHour * 60 + m_schedule.dawnMinute;
    int dawnDurSec = (m_schedule.dawnDurationMin > 0 ? m_schedule.dawnDurationMin : 10) * 60;
    int dawnTargetSec = dawnTargetMin * 60;
    int dawnStartSec = (dawnTargetSec >= dawnDurSec) ? (dawnTargetSec - dawnDurSec) : (86400 + dawnTargetSec - dawnDurSec);

    bool inDawn = false;
    if (dawnStartSec <= dawnTargetSec) {
        if (curSecOfDay >= dawnStartSec && curSecOfDay <= dawnTargetSec) inDawn = true;
    } else {
        if (curSecOfDay >= dawnStartSec || curSecOfDay <= dawnTargetSec) inDawn = true;
    }

    // Dusk window
    int duskStartMin = m_schedule.duskHour * 60 + m_schedule.duskMinute;
    int duskDurSec = (m_schedule.duskDurationMin > 0 ? m_schedule.duskDurationMin : 30) * 60;
    int duskStartSec = duskStartMin * 60;

    // Wake up if dawn window starts and lamp is off
    if (inDawn && !m_powerState) {
        Serial.println(F("[Schedule] Dawn sunrise triggered: turning ON lamp and starting DawnDusk effect."));
        powerOn();
        if (m_currentEffectIndex != 1) {
            setEffect(1, matrix);
        }
    }

    // Sleep if dusk window ends and lamp is on in DawnDusk effect
    if (m_powerState && m_currentEffectIndex == 1) {
        if (curSecOfDay >= (duskStartSec + duskDurSec - 2) && curSecOfDay <= (duskStartSec + duskDurSec + 2)) {
            Serial.println(F("[Schedule] Dusk sunset ended: turning OFF lamp."));
            powerOff();
        }
    }
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

