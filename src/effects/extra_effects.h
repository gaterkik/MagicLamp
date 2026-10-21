#pragma once

#include "effects/effect_manager.h"
#include "hal/led_matrix.h"
#include "hal/imu_mpu6050.h"
#include "hal/mic_i2s.h"

/**
 * @file extra_effects.h
 * @brief Extra native built-in effects (indices 10..15) for MagicLamp:
 * 10: EffectAurora (Northern lights undulating Perlin ribbons)
 * 11: EffectMatrixRain (Digital rain code streams)
 * 12: EffectLavaLamp (Thermal wax blob convection metaballs)
 * 13: EffectVortex (Cosmic spiral arms wrapping the cylinder)
 * 14: EffectFireworks (Rocket launch, circular burst and gravity particles)
 * 15: EffectCyberPulse (Synthwave neon rings with sound/wave reactivity)
 */

class EffectAurora : public Effect {
public:
    EffectAurora();
    void init(LedMatrix& matrix) override;
    void update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) override;
    const char* getName() const override { return "Aurora"; }

private:
    CRGBPalette16 m_palette;
    uint32_t m_lastFrameTime;
    float m_phase;
};

class EffectMatrixRain : public Effect {
public:
    EffectMatrixRain();
    void init(LedMatrix& matrix) override;
    void update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) override;
    const char* getName() const override { return "MatrixRain"; }

private:
    struct RainDrop {
        float y;
        float speed;
        uint8_t length;
    } m_drops[MATRIX_WIDTH];

    CRGBPalette16 m_palette;
    uint32_t m_lastFrameTime;
};

class EffectLavaLamp : public Effect {
public:
    EffectLavaLamp();
    void init(LedMatrix& matrix) override;
    void update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) override;
    const char* getName() const override { return "LavaLamp"; }

private:
    struct LavaBlob {
        float y;
        float targetY;
        float speed;
        float radius;
        uint8_t hue;
    } m_blobs[5];

    CRGBPalette16 m_palette;
    uint32_t m_lastFrameTime;
    float m_phase;
};

class EffectVortex : public Effect {
public:
    EffectVortex();
    void init(LedMatrix& matrix) override;
    void update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) override;
    const char* getName() const override { return "Vortex"; }

private:
    CRGBPalette16 m_palette;
    uint32_t m_lastFrameTime;
    float m_phase;
};

class EffectFireworks : public Effect {
public:
    EffectFireworks();
    void init(LedMatrix& matrix) override;
    void update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) override;
    const char* getName() const override { return "Fireworks"; }

private:
    struct FireworkSpark {
        float x;
        float y;
        float vx;
        float vy;
        float life;
        CRGB color;
        bool active;
    };
    static constexpr uint8_t MAX_SPARKS = 36;
    FireworkSpark m_sparks[MAX_SPARKS];
    bool m_rocketActive;
    float m_rocketX;
    float m_rocketY;
    float m_rocketSpeed;
    float m_burstHeight;

    CRGBPalette16 m_palette;
    uint32_t m_lastFrameTime;
};

class EffectCyberPulse : public Effect {
public:
    EffectCyberPulse();
    void init(LedMatrix& matrix) override;
    void update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) override;
    const char* getName() const override { return "CyberPulse"; }

    void setSensitivity(uint8_t sens) { m_sensitivity = (sens < 1) ? 1 : sens; }
    uint8_t getSensitivity() const { return m_sensitivity; }

private:
    uint8_t m_sensitivity;
    float m_peakHeight[MATRIX_WIDTH];
    static constexpr uint8_t MAX_SHOCKWAVES = 3;
    float m_shockRadius[MAX_SHOCKWAVES];
    float m_shockAlpha[MAX_SHOCKWAVES];
    CRGBPalette16 m_palette;
    uint32_t m_lastFrameTime;
    float m_phase;
};
