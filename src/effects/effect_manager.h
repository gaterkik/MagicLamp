#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include "hal/led_matrix.h"
#include "hal/mic_i2s.h"

#ifndef TOTAL_EFFECTS
#define TOTAL_EFFECTS 15
#endif

#ifndef NUM_CORE_EFFECTS
#define NUM_CORE_EFFECTS 15
#endif

#ifndef MAX_TOTAL_EFFECTS
#define MAX_TOTAL_EFFECTS 15
#endif

#ifndef DEFAULT_CYCLE_MASK
#define DEFAULT_CYCLE_MASK 0x7FFF
#endif

#ifndef NVS_SAVE_DELAY_MS
#define NVS_SAVE_DELAY_MS 3000
#endif

#ifndef CROSSFADE_DURATION_MS
#define CROSSFADE_DURATION_MS 500
#endif

#ifndef CURTAIN_DURATION_MS
#define CURTAIN_DURATION_MS 600
#endif

/**
 * @file effect_manager.h
 * @brief Base Effect class and EffectManager driver with NVS persistence for MagicLamp.
 *
 * Implements 15 built-in native effects (0..14):
 *  0: Nightlight
 *  1: DawnDusk
 *  2: Fire
 *  3: PerlinNoise
 *  4: GameOfLife
 *  5: LorenzAttractor
 *  6: Metaballs
 *  7: AudioReactive
 *  8: Fireflies
 *  9: Aurora
 * 10: MatrixRain
 * 11: LavaLamp
 * 12: Vortex
 * 13: Fireworks
 * 14: CyberPulse
 *
 * UX features:
 * - 500 ms seamless crossfade buffer blending between effects.
 * - 600 ms curtain wave for lamp power on (0..15) and power off (15..0).
 * - Individual NVS profiles for all 15 effects (brightness, speed, sensitivity, inCycle).
 * - cycle_mask bitmask (uint16_t, default 0x7FFF) for circular button switching.
 * - Melatonin-safe amber spectrum (B = 0) for DawnDusk inDusk mode.
 */

class Effect {
public:
    virtual ~Effect() = default;

    /**
     * @brief Initialize effect state and clear or pre-seed matrix.
     * @param matrix Reference to LedMatrix.
     */
    virtual void init(LedMatrix& matrix) = 0;

    /**
     * @brief Render one frame of the effect (~60 FPS non-blocking).
     * @param matrix Reference to LedMatrix.
     * @param mic Reference to MicI2S driver for sound/music reactivity.
     */
    virtual void update(LedMatrix& matrix, const MicI2S& mic) = 0;

    /**
     * @brief Get human-readable effect name.
     */
    virtual const char* getName() const = 0;

    /**
     * @brief Set animation speed multiplier (1..255, default: 128).
     */
    virtual void setSpeed(uint8_t speed) { m_speed = speed; }

    /**
     * @brief Get animation speed multiplier.
     */
    uint8_t getSpeed() const { return m_speed; }

protected:
    uint8_t m_speed = 128;
};

struct EffectProfile {
    uint8_t brightness = 128;
    uint8_t speed = 128;
    uint8_t sensitivity = 128;
    bool inCycle = true;
    uint8_t param1 = 90; // for Nightlight: colorTemp 0..255 (default 90 ~ 2700K)
};

struct ScheduleConfig {
    bool dawnEnabled = false;
    uint8_t dawnHour = 7;
    uint8_t dawnMinute = 0;
    uint8_t dawnDurationMin = 5; // 5 min wake-up ramp before dawn target time

    bool duskEnabled = false;
    uint8_t duskHour = 23;
    uint8_t duskMinute = 0;
    uint8_t duskDurationMin = 30; // 30 min amber sleep fade to black

    bool sleepNowActive = false;
    uint32_t sleepNowStartMs = 0;
    uint32_t sleepNowDurationMs = 0;

    bool enabled = false; // Backward-compatibility
    bool isEnabled() const { return dawnEnabled || duskEnabled || enabled; }
};

enum class CurtainState : uint8_t {
    OPEN = 0,
    OPENING,
    CLOSING,
    CLOSED
};

class EffectManager {
public:
    EffectManager();
    ~EffectManager();

    /**
     * @brief Initialize NVS preferences and instantiate all 16 built-in effects.
     * Loads saved effect index, brightness, speed, sensitivity, cycle_mask, and power state.
     * @param matrix Reference to LedMatrix driver.
     */
    void begin(LedMatrix& matrix);

    /**
     * @brief Non-blocking animation and NVS persistence update. Call in loop().
     * @param matrix Reference to LedMatrix driver.
     * @param mic Reference to MicI2S driver.
     */
    void update(LedMatrix& matrix, const MicI2S& mic);

    /**
     * @brief Switch to the next enabled effect in circular order (0..14 -> 0) filtering by cycle_mask.
     * @param matrix Reference to LedMatrix driver.
     */
    void nextEffect(LedMatrix& matrix);

    /**
     * @brief Switch to the previous enabled effect in circular order filtering by cycle_mask.
     * @param matrix Reference to LedMatrix driver.
     */
    void prevEffect(LedMatrix& matrix);

    /**
     * @brief Set specific effect by index (0..14) with 500 ms crossfade.
     * @param index Effect index.
     * @param matrix Reference to LedMatrix driver.
     */
    void setEffect(uint8_t index, LedMatrix& matrix);

    /**
     * @brief Toggle lamp power with 600 ms curtain transition.
     */
    void togglePower();

    /**
     * @brief Turn lamp ON with 600 ms rising curtain (0..15).
     */
    void powerOn();

    /**
     * @brief Turn lamp OFF with 600 ms falling curtain (15..0).
     */
    void powerOff();

    /**
     * @brief Set lamp power state directly.
     */
    void setPower(bool on);

    /**
     * @brief Check whether the lamp is currently powered on or turning on.
     */
    bool isPoweredOn() const { return m_powerState; }

    /**
     * @brief Set master brightness level (1..255).
     * @param brightness Target brightness.
     */
    void setBrightness(uint8_t brightness);

    /**
     * @brief Get target master brightness level (1..255).
     */
    uint8_t getBrightness() const { return m_targetBrightness; }

    /**
     * @brief Set global effect animation speed (1..255, default 128).
     */
    void setSpeed(uint8_t speed);

    /**
     * @brief Get global effect animation speed (1..255).
     */
    uint8_t getSpeed() const { return m_speed; }

    /**
     * @brief Set microphone sensitivity (1..255, default 128).
     */
    void setMicSensitivity(uint8_t sensitivity);

    /**
     * @brief Get microphone sensitivity (1..255).
     */
    uint8_t getMicSensitivity() const { return m_micSensitivity; }

    /**
     * @brief Factory reset settings to default in NVS.
     */
    void resetSettings(LedMatrix& matrix);

    /**
     * @brief Perform one step of smooth dimmer adjustment (during button hold).
     * @param matrix Reference to LedMatrix driver.
     */
    void stepDimmer(LedMatrix& matrix);

    /**
     * @brief Invert dimmer direction (called when button release or new press begins).
     */
    void toggleDimmerDirection();

    /**
     * @brief Get index of currently active effect (0..14).
     */
    uint8_t getCurrentEffectIndex() const { return m_currentEffectIndex; }

    /**
     * @brief Get name of currently active effect.
     */
    const char* getCurrentEffectName() const;

    /**
     * @brief Check if there are unsaved settings pending delayed NVS write.
     */
    bool isNvsDirty() const { return m_nvsDirty; }

    /**
     * @brief Get total number of active effects (15).
     */
    uint8_t getTotalEffects() const { return NUM_CORE_EFFECTS; }

    /**
     * @brief Get effect pointer by index.
     */
    Effect* getEffect(uint8_t index) const;

    /**
     * @brief Get effect name by index.
     */
    const char* getEffectName(uint8_t index) const;

    enum class LimitFlashType : uint8_t {
        NONE = 0,
        MAX_LIMIT,
        MIN_LIMIT
    };

    /**
     * @brief Trigger visual limit indication (double flash).
     * @param type MAX_LIMIT for top ring white flash, MIN_LIMIT for bottom ring turquoise flash.
     */
    void triggerLimitFlash(LimitFlashType type);

    /**
     * @brief Get individual profile for effect by index.
     */
    EffectProfile getProfile(uint8_t index) const;

    /**
     * @brief Set individual profile for effect by index.
     */
    void setProfile(uint8_t index, const EffectProfile& prof);

    /**
     * @brief Get button cycle bitmask (15 bits for effects 0..14).
     */
    uint16_t getCycleMask() const { return m_cycleMask; }

    /**
     * @brief Set button cycle bitmask.
     */
    void setCycleMask(uint16_t mask);

    /**
     * @brief Get Dawn/Dusk schedule configuration.
     */
    ScheduleConfig getSchedule() const { return m_schedule; }

    /**
     * @brief Set Dawn/Dusk schedule configuration.
     */
    void setSchedule(const ScheduleConfig& cfg);

    /**
     * @brief Start immediate sleep timer fade out for duskDurationMin minutes.
     */
    void startSleepNow(LedMatrix& matrix);

    /**
     * @brief Stop immediate sleep timer.
     */
    void stopSleepNow();

    /**
     * @brief Check whether "Sleep Now" timer is currently active.
     */
    bool isSleepNowActive() const { return m_sleepNowActive; }

    /**
     * @brief Get human-readable schedule phase status.
     */
    String getScheduleStatus() const;

    uint8_t getDawnDurationMin() const { return m_schedule.dawnDurationMin; }
    uint8_t getDuskDurationMin() const { return m_schedule.duskDurationMin; }
    void setDawnDurationMin(uint8_t min);
    void setDuskDurationMin(uint8_t min);

    /**
     * @brief Thread-safe OTA states for non-blocking matrix rendering.
     */
    enum OtaState : uint8_t {
        OTA_STATE_IDLE = 0,
        OTA_STATE_CHECKING,
        OTA_STATE_DOWNLOADING,
        OTA_STATE_UP_TO_DATE,
        OTA_STATE_SUCCESS,
        OTA_STATE_ERROR
    };

    /**
     * @brief Thread-safe update of OTA state and progress from background tasks.
     */
    void setOtaState(OtaState state, uint8_t progress = 0);

    /**
     * @brief Get current OTA state.
     */
    OtaState getOtaState() const { return m_otaState; }

    /**
     * @brief Get current OTA progress (0..100%).
     */
    uint8_t getOtaProgress() const { return m_otaProgress; }

    /**
     * @brief Get current power curtain state.
     */
    CurtainState getCurtainState() const { return m_curtainState; }

    /**
     * @brief Trigger 2-second non-blocking radio mode switch animation:
     * - isZigbee == true: 2-second convergence of purple-gold rings towards equator (Y=7..8) with central flash.
     * - isZigbee == false: 2-second rotating bright-green beacon on top rings Y=14..15.
     * @param isZigbee True if switching to Zigbee, false for Wi-Fi.
     */
    void triggerRadioModeAnimation(bool isZigbee);

    /**
     * @brief Check whether radio mode animation is currently active.
     */
    bool isRadioAnimActive() const { return m_radioAnimActive; }

    /**
     * @brief Set Zigbee / Alice pairing mode indicator state.
     * When active and lamp is powered on and no other animation is playing,
     * a breathing Alice purple belt pulses at center rings Y=7..8 on dark background.
     * @param active True to enable pairing indicator, false to disable.
     */
    void setZigbeePairingMode(bool active);

    /**
     * @brief Check whether Zigbee pairing mode indicator is active.
     */
    bool isZigbeePairingMode() const { return m_zigbeePairingMode; }

    /**
     * @brief Trigger 1.5-second emerald-green wave across the cylinder upon successful Zigbee connection.
     */
    void triggerSuccessWave();

    /**
     * @brief Check whether success wave animation is active.
     */
    bool isSuccessWaveActive() const { return m_successWaveActive; }

    /**
     * @brief Render radio mode animation frame for given elapsed time.
     */
    void renderRadioModeAnimation(LedMatrix& matrix, uint32_t elapsed);

private:
    void markNvsDirty();
    void updateNvs();
    void markProfilesDirty();
    void updateProfilesNvs();
    void updateSchedule(LedMatrix& matrix);
    void applyCurtain(LedMatrix& matrix);
    void renderLimitFlash(LedMatrix& matrix);
    void renderOta(LedMatrix& matrix);
    void renderZigbeePairing(LedMatrix& matrix);

    volatile OtaState m_otaState;
    volatile uint8_t m_otaProgress;
    uint32_t m_otaStateStartTime;

    Preferences m_prefs;
    bool m_powerState;
    uint8_t m_currentEffectIndex;
    uint8_t m_targetBrightness;
    float m_currentFadeBrightness;

    uint8_t m_speed;
    uint8_t m_micSensitivity;

    EffectProfile m_profiles[NUM_CORE_EFFECTS];
    uint16_t m_cycleMask;
    bool m_profilesDirty;
    uint32_t m_profileSaveTime;

    ScheduleConfig m_schedule;
    uint32_t m_lastScheduleCheckTime;
    bool m_sleepNowActive;
    uint32_t m_sleepNowStartMs;
    uint32_t m_sleepNowDurationMs;

    bool m_dimmerDirUp;
    uint32_t m_lastDimmerStepTime;

    LimitFlashType m_limitFlashType;
    uint32_t m_limitFlashStartTime;

    bool m_radioAnimActive;
    bool m_radioAnimIsZigbee;
    uint32_t m_radioAnimStartMs;
    bool m_zigbeePairingMode;
    bool m_successWaveActive;
    uint32_t m_successWaveStartMs;

    bool m_nvsDirty;
    uint32_t m_nvsDirtyTime;

    uint32_t m_lastFrameTime;
    Effect* m_effects[NUM_CORE_EFFECTS];

    // Crossfade 500 ms state
    CRGB m_prevBuffer[NUM_LEDS];
    bool m_crossfading;
    uint32_t m_crossfadeStartTime;

    // Curtain 600 ms animation state
    CurtainState m_curtainState;
    uint32_t m_curtainStartTime;
};
