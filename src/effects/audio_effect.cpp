#include "audio_effect.h"
#include <math.h>

void EffectAudioReactive::init(LedMatrix& matrix) {
    for (int x = 0; x < MATRIX_WIDTH; x++) {
        m_peakY[x] = 0.0f;
    }
    m_beatFlash = 0;
    m_rippleY = -1.0f;
    matrix.clear(false);
}

void EffectAudioReactive::update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) {
    // Check if bass beat detector fired
    bool beat = const_cast<MicI2S&>(mic).isBeat();
    if (beat) {
        m_beatFlash = 255;
        m_rippleY = 0.0f; // launch bass energy wave starting from Y=0
    }

    // Step expanding beat wave
    if (m_rippleY >= 0.0f) {
        m_rippleY += 0.85f;
        if (m_rippleY >= static_cast<float>(MATRIX_HEIGHT)) {
            m_rippleY = -1.0f;
        }
    }

    // Fast decay of beat flash
    if (m_beatFlash > 12) {
        m_beatFlash -= 12;
    } else {
        m_beatFlash = 0;
    }

    // Render 16 frequency columns (X = 0..15)
    for (int x = 0; x < MATRIX_WIDTH; x++) {
        float bandMag = mic.getBand(x); // 0.0f .. 1.0f
        float targetHeight = bandMag * static_cast<float>(MATRIX_HEIGHT);

        // Peak drop ballistics
        if (targetHeight > m_peakY[x]) {
            m_peakY[x] = targetHeight;
        } else {
            m_peakY[x] = fmaxf(0.0f, m_peakY[x] - 0.42f);
        }

        int colHeight = static_cast<int>(roundf(targetHeight));
        int peakHeight = static_cast<int>(roundf(m_peakY[x]));

        for (int y = 0; y < MATRIX_HEIGHT; y++) {
            CRGB pixelColor = CRGB::Black;

            if (y < colHeight) {
                // Spectral color gradient: Green (Y=0) -> Yellow (Y=7) -> Red (Y=15)
                uint8_t hue = map(y, 0, MATRIX_HEIGHT - 1, 96, 0); // 96=Green, 64=Yellow, 0=Red
                uint8_t bri = 240;
                pixelColor = CHSV(hue, 240, bri);
            } else if (y == peakHeight && peakHeight > 0) {
                // Bright peak dot
                pixelColor = CRGB(255, 255, 220);
            }

            // Overlay beat flash and ripple
            if (m_beatFlash > 0 && y <= colHeight) {
                pixelColor += CRGB(m_beatFlash / 4, m_beatFlash / 5, m_beatFlash / 3);
            }

            // Expanding bass ripple ring
            if (m_rippleY >= 0.0f) {
                float dist = fabsf(static_cast<float>(y) - m_rippleY);
                if (dist < 1.2f) {
                    uint8_t rippleBri = static_cast<uint8_t>((1.2f - dist) * 160.0f);
                    pixelColor += CRGB(rippleBri, rippleBri / 2, rippleBri);
                }
            }

            matrix.setPixel(x, y, pixelColor);
        }
    }
}
