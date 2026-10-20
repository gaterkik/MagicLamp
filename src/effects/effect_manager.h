#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include "hal/led_matrix.h"
#include "hal/imu_mpu6050.h"
#include "hal/mic_i2s.h"

#ifndef NUM_CORE_EFFECTS
#define NUM_CORE_EFFECTS 10
#endif

#ifndef MAX_TOTAL_EFFECTS
#define MAX_TOTAL_EFFECTS 24
#endif


#ifndef NVS_SAVE_DELAY_MS
#define NVS_SAVE_DELAY_MS 3000
#endif

#ifndef FADE_DURATION_MS
#define FADE_DURATION_MS 350
#endif

/**
 * @file effect_manager.h
 * @brief Base Effect class and EffectManager driver with NVS persistence for MagicLamp.
 *
 * Implements:
 * - Base class `Effect` with `init()` and `update()` methods.
 * - 9 core effect slots:
 *     0: Nightlight
 *     1: DawnDusk
 *     2: Fire
 *     3: PerlinNoise
 *     4: GameOfLife
 *     5: LorenzAttractor
 *     6: Metaballs
 *     7: SandHourglass
 *     8: AudioReactive
 * - Smooth power toggle with fade in / fade out.
 * - Circular effect switching.
 * - Smooth brightness dimmer with direction reversal.
 * - Delayed NVS persistence via Preferences (>= 3000 ms debounce, zero flash wear).
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
     * @param imu Reference to ImuMpu6050 driver for physical orientation/shake.
     * @param mic Reference to MicI2S driver for sound/music reactivity.
     */
    virtual void update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) = 0;

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
};

struct ScheduleConfig {
    bool enabled = false;
    uint8_t dawnHour = 7;
    uint8_t dawnMinute = 0;
    uint8_t dawnDurationMin = 10; // 5-15 min (default: 10)
    uint8_t duskHour = 23;
    uint8_t duskMinute = 0;
    uint8_t duskDurationMin = 30; // 30 min
};

class EffectManager {
public:
    EffectManager();
    ~EffectManager();

    /**
     * @brief Initialize NVS preferences and instantiate all 10 core effects.
     * Loads saved effect index, brightness, speed, sensitivity, and power state.
     * @param matrix Reference to LedMatrix driver.
     */
    void begin(LedMatrix& matrix);

    /**
     * @brief Non-blocking animation and NVS persistence update. Call in loop().
     * @param matrix Reference to LedMatrix driver.
     * @param imu Reference to ImuMpu6050 driver.
     * @param mic Reference to MicI2S driver.
     */
    void update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic);

    /**
     * @brief Switch to the next effect in circular order (0..9 -> 0).
     * @param matrix Reference to LedMatrix driver.
     */
    void nextEffect(LedMatrix& matrix);

    /**
     * @brief Switch to the previous effect in circular order.
     * @param matrix Reference to LedMatrix driver.
     */
    void prevEffect(LedMatrix& matrix);

    /**
     * @brief Set specific effect by index (0..9).
     * @param index Effect index.
     * @param matrix Reference to LedMatrix driver.
     */
    void setEffect(uint8_t index, LedMatrix& matrix);

    /**
     * @brief Toggle lamp power with smooth fade transition.
     */
    void togglePower();

    /**
     * @brief Turn lamp ON with smooth fade.
     */
    void powerOn();

    /**
     * @brief Turn lamp OFF with smooth fade.
     */
    void powerOff();

    /**
     * @brief Check whether the lamp is currently powered on.
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
     * @brief Get index of currently active effect (0..9).
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
     * @brief Register a dynamic custom effect (e.g. from LittleFS .eff).
     * @param effect Pointer to allocated Effect instance (ownership transferred to EffectManager).
     * @return true if registered, false if MAX_TOTAL_EFFECTS reached.
     */
    bool registerCustomEffect(Effect* effect);

    /**
     * @brief Remove and delete all dynamically loaded custom effects.
     */
    void clearCustomEffects();

    /**
     * @brief Get total number of active effects (core 10 + installed custom).
     */
    uint8_t getTotalEffects() const { return m_totalEffects; }

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
     * @brief Get Dawn/Dusk schedule configuration.
     */
    ScheduleConfig getSchedule() const { return m_schedule; }

    /**
     * @brief Set Dawn/Dusk schedule configuration.
     */
    void setSchedule(const ScheduleConfig& cfg);

private:
    void markNvsDirty();
    void updateNvs();
    void markProfilesDirty();
    void updateProfilesNvs();
    void updateSchedule(LedMatrix& matrix);
    void updateFade(LedMatrix& matrix);
    void renderLimitFlash(LedMatrix& matrix);

    Preferences m_prefs;
    bool m_powerState;
    uint8_t m_currentEffectIndex;
    uint8_t m_targetBrightness;
    float m_currentFadeBrightness;

    uint8_t m_speed;
    uint8_t m_micSensitivity;

    EffectProfile m_profiles[MAX_TOTAL_EFFECTS];
    bool m_profilesDirty;
    uint32_t m_profileSaveTime;

    ScheduleConfig m_schedule;
    uint32_t m_lastScheduleCheckTime;

    bool m_dimmerDirUp;
    uint32_t m_lastDimmerStepTime;

    LimitFlashType m_limitFlashType;
    uint32_t m_limitFlashStartTime;

    bool m_nvsDirty;
    uint32_t m_nvsDirtyTime;

    uint32_t m_lastFrameTime;
    uint8_t m_totalEffects;
    Effect* m_effects[MAX_TOTAL_EFFECTS];
};

