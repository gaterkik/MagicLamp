#include "led_matrix.h"
#include <string.h>

LedMatrix::LedMatrix()
    : m_brightness(DEFAULT_BRIGHTNESS),
      m_testEffectEnabled(false),
      m_lastFrameTime(0) {
    memset(m_leds, 0, sizeof(m_leds));
}

void LedMatrix::begin() {
    // Initialize WS2812B on GPIO 2 (hardware RMT on ESP32-C6)
    FastLED.addLeds<WS2812B, PIN_LED_DIN, GRB>(m_leds, NUM_LEDS);

    // Strict software current limit: 5V / 2500 mA (ARCHITECTURE.md)
    FastLED.setMaxPowerInVoltsAndMilliamps(POWER_SUPPLY_VOLTS, CURRENT_LIMIT_MA);

    // Set initial brightness
    FastLED.setBrightness(m_brightness);

    // Clear LEDs on startup
    clear(true);
}

void LedMatrix::setPixel(int x, int y, const CRGB& color) {
    if (y < 0 || y >= MATRIX_HEIGHT) {
        return;
    }
    m_leds[XY(x, y)] = color;
}

void LedMatrix::setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    setPixel(x, y, CRGB(r, g, b));
}

CRGB LedMatrix::getPixel(int x, int y) const {
    if (y < 0 || y >= MATRIX_HEIGHT) {
        return CRGB::Black;
    }
    return m_leds[XY(x, y)];
}

void LedMatrix::setPixelRaw(uint16_t index, const CRGB& color) {
    if (index < NUM_LEDS) {
        m_leds[index] = color;
    }
}

CRGB LedMatrix::getPixelRaw(uint16_t index) const {
    if (index < NUM_LEDS) {
        return m_leds[index];
    }
    return CRGB::Black;
}

void LedMatrix::fill(const CRGB& color) {
    fill_solid(m_leds, NUM_LEDS, color);
    m_leds[NUM_LEDS] = CRGB::Black;
}

void LedMatrix::clear(bool showAfter) {
    memset(m_leds, 0, sizeof(m_leds));
    if (showAfter) {
        show();
    }
}

void LedMatrix::fadeToBlackBy(uint8_t factor) {
    ::fadeToBlackBy(m_leds, NUM_LEDS, factor);
    m_leds[NUM_LEDS] = CRGB::Black;
}

void LedMatrix::show() {
    FastLED.show();
}

void LedMatrix::setBrightness(uint8_t brightness) {
    m_brightness = brightness;
    FastLED.setBrightness(m_brightness);
}

uint8_t LedMatrix::getBrightness() const {
    return m_brightness;
}

void LedMatrix::setTestEffectEnabled(bool enabled) {
    if (m_testEffectEnabled != enabled) {
        m_testEffectEnabled = enabled;
        if (!m_testEffectEnabled) {
            clear(true);
        }
    }
}

bool LedMatrix::isTestEffectEnabled() const {
    return m_testEffectEnabled;
}

void LedMatrix::toggleTestEffect() {
    setTestEffectEnabled(!m_testEffectEnabled);
}

void LedMatrix::update() {
    if (!m_testEffectEnabled) {
        return;
    }

    uint32_t now = millis();
    constexpr uint32_t frameIntervalMs = 1000 / MATRIX_TARGET_FPS;
    if (now - m_lastFrameTime >= frameIntervalMs) {
        m_lastFrameTime = now;
        renderTestWave();
        show();
    }
}

void LedMatrix::renderTestWave() {
    uint32_t now = millis();
    // Smooth wave rotating around cylindrical matrix:
    // Cylindrical angle: x mapped to 0..255 (x * 256 / MATRIX_WIDTH)
    // Speed: complete rotation in ~1.5 seconds (now / 6)
    // Diagonal wave phase: adds y offset for a rotating helix
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            uint8_t angle = (x * 256) / MATRIX_WIDTH;
            uint8_t wavePhase = angle + (y * 10) + static_cast<uint8_t>(now / 6);
            uint8_t waveVal = sin8(wavePhase);
            uint8_t hue = static_cast<uint8_t>(now / 16) + angle + (y * 6);
            setPixel(x, y, CHSV(hue, 230, waveVal));
        }
    }
}

CRGB* LedMatrix::getLeds() {
    return m_leds;
}

const CRGB* LedMatrix::getLeds() const {
    return m_leds;
}

void LedMatrix::showOtaProgress(uint8_t percent, bool success, bool error) {
    if (error) {
        // 3 soft red pulses
        for (int p = 0; p < 3; p++) {
            for (int b = 0; b <= 180; b += 12) {
                fill(CRGB(b, 0, 0));
                show();
                delay(12);
            }
            for (int b = 180; b >= 0; b -= 12) {
                fill(CRGB(b, 0, 0));
                show();
                delay(12);
            }
        }
        clear(true);
        return;
    }

    if (success) {
        // 1.2-second emerald-green wave
        uint32_t start = millis();
        while (millis() - start < 1200) {
            uint32_t t = millis() - start;
            float phase = static_cast<float>(t) / 1200.0f; // 0..1
            float waveCenter = phase * 18.0f; // bottom to top
            for (int y = 0; y < MATRIX_HEIGHT; y++) {
                float dist = fabsf(static_cast<float>(y) - waveCenter);
                float bright = fmaxf(0.12f, 1.0f - dist * 0.28f);
                CRGB col = CRGB(0, static_cast<uint8_t>(bright * 255), static_cast<uint8_t>(bright * 120));
                for (int x = 0; x < MATRIX_WIDTH; x++) {
                    setPixel(x, y, col);
                }
            }
            show();
            delay(16);
        }
        clear(true);
        return;
    }

    // 0..100% cylinder bottom-up filling with breathing pulsating edge
    if (percent > 100) percent = 100;
    float fillHeight = (static_cast<float>(percent) / 100.0f) * static_cast<float>(MATRIX_HEIGHT);
    uint8_t pulse = sin8(static_cast<uint8_t>(millis() / 4));
    float pulseFactor = 0.5f + 0.5f * (static_cast<float>(pulse) / 255.0f);

    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        float fy = static_cast<float>(y);
        CRGB rowColor = CRGB::Black;
        if (fy < fillHeight - 1.0f) {
            // Solid turquoise/cyan-blue
            rowColor = CRGB(0, 140, 240);
        } else if (fy <= fillHeight) {
            // Pulsing edge
            float edgeAlpha = fillHeight - fy;
            rowColor = CRGB(0, static_cast<uint8_t>(180.0f * pulseFactor * edgeAlpha), static_cast<uint8_t>(255.0f * pulseFactor * edgeAlpha));
        } else {
            rowColor = CRGB::Black;
        }

        for (int x = 0; x < MATRIX_WIDTH; x++) {
            setPixel(x, y, rowColor);
        }
    }
    show();
}
