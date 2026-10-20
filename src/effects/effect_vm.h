#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include "effect_manager.h"
#include "hal/led_matrix.h"
#include "hal/imu_mpu6050.h"
#include "hal/mic_i2s.h"

/**
 * @file effect_vm.h
 * @brief Dynamic Effect Virtual Machine (EffectVM) for loading and executing .eff descriptors.
 *
 * Supports 6 shader / parametric rendering engines on 16x16 seamless cylinder:
 * 1. aurora (Northern lights with undulating Perlin noise ribbons)
 * 2. matrix_rain (Digital rain code streams)
 * 3. lava_lamp (Thermal wax blob convection)
 * 4. vortex (Cosmic spiral arms wrapping the cylinder)
 * 5. fireworks (Rocket launch, circular burst and gravity particles)
 * 6. cyber_pulse (Synthwave/cyberpunk neon rings with sound/wave reactivity)
 */

enum class VMEngineType : uint8_t {
    UNKNOWN = 0,
    AURORA,
    MATRIX_RAIN,
    LAVA_LAMP,
    VORTEX,
    FIREWORKS,
    CYBER_PULSE
};

class EffectVM : public Effect {
public:
    EffectVM();
    ~EffectVM() override = default;

    /**
     * @brief Parse .eff JSON descriptor and configure effect parameters and palette.
     * @param jsonStr JSON descriptor string.
     * @return true if successfully parsed and engine is valid.
     */
    bool parseDescriptor(const String& jsonStr);

    /**
     * @brief Load .eff file from LittleFS path (e.g. "/effects/aurora.eff").
     * @param filePath Absolute path in LittleFS.
     * @return true if file opened and parsed successfully.
     */
    bool loadFromFile(const char* filePath);

    // Overrides from Effect base class
    void init(LedMatrix& matrix) override;
    void update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) override;
    const char* getName() const override { return m_name.c_str(); }

    const String& getId() const { return m_id; }
    VMEngineType getEngineType() const { return m_engineType; }
    uint8_t getScale() const { return m_scale; }
    uint8_t getIntensity() const { return m_intensity; }

private:
    String m_id;
    String m_name;
    String m_engineName;
    VMEngineType m_engineType;

    uint8_t m_scale;       // Spatial scale (1..255)
    uint8_t m_intensity;   // Brightness/intensity multiplier (1..255)
    CRGBPalette16 m_palette;

    uint32_t m_lastFrameTime;
    float m_phase;

    // --- Engine-specific state buffers ---
    // Matrix Rain
    struct RainDrop {
        float y;
        float speed;
        uint8_t length;
    } m_drops[MATRIX_WIDTH];

    // Lava Lamp Blobs
    struct LavaBlob {
        float y;
        float targetY;
        float speed;
        float radius;
        uint8_t hue;
    } m_blobs[5];

    // Fireworks
    struct FireworkSpark {
        float x;
        float y;
        float vx;
        float vy;
        float life;
        CRGB color;
        bool active;
    };
    static const uint8_t MAX_SPARKS = 36;
    FireworkSpark m_sparks[MAX_SPARKS];
    bool m_rocketActive;
    float m_rocketX;
    float m_rocketY;
    float m_rocketSpeed;
    float m_burstHeight;

    // Cyber Pulse
    float m_pulseRadius;
    float m_pulseSpeed;

    // Helper rendering routines
    void updateAurora(LedMatrix& matrix, float dt);
    void updateMatrixRain(LedMatrix& matrix, float dt);
    void updateLavaLamp(LedMatrix& matrix, float dt);
    void updateVortex(LedMatrix& matrix, float dt);
    void updateFireworks(LedMatrix& matrix, float dt);
    void updateCyberPulse(LedMatrix& matrix, const MicI2S& mic, float dt);

    void setupPalette(const String& paletteName, const String& colorsCsv);
};
