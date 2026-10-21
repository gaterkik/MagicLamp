#pragma once

#include "effect_manager.h"

/**
 * @file core_effects.h
 * @brief Atmospheric core effects for MagicLamp (ESP32-C6):
 * 1. EffectNightlight (Soft warm incandescent light with subtle pulse)
 * 2. EffectDawnDusk (Realistic diurnal sun cycle bottom-up with GMT+2 support)
 * 3. EffectFire (Campfire rising from Y=0 to Y=15 with cylinder ring wrap)
 * 4. EffectPerlinNoise (Seamless 3D Perlin noise wrapped around cylinder)
 */

class EffectNightlight : public Effect {
public:
    void init(LedMatrix& matrix) override;
    void update(LedMatrix& matrix, const MicI2S& mic) override;
    const char* getName() const override { return "Nightlight"; }
    void setColorTemp(uint8_t temp) { m_colorTemp = temp; }
    uint8_t getColorTemp() const { return m_colorTemp; }

private:
    uint8_t m_colorTemp = 90;
};

class EffectDawnDusk : public Effect {
public:
    void init(LedMatrix& matrix) override;
    void update(LedMatrix& matrix, const MicI2S& mic) override;
    const char* getName() const override { return "DawnDusk"; }
    void setSchedule(const ScheduleConfig* schedule) { m_schedule = schedule; }

private:
    uint32_t m_cycleStartTime = 0;
    const ScheduleConfig* m_schedule = nullptr;
};

class EffectFire : public Effect {
public:
    void init(LedMatrix& matrix) override;
    void update(LedMatrix& matrix, const MicI2S& mic) override;
    const char* getName() const override { return "Fire"; }

private:
    void stepSimulation();

    static constexpr uint8_t MAX_FIRE_SPARKS = 2;

    struct FireSpark {
        bool active = false;
        float x = 0.0f;
        float y = 0.0f;
        float vx = 0.0f;
        float vy = 0.0f;
        float brightness = 0.0f;
        float decay = 0.0f;
        CRGB color = CRGB::Black;
    };

    uint8_t m_currentHeat[MATRIX_HEIGHT][MATRIX_WIDTH];
    uint8_t m_prevHeat[MATRIX_HEIGHT][MATRIX_WIDTH];
    FireSpark m_sparks[MAX_FIRE_SPARKS];
    uint32_t m_lastStepTime = 0;
};

class EffectPerlinNoise : public Effect {
public:
    void init(LedMatrix& matrix) override;
    void update(LedMatrix& matrix, const MicI2S& mic) override;
    const char* getName() const override { return "PerlinNoise"; }

private:
    uint16_t m_xCoords[MATRIX_WIDTH];
    uint16_t m_yCoords[MATRIX_WIDTH];
    float m_zAccum = 0.0f;
    float m_colorAccum = 0.0f;
    uint32_t m_lastPerlinTime = 0;
};

class EffectFireflies : public Effect {
public:
    void init(LedMatrix& matrix) override;
    void update(LedMatrix& matrix, const MicI2S& mic) override;
    const char* getName() const override { return "Fireflies"; }

private:
    static constexpr uint8_t NUM_FIREFLIES = 15;

    struct Firefly {
        float x;
        float y;
        float vx;
        float vy;
        float phase;
        float phaseSpeed;
        CRGB color;
    };

    Firefly m_fireflies[NUM_FIREFLIES];
    uint32_t m_lastUpdateTime = 0;
};
