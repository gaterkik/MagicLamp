#include "effect_manager.h"
#include "core_effects.h"
#include "math_effects.h"
#include "audio_effect.h"
#include "extra_effects.h"
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
      m_cycleMask(DEFAULT_CYCLE_MASK),
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
      m_crossfading(false),
      m_crossfadeStartTime(0),
      m_curtainState(CurtainState::OPEN),
      m_curtainStartTime(0),
      m_otaState(OTA_STATE_IDLE),
      m_otaProgress(0),
      m_otaStateStartTime(0),
      m_sleepNowActive(false),
      m_sleepNowStartMs(0),
      m_sleepNowDurationMs(0) {
    // Initialize default profiles for all 15 built-in effects
    for (int i = 0; i < NUM_CORE_EFFECTS; i++) {
        m_profiles[i].brightness = 128;
        m_profiles[i].speed = 128;
        m_profiles[i].sensitivity = 128;
        m_profiles[i].inCycle = true;
        m_profiles[i].param1 = 90;
    }
    // Nightlight default warm brightness
    m_profiles[0].brightness = 80;
    m_profiles[0].param1 = 90;

    memset(m_prevBuffer, 0, sizeof(m_prevBuffer));

    // Instantiate all 15 built-in native effects (0..14)
    m_effects[0]  = new EffectNightlight();
    m_effects[1]  = new EffectDawnDusk();
    m_effects[2]  = new EffectFire();
    m_effects[3]  = new EffectPerlinNoise();
    m_effects[4]  = new EffectGameOfLife();
    m_effects[5]  = new EffectLorenzAttractor();
    m_effects[6]  = new EffectMetaballs();
    m_effects[7]  = new EffectAudioReactive();
    m_effects[8]  = new EffectFireflies();
    m_effects[9]  = new EffectAurora();
    m_effects[10] = new EffectMatrixRain();
    m_effects[11] = new EffectLavaLamp();
    m_effects[12] = new EffectVortex();
    m_effects[13] = new EffectFireworks();
    m_effects[14] = new EffectCyberPulse();

    static_cast<EffectDawnDusk*>(m_effects[1])->setSchedule(&m_schedule);
}

EffectManager::~EffectManager() {
    for (int i = 0; i < NUM_CORE_EFFECTS; i++) {
        delete m_effects[i];
        m_effects[i] = nullptr;
    }
}

void EffectManager::begin(LedMatrix& matrix) {
    // 1. Open NVS namespace "magiclamp" in read-write mode
    m_prefs.begin("magiclamp", false);

    m_powerState = m_prefs.getBool("power_on", m_prefs.getBool("power", true));
    m_currentEffectIndex = m_prefs.getUChar("effect", 0);
    if (m_currentEffectIndex >= NUM_CORE_EFFECTS) {
        m_currentEffectIndex = 0;
    }
    m_cycleMask = m_prefs.getUShort("cycle_mask", DEFAULT_CYCLE_MASK);
    m_cycleMask &= DEFAULT_CYCLE_MASK;
    if (m_cycleMask == 0) m_cycleMask = DEFAULT_CYCLE_MASK;

    // 2. Load individual effect profiles from "eff_prof" namespace
    Preferences profPrefs;
    profPrefs.begin("eff_prof", false);
    if (profPrefs.isKey("profiles")) {
        profPrefs.getBytes("profiles", m_profiles, sizeof(m_profiles));
    } else {
        profPrefs.putBytes("profiles", m_profiles, sizeof(m_profiles));
    }
    profPrefs.end();

    // Sanitize loaded profiles and synchronize inCycle with cycleMask
    for (int i = 0; i < NUM_CORE_EFFECTS; i++) {
        if (m_profiles[i].brightness < 10) m_profiles[i].brightness = (i == 0 ? 80 : 128);
        if (m_profiles[i].speed < 1) m_profiles[i].speed = 128;
        if (m_profiles[i].sensitivity < 1) m_profiles[i].sensitivity = 128;
        if (m_profiles[i].param1 == 0 && i != 0) m_profiles[i].param1 = 90;
        m_profiles[i].inCycle = ((m_cycleMask & (1 << i)) != 0);
    }
    if (m_profiles[0].param1 == 0) m_profiles[0].param1 = 90;

    // 3. Load Dawn/Dusk schedule from "schedule" namespace
    Preferences schPrefs;
    schPrefs.begin("schedule", false);
    if (schPrefs.isKey("dawn_en")) {
        m_schedule.dawnEnabled = schPrefs.getBool("dawn_en", false);
        m_schedule.dawnHour = schPrefs.getUChar("dawn_h", 7);
        m_schedule.dawnMinute = schPrefs.getUChar("dawn_m", 0);
        m_schedule.duskEnabled = schPrefs.getBool("dusk_en", false);
        m_schedule.duskHour = schPrefs.getUChar("dusk_h", 23);
        m_schedule.duskMinute = schPrefs.getUChar("dusk_m", 0);
    } else {
        bool oldEn = schPrefs.getBool("en", false);
        m_schedule.dawnEnabled = oldEn;
        m_schedule.dawnHour = schPrefs.getUChar("dh", 7);
        m_schedule.dawnMinute = schPrefs.getUChar("dm", 0);
        m_schedule.duskEnabled = oldEn;
        m_schedule.duskHour = schPrefs.getUChar("sh", 23);
        m_schedule.duskMinute = schPrefs.getUChar("sm", 0);
    }
    m_schedule.dawnDurationMin = schPrefs.getUChar("dawn_dur", schPrefs.getUChar("dd", 5));
    m_schedule.duskDurationMin = schPrefs.getUChar("dusk_dur", schPrefs.getUChar("sd", 30));
    if (m_schedule.dawnDurationMin == 0) m_schedule.dawnDurationMin = 5;
    if (m_schedule.duskDurationMin == 0) m_schedule.duskDurationMin = 30;
    m_schedule.enabled = m_schedule.isEnabled();
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
    if (m_effects[0]) {
        static_cast<EffectNightlight*>(m_effects[0])->setColorTemp(m_profiles[0].param1);
    }

    m_curtainState = m_powerState ? CurtainState::OPEN : CurtainState::CLOSED;
    matrix.setBrightness(m_powerState ? m_targetBrightness : 0);

    Serial.printf("[EffectManager] NVS Loaded: Power=%s, Effect=%d (%s), Brightness=%d, Speed=%d, MicSens=%d, CycleMask=0x%04X, NightlightTemp=%d\n",
                  m_powerState ? "ON" : "OFF",
                  m_currentEffectIndex,
                  getCurrentEffectName(),
                  m_targetBrightness,
                  m_speed,
                  m_micSensitivity,
                  m_cycleMask,
                  m_profiles[0].param1);

    // Initialize the restored effect
    if (m_effects[m_currentEffectIndex]) {
        m_effects[m_currentEffectIndex]->init(matrix);
    }

    if (!m_powerState) {
        matrix.setBrightness(0);
        FastLED.setBrightness(0);
        matrix.clear(true);
        FastLED.clear(true);
        FastLED.show();
    }
}

void EffectManager::update(LedMatrix& matrix, const MicI2S& mic) {
    // Check pending delayed NVS writes
    updateNvs();
    updateProfilesNvs();

    // Check automated Dawn/Dusk schedule
    updateSchedule(matrix);

    // If OTA update/check is in progress, render OTA animation exclusively and block effects
    if (m_otaState != OTA_STATE_IDLE) {
        renderOta(matrix);
        return;
    }

    // Synchronize microphone sensitivity
    if (const_cast<MicI2S&>(mic).getSensitivity() != m_micSensitivity) {
        const_cast<MicI2S&>(mic).setSensitivity(m_micSensitivity);
    }

    // If lamp is fully CLOSED/OFF, save CPU/RMT cycles
    if (m_curtainState == CurtainState::CLOSED) {
        return;
    }

    uint32_t now = millis();
    constexpr uint32_t frameIntervalMs = 1000 / MATRIX_TARGET_FPS; // ~16 ms for 60 FPS
    if (now - m_lastFrameTime < frameIntervalMs) {
        return;
    }
    m_lastFrameTime = now;

    // 1. Render active effect frame
    if (m_effects[m_currentEffectIndex]) {
        m_effects[m_currentEffectIndex]->update(matrix, mic);
    }

    // 2. Crossfade 500 ms between previous and current effect
    if (m_crossfading) {
        uint32_t elapsed = now - m_crossfadeStartTime;
        if (elapsed >= CROSSFADE_DURATION_MS) {
            m_crossfading = false;
        } else {
            fract8 amount = static_cast<fract8>((elapsed * 255) / CROSSFADE_DURATION_MS);
            for (int i = 0; i < NUM_LEDS; i++) {
                CRGB curr = matrix.getPixelRaw(i);
                matrix.setPixelRaw(i, blend(m_prevBuffer[i], curr, amount));
            }
        }
    }

    // 3. Render dimmer limit flashes if triggered
    renderLimitFlash(matrix);

    // 4. Apply power curtain animation (rising on turn-on, falling on turn-off)
    applyCurtain(matrix);

    matrix.show();
}

void EffectManager::applyCurtain(LedMatrix& matrix) {
    if (m_curtainState == CurtainState::OPEN) {
        return;
    }

    if (m_curtainState == CurtainState::CLOSED) {
        matrix.clear(false);
        return;
    }

    uint32_t now = millis();
    uint32_t elapsed = now - m_curtainStartTime;

    if (m_curtainState == CurtainState::OPENING) {
        if (elapsed >= CURTAIN_DURATION_MS) {
            m_curtainState = CurtainState::OPEN;
            return;
        }
        float progress = static_cast<float>(elapsed) / static_cast<float>(CURTAIN_DURATION_MS);
        float curY = progress * 16.0f;

        for (int y = 0; y < MATRIX_HEIGHT; y++) {
            float factor = constrain(curY - static_cast<float>(y), 0.0f, 1.0f);
            uint8_t scale = static_cast<uint8_t>(factor * 255.0f);
            for (int x = 0; x < MATRIX_WIDTH; x++) {
                CRGB p = matrix.getPixel(x, y);
                p.nscale8_video(scale);
                matrix.setPixel(x, y, p);
            }
        }
    } else if (m_curtainState == CurtainState::CLOSING) {
        if (elapsed >= CURTAIN_DURATION_MS) {
            m_curtainState = CurtainState::CLOSED;
            matrix.clear(true);
            return;
        }
        float progress = static_cast<float>(elapsed) / static_cast<float>(CURTAIN_DURATION_MS);
        float curY = 16.0f * (1.0f - progress);

        for (int y = 0; y < MATRIX_HEIGHT; y++) {
            float factor = constrain(curY - static_cast<float>(y), 0.0f, 1.0f);
            uint8_t scale = static_cast<uint8_t>(factor * 255.0f);
            for (int x = 0; x < MATRIX_WIDTH; x++) {
                CRGB p = matrix.getPixel(x, y);
                p.nscale8_video(scale);
                matrix.setPixel(x, y, p);
            }
        }
    }
}

void EffectManager::nextEffect(LedMatrix& matrix) {
    uint8_t next = m_currentEffectIndex;
    for (uint8_t i = 1; i <= NUM_CORE_EFFECTS; i++) {
        uint8_t cand = (m_currentEffectIndex + i) % NUM_CORE_EFFECTS;
        if (m_profiles[cand].inCycle && ((m_cycleMask & (1 << cand)) != 0)) {
            next = cand;
            break;
        }
    }
    setEffect(next, matrix);
}

void EffectManager::prevEffect(LedMatrix& matrix) {
    uint8_t prev = m_currentEffectIndex;
    for (uint8_t i = 1; i <= NUM_CORE_EFFECTS; i++) {
        uint8_t cand = (m_currentEffectIndex + NUM_CORE_EFFECTS - i) % NUM_CORE_EFFECTS;
        if (m_profiles[cand].inCycle && ((m_cycleMask & (1 << cand)) != 0)) {
            prev = cand;
            break;
        }
    }
    setEffect(prev, matrix);
}

void EffectManager::setEffect(uint8_t index, LedMatrix& matrix) {
    if (index >= NUM_CORE_EFFECTS) {
        index = 0;
    }

    if (index != 1 && m_sleepNowActive) {
        stopSleepNow();
    }

    if (m_powerState && m_curtainState != CurtainState::CLOSED) {
        // Snapshot current frame buffer for 500 ms crossfade
        for (int i = 0; i < NUM_LEDS; i++) {
            m_prevBuffer[i] = matrix.getPixelRaw(i);
        }
        m_crossfading = true;
        m_crossfadeStartTime = millis();
    } else {
        m_crossfading = false;
    }

    m_currentEffectIndex = index;

    // Load individual profile parameters for selected effect
    m_targetBrightness = m_profiles[index].brightness;
    m_speed = m_profiles[index].speed;
    m_micSensitivity = m_profiles[index].sensitivity;

    if (m_effects[m_currentEffectIndex]) {
        m_effects[m_currentEffectIndex]->setSpeed(m_speed);
        if (m_currentEffectIndex == 0) {
            static_cast<EffectNightlight*>(m_effects[0])->setColorTemp(m_profiles[0].param1);
        }
        m_effects[m_currentEffectIndex]->init(matrix);
    }

    if (m_powerState) {
        matrix.setBrightness(m_targetBrightness);
    }

    Serial.printf("[EffectManager] Switched to effect [%d]: %s (Bri: %d, Spd: %d, Sens: %d, inCycle: %d)\n",
                  m_currentEffectIndex,
                  getCurrentEffectName(),
                  m_targetBrightness,
                  m_speed,
                  m_micSensitivity,
                  m_profiles[m_currentEffectIndex].inCycle ? 1 : 0);

    markNvsDirty();
}

void EffectManager::togglePower() {
    if (m_powerState && (m_curtainState == CurtainState::OPEN || m_curtainState == CurtainState::OPENING)) {
        powerOff();
    } else {
        powerOn();
    }
}

void EffectManager::powerOn() {
    if (!m_powerState || m_curtainState == CurtainState::CLOSING || m_curtainState == CurtainState::CLOSED) {
        m_powerState = true;
        m_curtainState = CurtainState::OPENING;
        m_curtainStartTime = millis();
        FastLED.setBrightness(m_targetBrightness);
        m_prefs.putBool("power_on", true);
        m_prefs.putBool("power", true);
        Serial.println(F("[EffectManager] Power ON: curtain opening 0..15 over 600 ms"));
        markNvsDirty();
    }
}

void EffectManager::powerOff() {
    stopSleepNow();
    if (m_powerState || m_curtainState == CurtainState::OPENING || m_curtainState == CurtainState::OPEN) {
        m_powerState = false;
        m_curtainState = CurtainState::CLOSING;
        m_curtainStartTime = millis();
        m_prefs.putBool("power_on", false);
        m_prefs.putBool("power", false);
        Serial.println(F("[EffectManager] Power OFF: curtain closing 15..0 over 600 ms"));
        markNvsDirty();
    }
}

void EffectManager::setPower(bool on) {
    if (on) {
        powerOn();
    } else {
        powerOff();
    }
}

void EffectManager::setBrightness(uint8_t brightness) {
    if (brightness < 10) brightness = 10;
    m_targetBrightness = brightness;
    m_profiles[m_currentEffectIndex].brightness = brightness;
    if (m_powerState) {
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
    m_curtainState = CurtainState::OPEN;
    m_currentEffectIndex = 0;
    m_targetBrightness = DEFAULT_BRIGHTNESS;
    m_speed = 128;
    m_micSensitivity = 128;
    m_cycleMask = DEFAULT_CYCLE_MASK;

    for (int i = 0; i < NUM_CORE_EFFECTS; i++) {
        m_profiles[i].brightness = 128;
        m_profiles[i].speed = 128;
        m_profiles[i].sensitivity = 128;
        m_profiles[i].inCycle = true;
    }
    m_profiles[0].brightness = 80;

    m_schedule = ScheduleConfig();

    for (int i = 0; i < NUM_CORE_EFFECTS; i++) {
        if (m_effects[i]) {
            m_effects[i]->setSpeed(m_speed);
        }
    }
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
    } else {
        if (m_targetBrightness > MIN_BRIGHTNESS) {
            if (m_targetBrightness <= MIN_BRIGHTNESS + step) {
                m_targetBrightness = MIN_BRIGHTNESS;
                triggerLimitFlash(LimitFlashType::MIN_LIMIT);
            } else {
                m_targetBrightness -= step;
            }
        }
    }

    if (m_powerState) {
        matrix.setBrightness(m_targetBrightness);
    }

    m_profiles[m_currentEffectIndex].brightness = m_targetBrightness;
    markProfilesDirty();
    markNvsDirty();
}

EffectProfile EffectManager::getProfile(uint8_t index) const {
    if (index < NUM_CORE_EFFECTS) {
        return m_profiles[index];
    }
    return EffectProfile();
}

void EffectManager::setProfile(uint8_t index, const EffectProfile& prof) {
    if (index < NUM_CORE_EFFECTS) {
        m_profiles[index] = prof;
        if (index == 0 && m_effects[0]) {
            static_cast<EffectNightlight*>(m_effects[0])->setColorTemp(prof.param1);
        }
        if (prof.inCycle) {
            m_cycleMask |= (1 << index);
        } else {
            m_cycleMask &= ~(1 << index);
        }
        if (index == m_currentEffectIndex) {
            m_targetBrightness = prof.brightness;
            m_speed = prof.speed;
            m_micSensitivity = prof.sensitivity;
            if (m_effects[index]) {
                m_effects[index]->setSpeed(m_speed);
            }
            if (m_powerState) {
                FastLED.setBrightness(m_targetBrightness);
            }
        }
        markProfilesDirty();
        markNvsDirty();
    }
}

void EffectManager::setCycleMask(uint16_t mask) {
    m_cycleMask = mask & DEFAULT_CYCLE_MASK;
    for (int i = 0; i < NUM_CORE_EFFECTS; i++) {
        m_profiles[i].inCycle = ((m_cycleMask & (1 << i)) != 0);
    }
    markProfilesDirty();
    markNvsDirty();
}

void EffectManager::setSchedule(const ScheduleConfig& cfg) {
    m_schedule = cfg;
    m_schedule.enabled = m_schedule.isEnabled();
    if (m_schedule.dawnDurationMin == 0) m_schedule.dawnDurationMin = 5;
    if (m_schedule.duskDurationMin == 0) m_schedule.duskDurationMin = 30;

    Preferences schPrefs;
    schPrefs.begin("schedule", false);
    schPrefs.putBool("dawn_en", m_schedule.dawnEnabled);
    schPrefs.putUChar("dawn_h", m_schedule.dawnHour);
    schPrefs.putUChar("dawn_m", m_schedule.dawnMinute);
    schPrefs.putUChar("dawn_dur", m_schedule.dawnDurationMin);
    schPrefs.putUChar("dd", m_schedule.dawnDurationMin);

    schPrefs.putBool("dusk_en", m_schedule.duskEnabled);
    schPrefs.putUChar("dusk_h", m_schedule.duskHour);
    schPrefs.putUChar("dusk_m", m_schedule.duskMinute);
    schPrefs.putUChar("dusk_dur", m_schedule.duskDurationMin);
    schPrefs.putUChar("sd", m_schedule.duskDurationMin);

    schPrefs.putBool("en", m_schedule.enabled);
    schPrefs.putUChar("dh", m_schedule.dawnHour);
    schPrefs.putUChar("dm", m_schedule.dawnMinute);
    schPrefs.putUChar("sh", m_schedule.duskHour);
    schPrefs.putUChar("sm", m_schedule.duskMinute);
    schPrefs.end();

    Serial.printf("[Schedule] Saved: Dawn=%02d:%02d (en=%s, %d min), Dusk=%02d:%02d (en=%s, %d min)\n",
                  m_schedule.dawnHour, m_schedule.dawnMinute, m_schedule.dawnEnabled ? "YES" : "NO", m_schedule.dawnDurationMin,
                  m_schedule.duskHour, m_schedule.duskMinute, m_schedule.duskEnabled ? "YES" : "NO", m_schedule.duskDurationMin);
}

void EffectManager::setDawnDurationMin(uint8_t min) {
    m_schedule.dawnDurationMin = (min > 0) ? min : 5;
    setSchedule(m_schedule);
}

void EffectManager::setDuskDurationMin(uint8_t min) {
    m_schedule.duskDurationMin = (min > 0) ? min : 30;
    setSchedule(m_schedule);
}

void EffectManager::startSleepNow(LedMatrix& matrix) {
    m_sleepNowActive = true;
    m_sleepNowStartMs = millis();
    uint8_t durMin = (m_schedule.duskDurationMin > 0) ? m_schedule.duskDurationMin : 30;
    m_sleepNowDurationMs = static_cast<uint32_t>(durMin) * 60UL * 1000UL;

    m_schedule.sleepNowActive = true;
    m_schedule.sleepNowStartMs = m_sleepNowStartMs;
    m_schedule.sleepNowDurationMs = m_sleepNowDurationMs;

    if (!m_powerState) {
        powerOn();
    }
    if (m_currentEffectIndex != 1) {
        setEffect(1, matrix);
    }
    Serial.printf("[Schedule] Sleep Now started for %d min.\n", durMin);
}

void EffectManager::stopSleepNow() {
    if (m_sleepNowActive) {
        m_sleepNowActive = false;
        m_schedule.sleepNowActive = false;
        m_schedule.sleepNowStartMs = 0;
        m_schedule.sleepNowDurationMs = 0;
        Serial.println(F("[Schedule] Sleep Now stopped."));
    }
}

String EffectManager::getScheduleStatus() const {
    if (m_sleepNowActive) {
        uint32_t elapsedMs = millis() - m_sleepNowStartMs;
        if (elapsedMs < m_sleepNowDurationMs) {
            uint32_t remSec = (m_sleepNowDurationMs - elapsedMs) / 1000;
            uint32_t remMin = (remSec + 59) / 60;
            return "Таймер сна активен (осталось " + String(remMin) + " мин)";
        }
        return "Таймер сна завершен";
    }

    if (!m_schedule.isEnabled()) {
        return "Расписание выключено";
    }

    time_t rawtime;
    time(&rawtime);
    if (rawtime <= 100000) {
        return "Ожидание NTP синхронизации";
    }

    time_t local = rawtime + TIMEZONE_OFFSET_SEC;
    struct tm* ti = gmtime(&local);
    if (!ti) return "Ожидание расписания";

    int curSecOfDay = ti->tm_hour * 3600 + ti->tm_min * 60 + ti->tm_sec;

    // Check Dawn
    if (m_schedule.dawnEnabled) {
        int dawnTargetMin = m_schedule.dawnHour * 60 + m_schedule.dawnMinute;
        int dawnDurSec = (m_schedule.dawnDurationMin > 0 ? m_schedule.dawnDurationMin : 5) * 60;
        int dawnTargetSec = dawnTargetMin * 60;
        int dawnStartSec = (dawnTargetSec >= dawnDurSec) ? (dawnTargetSec - dawnDurSec) : (86400 + dawnTargetSec - dawnDurSec);

        bool inDawn = false;
        int elapsed = 0;
        if (dawnStartSec <= dawnTargetSec) {
            if (curSecOfDay >= dawnStartSec && curSecOfDay <= dawnTargetSec) {
                inDawn = true;
                elapsed = curSecOfDay - dawnStartSec;
            }
        } else {
            if (curSecOfDay >= dawnStartSec || curSecOfDay <= dawnTargetSec) {
                inDawn = true;
                elapsed = (curSecOfDay >= dawnStartSec) ? (curSecOfDay - dawnStartSec) : (86400 - dawnStartSec + curSecOfDay);
            }
        }
        if (inDawn) {
            int remSec = dawnDurSec - elapsed;
            if (remSec < 0) remSec = 0;
            int remMin = (remSec + 59) / 60;
            return "Рассвет активен (осталось " + String(remMin) + " мин)";
        }
    }

    // Check Dusk
    if (m_schedule.duskEnabled) {
        int duskTargetMin = m_schedule.duskHour * 60 + m_schedule.duskMinute;
        int duskDurSec = (m_schedule.duskDurationMin > 0 ? m_schedule.duskDurationMin : 30) * 60;
        int duskTargetSec = duskTargetMin * 60;
        int duskStartSec = (duskTargetSec >= duskDurSec) ? (duskTargetSec - duskDurSec) : (86400 + duskTargetSec - duskDurSec);

        bool inDusk = false;
        int elapsed = 0;
        if (duskStartSec <= duskTargetSec) {
            if (curSecOfDay >= duskStartSec && curSecOfDay <= duskTargetSec) {
                inDusk = true;
                elapsed = curSecOfDay - duskStartSec;
            }
        } else {
            if (curSecOfDay >= duskStartSec || curSecOfDay <= duskTargetSec) {
                inDusk = true;
                elapsed = (curSecOfDay >= duskStartSec) ? (curSecOfDay - duskStartSec) : (86400 - duskStartSec + curSecOfDay);
            }
        }
        if (inDusk) {
            int remSec = duskDurSec - elapsed;
            if (remSec < 0) remSec = 0;
            int remMin = (remSec + 59) / 60;
            return "Таймер сна активен (осталось " + String(remMin) + " мин)";
        }
    }

    return "Ожидание расписания";
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

        m_prefs.putUShort("cycle_mask", m_cycleMask);
        Serial.printf("[EffectManager] Saved all 16 profiles & cycleMask (0x%04X) to NVS.\n", m_cycleMask);
    }
}

void EffectManager::updateSchedule(LedMatrix& matrix) {
    // 0. Sleep Now mode active
    if (m_sleepNowActive) {
        uint32_t elapsed = millis() - m_sleepNowStartMs;
        if (elapsed >= m_sleepNowDurationMs) {
            Serial.println(F("[Schedule] Sleep Now timer complete: turning OFF lamp."));
            stopSleepNow();
            powerOff();
            return;
        }
        if (!m_powerState) {
            powerOn();
        }
        if (m_currentEffectIndex != 1) {
            setEffect(1, matrix);
        }
        return;
    }

    if (!m_schedule.isEnabled()) return;

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

    // 1. Dawn Alarm (Wake-up)
    if (m_schedule.dawnEnabled) {
        int dawnTargetMin = m_schedule.dawnHour * 60 + m_schedule.dawnMinute;
        int dawnDurSec = (m_schedule.dawnDurationMin > 0 ? m_schedule.dawnDurationMin : 5) * 60;
        int dawnTargetSec = dawnTargetMin * 60;
        int dawnStartSec = (dawnTargetSec >= dawnDurSec) ? (dawnTargetSec - dawnDurSec) : (86400 + dawnTargetSec - dawnDurSec);

        bool inDawn = false;
        if (dawnStartSec <= dawnTargetSec) {
            if (curSecOfDay >= dawnStartSec && curSecOfDay <= dawnTargetSec) inDawn = true;
        } else {
            if (curSecOfDay >= dawnStartSec || curSecOfDay <= dawnTargetSec) inDawn = true;
        }

        // Within wake-up ramp before alarm time: turn ON lamp from any state and start Dawn mode
        if (inDawn) {
            if (!m_powerState) {
                Serial.println(F("[Schedule] Dawn alarm triggered: turning ON lamp in Dawn mode."));
                powerOn();
            }
            if (m_currentEffectIndex != 1) {
                setEffect(1, matrix);
            }
        }
    }

    // 2. Dusk Sleep Timer
    if (m_schedule.duskEnabled) {
        int duskTargetMin = m_schedule.duskHour * 60 + m_schedule.duskMinute;
        int duskDurSec = (m_schedule.duskDurationMin > 0 ? m_schedule.duskDurationMin : 30) * 60;
        int duskTargetSec = duskTargetMin * 60;
        int duskStartSec = (duskTargetSec >= duskDurSec) ? (duskTargetSec - duskDurSec) : (86400 + duskTargetSec - duskDurSec);

        bool inDusk = false;
        if (duskStartSec <= duskTargetSec) {
            if (curSecOfDay >= duskStartSec && curSecOfDay <= duskTargetSec) inDusk = true;
        } else {
            if (curSecOfDay >= duskStartSec || curSecOfDay <= duskTargetSec) inDusk = true;
        }

        // In dusk window: regardless of active effect, switch to safe 1900K amber mode (DawnDusk effect index 1)
        if (inDusk && m_powerState) {
            if (m_currentEffectIndex != 1) {
                Serial.println(F("[Schedule] Dusk sleep timer: switching to 1900K amber sleep mode."));
                setEffect(1, matrix);
            }
            // Target moment reached -> turn OFF
            int diffToTarget = (curSecOfDay >= duskTargetSec) ? (curSecOfDay - duskTargetSec) : (86400 - duskTargetSec + curSecOfDay);
            if (curSecOfDay == duskTargetSec || diffToTarget <= 2) {
                Serial.println(F("[Schedule] Dusk fade complete at target time: turning OFF lamp."));
                powerOff();
            }
        }
    }
}

void EffectManager::setOtaState(OtaState state, uint8_t progress) {
    if (m_otaState != state) {
        m_otaState = state;
        m_otaStateStartTime = millis();
        Serial.printf("[OTA] State changed to %d (progress=%d%%)\n", static_cast<int>(state), progress);
    }
    m_otaProgress = progress;
}

void EffectManager::renderOta(LedMatrix& matrix) {
    if (m_otaState == OTA_STATE_IDLE) {
        return;
    }

    uint32_t now = millis();
    uint32_t elapsed = now - m_otaStateStartTime;

    // Stealth Mode: if lamp is powered off, matrix remains completely black
    if (!isPoweredOn()) {
        matrix.clear(false);
        matrix.show();
        if ((m_otaState == OTA_STATE_UP_TO_DATE || m_otaState == OTA_STATE_ERROR) && elapsed >= 1200) {
            m_otaState = OTA_STATE_IDLE;
        } else if (m_otaState == OTA_STATE_SUCCESS && elapsed >= 1200) {
            matrix.clear(true);
            ESP.restart();
        }
        return;
    }

    // Lamp is ON: render visual indicator according to OTA state
    switch (m_otaState) {
        case OTA_STATE_CHECKING: {
            matrix.clear(false);
            // Fast rotating bright white-blue radar dot on top ring Y=15
            uint8_t radarX = static_cast<uint8_t>((now / 40) % MATRIX_WIDTH);
            matrix.setPixel(radarX, MATRIX_HEIGHT - 1, CRGB(220, 245, 255));
            matrix.setPixel((radarX + MATRIX_WIDTH - 1) % MATRIX_WIDTH, MATRIX_HEIGHT - 1, CRGB(80, 160, 240));
            matrix.setPixel((radarX + MATRIX_WIDTH - 2) % MATRIX_WIDTH, MATRIX_HEIGHT - 1, CRGB(20, 70, 150));
            matrix.show();
            break;
        }

        case OTA_STATE_UP_TO_DATE: {
            if (elapsed >= 1200) {
                m_otaState = OTA_STATE_IDLE;
                return;
            }
            matrix.clear(false);
            // Two top rings Y=14..15 double-pulse smoothly in emerald green over 1.2s
            float phase = static_cast<float>(elapsed % 600) / 600.0f;
            float pulse = sinf(phase * 3.14159265f);
            uint8_t scale = static_cast<uint8_t>(pulse * 255.0f);
            CRGB emCol = CRGB(0, 230, 90);
            emCol.nscale8_video(scale);
            for (int y = 14; y <= 15; y++) {
                for (int x = 0; x < MATRIX_WIDTH; x++) {
                    matrix.setPixel(x, y, emCol);
                }
            }
            matrix.show();
            break;
        }

        case OTA_STATE_DOWNLOADING: {
            matrix.clear(false);
            uint8_t pct = m_otaProgress;
            if (pct > 100) pct = 100;
            float fillHeight = (static_cast<float>(pct) / 100.0f) * static_cast<float>(MATRIX_HEIGHT);
            uint8_t pulse = sin8(static_cast<uint8_t>(now / 4));
            float pulseFactor = 0.5f + 0.5f * (static_cast<float>(pulse) / 255.0f);

            for (int y = 0; y < MATRIX_HEIGHT; y++) {
                float fy = static_cast<float>(y);
                CRGB rowColor = CRGB::Black;
                if (fy < fillHeight - 1.0f) {
                    rowColor = CRGB(0, 140, 240); // Soft cyan/turquoise-blue
                } else if (fy <= fillHeight) {
                    float edgeAlpha = fillHeight - fy;
                    rowColor = CRGB(0, static_cast<uint8_t>(180.0f * pulseFactor * edgeAlpha),
                                       static_cast<uint8_t>(255.0f * pulseFactor * edgeAlpha));
                } else {
                    rowColor = CRGB::Black;
                }
                for (int x = 0; x < MATRIX_WIDTH; x++) {
                    matrix.setPixel(x, y, rowColor);
                }
            }
            matrix.show();
            break;
        }

        case OTA_STATE_SUCCESS: {
            if (elapsed >= 1200) {
                matrix.clear(true);
                ESP.restart();
                return;
            }
            matrix.clear(false);
            // 1.2-second emerald-green wave across the whole cylinder
            float phase = static_cast<float>(elapsed) / 1200.0f;
            float waveCenter = phase * 18.0f;
            for (int y = 0; y < MATRIX_HEIGHT; y++) {
                float dist = fabsf(static_cast<float>(y) - waveCenter);
                float bright = fmaxf(0.12f, 1.0f - dist * 0.28f);
                CRGB col = CRGB(0, static_cast<uint8_t>(bright * 255), static_cast<uint8_t>(bright * 120));
                for (int x = 0; x < MATRIX_WIDTH; x++) {
                    matrix.setPixel(x, y, col);
                }
            }
            matrix.show();
            break;
        }

        case OTA_STATE_ERROR: {
            if (elapsed >= 1200) {
                m_otaState = OTA_STATE_IDLE;
                return;
            }
            matrix.clear(false);
            // Two top rings Y=14..15 double-pulse in red-orange
            float phase = static_cast<float>(elapsed % 600) / 600.0f;
            float pulse = sinf(phase * 3.14159265f);
            uint8_t scale = static_cast<uint8_t>(pulse * 255.0f);
            CRGB errCol = CRGB(255, 60, 0);
            errCol.nscale8_video(scale);
            for (int y = 14; y <= 15; y++) {
                for (int x = 0; x < MATRIX_WIDTH; x++) {
                    matrix.setPixel(x, y, errCol);
                }
            }
            matrix.show();
            break;
        }

        default:
            break;
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
        m_prefs.putBool("power_on", m_powerState);
        m_prefs.putBool("power", m_powerState);
        m_prefs.putUChar("effect", m_currentEffectIndex);
        m_prefs.putUChar("bright", m_targetBrightness);
        m_prefs.putUChar("speed", m_speed);
        m_prefs.putUChar("micsens", m_micSensitivity);
        m_prefs.putUShort("cycle_mask", m_cycleMask);

        Serial.printf("[NVS] Delayed write flushed: Power=%s, Effect=%d (%s), Brightness=%d, Speed=%d, MicSens=%d, CycleMask=0x%04X\n",
                      m_powerState ? "ON" : "OFF",
                      m_currentEffectIndex,
                      getCurrentEffectName(),
                      m_targetBrightness,
                      m_speed,
                      m_micSensitivity,
                      m_cycleMask);
    }
}

Effect* EffectManager::getEffect(uint8_t index) const {
    if (index < NUM_CORE_EFFECTS) {
        return m_effects[index];
    }
    return nullptr;
}

const char* EffectManager::getEffectName(uint8_t index) const {
    if (index < NUM_CORE_EFFECTS && m_effects[index]) {
        return m_effects[index]->getName();
    }
    return "Unknown";
}
