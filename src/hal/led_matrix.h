#pragma once

#include <Arduino.h>
#define FASTLED_INTERNAL
#include <FastLED.h>
#include "config.h"

#ifndef DEFAULT_BRIGHTNESS
#define DEFAULT_BRIGHTNESS 128
#endif

#ifndef MATRIX_TARGET_FPS
#define MATRIX_TARGET_FPS 60
#endif

/**
 * @file led_matrix.h
 * @brief Driver for 16x16 cylindrical WS2812B matrix on ESP32-C6 (GPIO 2).
 * 
 * Hardware characteristics:
 * - 256 WS2812B RGB LEDs connected to GPIO 2 (PIN_LED_DIN).
 * - Driven via hardware RMT peripheral on ESP32-C6.
 * - Strict power limit: 5V / 2500 mA enforced via FastLED power management.
 * - Cylindrical geometry: horizontal coordinate X wraps seamlessly around the cylinder.
 * - Serpentine layout: alternate rows reverse direction (MATRIX_SERPENTINE == 1).
 * - Non-blocking frame generation (~60 FPS target).
 */
class LedMatrix {
public:
    LedMatrix();

    /**
     * @brief Initialize FastLED RMT driver and configure power limits.
     */
    void begin();

    /**
     * @brief Map 2D cylinder coordinates (x, y) to 1D physical LED index.
     * 
     * - Seamless cylinder wrap: x is safely wrapped across [0, MATRIX_WIDTH - 1].
     * - Vertical bounds check: if y < 0 or y >= MATRIX_HEIGHT, returns NUM_LEDS (out-of-bounds).
     * - Serpentine layout: alternates direction on odd rows when MATRIX_SERPENTINE == 1.
     * 
     * @param x Horizontal cylinder coordinate (supports negative and arbitrary values).
     * @param y Vertical coordinate (0 = bottom, MATRIX_HEIGHT - 1 = top).
     * @return Physical LED index in range [0, NUM_LEDS - 1], or NUM_LEDS if out of vertical bounds.
     */
    static inline uint16_t XY(int x, int y) {
        if (y < 0 || y >= MATRIX_HEIGHT) {
            return NUM_LEDS;
        }
        int cx = (x % MATRIX_WIDTH + MATRIX_WIDTH) % MATRIX_WIDTH;
#if MATRIX_SERPENTINE
        if (y & 1) {
            return (uint16_t)(y * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - cx));
        } else {
            return (uint16_t)(y * MATRIX_WIDTH + cx);
        }
#else
        return (uint16_t)(y * MATRIX_WIDTH + cx);
#endif
    }

    /**
     * @brief Set pixel color at (x, y) coordinates with cylinder wrapping and vertical bounds check.
     * @param x Horizontal cylinder coordinate.
     * @param y Vertical coordinate (0..15).
     * @param color CRGB color value.
     */
    void setPixel(int x, int y, const CRGB& color);

    /**
     * @brief Set pixel color using RGB components.
     * @param x Horizontal cylinder coordinate.
     * @param y Vertical coordinate (0..15).
     * @param r Red component (0..255).
     * @param g Green component (0..255).
     * @param b Blue component (0..255).
     */
    void setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Get pixel color at (x, y) coordinates.
     * @return CRGB color value, or CRGB::Black if y is out of bounds.
     */
    CRGB getPixel(int x, int y) const;

    /**
     * @brief Set pixel by raw physical LED index (0..255).
     * @param index Physical LED index.
     * @param color CRGB color value.
     */
    void setPixelRaw(uint16_t index, const CRGB& color);

    /**
     * @brief Get pixel by raw physical LED index (0..255).
     * @param index Physical LED index.
     * @return CRGB color value, or CRGB::Black if index >= NUM_LEDS.
     */
    CRGB getPixelRaw(uint16_t index) const;

    /**
     * @brief Fill entire matrix with a single color.
     * @param color CRGB color value.
     */
    void fill(const CRGB& color);

    /**
     * @brief Clear matrix (set all LEDs to Black).
     * @param showAfter If true, immediately pushes update to physical LEDs.
     */
    void clear(bool showAfter = false);

    /**
     * @brief Dim all pixels in the matrix by a given factor.
     * @param factor Dimming factor (0 = unchanged, 255 = completely black).
     */
    void fadeToBlackBy(uint8_t factor);

    /**
     * @brief Output current pixel buffer to LEDs via hardware RMT.
     */
    void show();

    /**
     * @brief Set master brightness level (0..255).
     * @param brightness Brightness value.
     */
    void setBrightness(uint8_t brightness);

    /**
     * @brief Get current master brightness level.
     * @return uint8_t Brightness value.
     */
    uint8_t getBrightness() const;

    /**
     * @brief Enable or disable the running wave test effect.
     * If disabled, turns off all LEDs immediately.
     * @param enabled True to enable, false to disable.
     */
    void setTestEffectEnabled(bool enabled);

    /**
     * @brief Check if test wave effect is currently active.
     * @return true if enabled, false otherwise.
     */
    bool isTestEffectEnabled() const;

    /**
     * @brief Toggle test wave effect state (ON <-> OFF).
     */
    void toggleTestEffect();

    /**
     * @brief Non-blocking frame update function.
     * Must be called in loop(). Drives ~60 FPS animation when test effect is active.
     */
    void update();

    /**
     * @brief Access internal LED array pointer.
     * @return CRGB* Pointer to LED array (length NUM_LEDS + 1).
     */
    CRGB* getLeds();
    const CRGB* getLeds() const;

private:
    CRGB m_leds[NUM_LEDS + 1];  // Guard pixel at NUM_LEDS absorbs out-of-bounds writes
    uint8_t m_brightness;
    bool m_testEffectEnabled;
    uint32_t m_lastFrameTime;

    void renderTestWave();
};

/**
 * @brief Global convenience function for XY coordinate mapping.
 */
inline uint16_t XY(int x, int y) {
    return LedMatrix::XY(x, y);
}
