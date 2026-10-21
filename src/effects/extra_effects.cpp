#include "extra_effects.h"
#include <math.h>

// =============================================================================
// 10: EffectAurora (Northern lights undulating Perlin ribbons)
// =============================================================================

EffectAurora::EffectAurora()
    : m_lastFrameTime(0),
      m_phase(0.0f) {
    m_speed = 128;
    // Northern lights green-cyan-purple palette
    m_palette = CRGBPalette16(
        CRGB(0, 40, 20), CRGB(0, 80, 40), CRGB(0, 140, 60), CRGB(0, 180, 80),
        CRGB(0, 220, 110), CRGB(0, 255, 140), CRGB(0, 255, 180), CRGB(0, 200, 220),
        CRGB(0, 160, 255), CRGB(40, 100, 255), CRGB(80, 40, 220), CRGB(120, 0, 180),
        CRGB(80, 0, 180), CRGB(0, 160, 255), CRGB(0, 255, 120), CRGB(0, 40, 20)
    );
}

void EffectAurora::init(LedMatrix& matrix) {
    matrix.clear(false);
    m_lastFrameTime = millis();
    m_phase = 0.0f;
}

void EffectAurora::update(LedMatrix& matrix, const ImuMpu6050& /*imu*/, const MicI2S& /*mic*/) {
    uint32_t now = millis();
    float dt = (now - m_lastFrameTime) / 1000.0f;
    if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;
    m_lastFrameTime = now;

    float speedMult = static_cast<float>(m_speed) / 128.0f;
    m_phase += speedMult * dt * 2.0f;

    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
        float theta = static_cast<float>(x) * (2.0f * M_PI / static_cast<float>(MATRIX_WIDTH));

        float ribbon1 = sinf(theta * 2.0f + m_phase * 1.5f) * 3.5f + 7.5f;
        float ribbon2 = cosf(theta * 3.0f - m_phase * 1.0f) * 2.5f + 9.0f;

        for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
            float d1 = fabsf(static_cast<float>(y) - ribbon1);
            float d2 = fabsf(static_cast<float>(y) - ribbon2);

            float brightness1 = fmaxf(0.0f, 1.0f - d1 * 0.28f);
            float brightness2 = fmaxf(0.0f, 1.0f - d2 * 0.35f);
            float totalB = fminf(1.0f, brightness1 + brightness2);

            if (totalB > 0.05f) {
                uint8_t noiseVal = inoise8(static_cast<uint16_t>(x * 128), static_cast<uint16_t>(y * 128), static_cast<uint16_t>(m_phase * 100.0f));
                uint8_t palIdx = noiseVal + (y * 6) + static_cast<uint8_t>(m_phase * 20.0f);
                uint8_t bri = static_cast<uint8_t>(totalB * 255.0f);
                CRGB color = ColorFromPalette(m_palette, palIdx, bri, LINEARBLEND);
                matrix.setPixel(x, y, color);
            } else {
                matrix.setPixel(x, y, CRGB::Black);
            }
        }
    }
}

// =============================================================================
// 11: EffectMatrixRain (Digital rain code streams)
// =============================================================================

EffectMatrixRain::EffectMatrixRain()
    : m_lastFrameTime(0) {
    m_speed = 128;
    m_palette = CRGBPalette16(
        CRGB(0, 0, 0), CRGB(0, 20, 0), CRGB(0, 40, 0), CRGB(0, 70, 0),
        CRGB(0, 100, 5), CRGB(0, 140, 10), CRGB(0, 180, 15), CRGB(0, 220, 20),
        CRGB(0, 255, 40), CRGB(80, 255, 100), CRGB(180, 255, 200), CRGB(80, 255, 100),
        CRGB(0, 255, 40), CRGB(0, 180, 15), CRGB(0, 90, 5), CRGB(0, 20, 0)
    );
}

void EffectMatrixRain::init(LedMatrix& matrix) {
    matrix.clear(false);
    m_lastFrameTime = millis();
    for (int x = 0; x < MATRIX_WIDTH; x++) {
        m_drops[x].y = static_cast<float>(random(0, MATRIX_HEIGHT * 2));
        m_drops[x].speed = 4.0f + static_cast<float>(random(0, 8));
        m_drops[x].length = 4 + random(0, 5);
    }
}

void EffectMatrixRain::update(LedMatrix& matrix, const ImuMpu6050& /*imu*/, const MicI2S& /*mic*/) {
    uint32_t now = millis();
    float dt = (now - m_lastFrameTime) / 1000.0f;
    if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;
    m_lastFrameTime = now;

    float speedMult = static_cast<float>(m_speed) / 128.0f;

    matrix.fadeToBlackBy(35);

    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
        RainDrop& drop = m_drops[x];
        drop.y -= drop.speed * dt * 2.5f * speedMult;

        if (drop.y < -static_cast<float>(drop.length)) {
            drop.y = static_cast<float>(MATRIX_HEIGHT - 1 + random(1, 6));
            drop.speed = 4.0f + static_cast<float>(random(0, 10));
            drop.length = 3 + random(0, 6);
        }

        int headY = static_cast<int>(roundf(drop.y));
        for (int i = 0; i <= drop.length; i++) {
            int curY = headY + i;
            if (curY >= 0 && curY < MATRIX_HEIGHT) {
                if (i == 0) {
                    matrix.setPixel(x, curY, CRGB(200, 255, 230));
                } else {
                    float tailFrac = 1.0f - (static_cast<float>(i) / static_cast<float>(drop.length));
                    uint8_t bri = static_cast<uint8_t>(tailFrac * 255.0f);
                    uint8_t palIdx = static_cast<uint8_t>(i * 20 + x * 10);
                    CRGB color = ColorFromPalette(m_palette, palIdx, bri, LINEARBLEND);
                    matrix.setPixel(x, curY, color);
                }
            }
        }
    }
}

// =============================================================================
// 12: EffectLavaLamp (Thermal wax blob convection metaballs)
// =============================================================================

EffectLavaLamp::EffectLavaLamp()
    : m_lastFrameTime(0),
      m_phase(0.0f) {
    m_speed = 128;
    m_palette = LavaColors_p;
}

void EffectLavaLamp::init(LedMatrix& matrix) {
    matrix.clear(false);
    m_lastFrameTime = millis();
    m_phase = 0.0f;

    for (int i = 0; i < 5; i++) {
        m_blobs[i].y = static_cast<float>(i * 3 + 1);
        m_blobs[i].targetY = (i % 2 == 0) ? 14.0f : 1.0f;
        m_blobs[i].speed = 1.0f + static_cast<float>(random(0, 15)) * 0.1f;
        m_blobs[i].radius = 1.8f + static_cast<float>(random(0, 10)) * 0.1f;
        m_blobs[i].hue = static_cast<uint8_t>(i * 45);
    }
}

void EffectLavaLamp::update(LedMatrix& matrix, const ImuMpu6050& /*imu*/, const MicI2S& /*mic*/) {
    uint32_t now = millis();
    float dt = (now - m_lastFrameTime) / 1000.0f;
    if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;
    m_lastFrameTime = now;

    float speedMult = static_cast<float>(m_speed) / 128.0f;
    m_phase += speedMult * dt * 2.0f;

    // Move blobs towards targetY with thermal buoyancy
    for (int i = 0; i < 5; i++) {
        LavaBlob& b = m_blobs[i];
        if (b.y < b.targetY) {
            b.y += b.speed * dt * 1.5f * speedMult;
            if (b.y >= b.targetY) {
                b.targetY = 0.5f + static_cast<float>(random(0, 4));
                b.speed = 1.0f + static_cast<float>(random(0, 15)) * 0.1f;
            }
        } else {
            b.y -= b.speed * dt * 1.5f * speedMult;
            if (b.y <= b.targetY) {
                b.targetY = 11.5f + static_cast<float>(random(0, 4));
                b.speed = 1.0f + static_cast<float>(random(0, 15)) * 0.1f;
            }
        }
    }

    // Render metaballs on 16x16 cylinder
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
        for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
            float field = 0.0f;
            for (int i = 0; i < 5; i++) {
                float blobX = static_cast<float>(i) * (16.0f / 5.0f);
                float dx = fabsf(static_cast<float>(x) - blobX);
                if (dx > 8.0f) dx = 16.0f - dx; // Seamless cylinder wrap
                float dy = static_cast<float>(y) - m_blobs[i].y;
                float dist2 = dx * dx + dy * dy;
                field += (m_blobs[i].radius * m_blobs[i].radius) / (dist2 + 0.8f);
            }

            if (field > 0.85f) {
                float norm = fminf(1.0f, (field - 0.85f) * 1.5f);
                uint8_t palIdx = static_cast<uint8_t>(field * 40.0f + m_phase * 10.0f);
                uint8_t bri = static_cast<uint8_t>(norm * 255.0f);
                CRGB color = ColorFromPalette(m_palette, palIdx, bri, LINEARBLEND);
                matrix.setPixel(x, y, color);
            } else {
                matrix.setPixel(x, y, CRGB::Black);
            }
        }
    }
}

// =============================================================================
// 13: EffectVortex (Cosmic spiral arms wrapping the cylinder)
// =============================================================================

EffectVortex::EffectVortex()
    : m_lastFrameTime(0),
      m_phase(0.0f) {
    m_speed = 128;
    m_palette = CRGBPalette16(
        CRGB(10, 0, 30), CRGB(40, 0, 80), CRGB(80, 0, 150), CRGB(0, 50, 200),
        CRGB(0, 120, 255), CRGB(0, 200, 255), CRGB(50, 255, 255), CRGB(200, 220, 255),
        CRGB(255, 255, 255), CRGB(200, 100, 255), CRGB(160, 0, 220), CRGB(100, 0, 180),
        CRGB(60, 0, 120), CRGB(0, 80, 200), CRGB(0, 150, 255), CRGB(10, 0, 30)
    );
}

void EffectVortex::init(LedMatrix& matrix) {
    matrix.clear(false);
    m_lastFrameTime = millis();
    m_phase = 0.0f;
}

void EffectVortex::update(LedMatrix& matrix, const ImuMpu6050& /*imu*/, const MicI2S& /*mic*/) {
    uint32_t now = millis();
    float dt = (now - m_lastFrameTime) / 1000.0f;
    if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;
    m_lastFrameTime = now;

    float speedMult = static_cast<float>(m_speed) / 128.0f;
    m_phase += speedMult * dt * 2.0f;

    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
        float normX = static_cast<float>(x) / static_cast<float>(MATRIX_WIDTH);
        float theta = normX * 2.0f * M_PI;

        for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
            float normY = (static_cast<float>(y) - 7.5f) / 7.5f;

            float twist = 3.2f;
            float spiral = theta * 3.0f + normY * twist * M_PI - m_phase * 2.5f;
            float armVal = (sinf(spiral) + 1.0f) * 0.5f;

            float envelope = fmaxf(0.0f, 1.0f - normY * normY * 0.7f);
            float intensity = armVal * envelope;

            if (intensity > 0.15f) {
                uint8_t palIdx = static_cast<uint8_t>((armVal * 128.0f) + (normY * 40.0f) + (m_phase * 15.0f));
                uint8_t bri = static_cast<uint8_t>(intensity * 255.0f);
                CRGB color = ColorFromPalette(m_palette, palIdx, bri, LINEARBLEND);
                matrix.setPixel(x, y, color);
            } else {
                matrix.setPixel(x, y, CRGB::Black);
            }
        }
    }
}

// =============================================================================
// 14: EffectFireworks (Rocket launch, circular burst and gravity particles)
// =============================================================================

EffectFireworks::EffectFireworks()
    : m_rocketActive(false),
      m_rocketX(8.0f),
      m_rocketY(0.0f),
      m_rocketSpeed(12.0f),
      m_burstHeight(12.0f),
      m_lastFrameTime(0) {
    m_speed = 128;
    m_palette = RainbowColors_p;
    for (int i = 0; i < MAX_SPARKS; i++) {
        m_sparks[i].active = false;
    }
}

void EffectFireworks::init(LedMatrix& matrix) {
    matrix.clear(false);
    m_lastFrameTime = millis();
    m_rocketActive = false;
    for (int i = 0; i < MAX_SPARKS; i++) {
        m_sparks[i].active = false;
    }
}

void EffectFireworks::update(LedMatrix& matrix, const ImuMpu6050& /*imu*/, const MicI2S& /*mic*/) {
    uint32_t now = millis();
    float dt = (now - m_lastFrameTime) / 1000.0f;
    if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;
    m_lastFrameTime = now;

    float speedMult = static_cast<float>(m_speed) / 128.0f;
    matrix.fadeToBlackBy(45);

    if (!m_rocketActive) {
        m_rocketActive = true;
        m_rocketX = static_cast<float>(random(0, MATRIX_WIDTH));
        m_rocketY = 0.0f;
        m_rocketSpeed = (9.0f + static_cast<float>(random(0, 5))) * speedMult;
        m_burstHeight = 9.0f + static_cast<float>(random(0, 6));
    } else {
        m_rocketY += m_rocketSpeed * dt;
        int rx = static_cast<int>(m_rocketX) % MATRIX_WIDTH;
        int ry = static_cast<int>(m_rocketY);
        if (ry < MATRIX_HEIGHT) {
            matrix.setPixel(rx, ry, CRGB(255, 220, 160));
        }

        if (m_rocketY >= m_burstHeight) {
            m_rocketActive = false;
            uint8_t baseHue = random8();

            for (int i = 0; i < MAX_SPARKS; i++) {
                float angle = static_cast<float>(i) * (2.0f * M_PI / static_cast<float>(MAX_SPARKS));
                float speed = (2.5f + static_cast<float>(random(0, 30)) * 0.1f) * speedMult;
                m_sparks[i].x = m_rocketX;
                m_sparks[i].y = m_burstHeight;
                m_sparks[i].vx = cosf(angle) * speed;
                m_sparks[i].vy = sinf(angle) * speed * 0.8f;
                m_sparks[i].life = 1.0f;
                m_sparks[i].color = ColorFromPalette(m_palette, baseHue + random(0, 32), 255);
                m_sparks[i].active = true;
            }
        }
    }

    for (int i = 0; i < MAX_SPARKS; i++) {
        if (!m_sparks[i].active) continue;

        m_sparks[i].x += m_sparks[i].vx * dt;
        m_sparks[i].y += m_sparks[i].vy * dt;
        m_sparks[i].vy -= 4.5f * dt;
        m_sparks[i].life -= dt * 1.2f * speedMult;

        while (m_sparks[i].x < 0.0f) m_sparks[i].x += static_cast<float>(MATRIX_WIDTH);
        while (m_sparks[i].x >= static_cast<float>(MATRIX_WIDTH)) m_sparks[i].x -= static_cast<float>(MATRIX_WIDTH);

        if (m_sparks[i].life <= 0.0f || m_sparks[i].y < 0.0f || m_sparks[i].y >= MATRIX_HEIGHT) {
            m_sparks[i].active = false;
        } else {
            int px = static_cast<int>(m_sparks[i].x);
            int py = static_cast<int>(m_sparks[i].y);
            CRGB col = m_sparks[i].color;
            col.nscale8_video(static_cast<uint8_t>(m_sparks[i].life * 255.0f));
            matrix.setPixel(px, py, col);
        }
    }
}

// =============================================================================
// 15: EffectCyberPulse (Synthwave neon rings with sound/wave reactivity)
// =============================================================================

EffectCyberPulse::EffectCyberPulse()
    : m_sensitivity(128),
      m_lastFrameTime(0),
      m_phase(0.0f) {
    m_speed = 128;
    for (int i = 0; i < MATRIX_WIDTH; i++) {
        m_peakHeight[i] = 0.0f;
    }
    for (int i = 0; i < MAX_SHOCKWAVES; i++) {
        m_shockRadius[i] = -1.0f;
        m_shockAlpha[i] = 0.0f;
    }
    m_palette = CRGBPalette16(
        CRGB(30, 0, 60),     // Deep violet
        CRGB(80, 0, 120),    // Purple
        CRGB(150, 0, 180),   // Neon purple
        CRGB(220, 0, 200),   // Magenta
        CRGB(255, 0, 140),   // Hot pink
        CRGB(255, 40, 180),  // Neon pink
        CRGB(180, 0, 240),   // Electric violet
        CRGB(100, 0, 255),   // Deep blue-violet
        CRGB(0, 80, 255),    // Electric blue
        CRGB(0, 160, 255),   // Neon blue
        CRGB(0, 220, 240),   // Bright cyan
        CRGB(0, 255, 255),   // Electric cyan
        CRGB(180, 240, 255), // White-cyan highlight
        CRGB(0, 220, 240),   // Bright cyan
        CRGB(140, 0, 255),   // Purple
        CRGB(255, 0, 140)    // Hot pink
    );
}

void EffectCyberPulse::init(LedMatrix& matrix) {
    matrix.clear(false);
    m_lastFrameTime = millis();
    m_phase = 0.0f;
    for (int i = 0; i < MATRIX_WIDTH; i++) {
        m_peakHeight[i] = 0.0f;
    }
    for (int i = 0; i < MAX_SHOCKWAVES; i++) {
        m_shockRadius[i] = -1.0f;
        m_shockAlpha[i] = 0.0f;
    }
}

void EffectCyberPulse::update(LedMatrix& matrix, const ImuMpu6050& /*imu*/, const MicI2S& mic) {
    uint32_t now = millis();
    float dt = (now - m_lastFrameTime) / 1000.0f;
    if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;
    m_lastFrameTime = now;

    float speedMult = static_cast<float>(m_speed) / 128.0f;
    float sensMult = static_cast<float>(m_sensitivity) / 128.0f;
    m_phase += speedMult * dt * 2.0f;

    // 1. Beat detection triggers expanding shockwave rings from center (Y=7.5) to edges (Y=0, Y=15)
    bool beat = const_cast<MicI2S&>(mic).isBeat();
    float peak = mic.getPeak() * sensMult;
    float rawRms = mic.getRawRms() * sensMult;

    if (beat || (peak > 0.45f && rawRms > 0.08f)) {
        int slot = 0;
        float minAlpha = m_shockAlpha[0];
        for (int i = 0; i < MAX_SHOCKWAVES; i++) {
            if (m_shockRadius[i] < 0.0f) {
                slot = i;
                break;
            }
            if (m_shockAlpha[i] < minAlpha) {
                minAlpha = m_shockAlpha[i];
                slot = i;
            }
        }
        m_shockRadius[slot] = 0.0f;
        m_shockAlpha[slot] = 1.0f;
    }

    // Step expanding shockwave rings
    for (int i = 0; i < MAX_SHOCKWAVES; i++) {
        if (m_shockRadius[i] >= 0.0f) {
            m_shockRadius[i] += dt * 14.0f * (0.8f + 0.3f * speedMult);
            m_shockAlpha[i] -= dt * 2.2f;
            if (m_shockRadius[i] > 10.0f || m_shockAlpha[i] <= 0.0f) {
                m_shockRadius[i] = -1.0f;
                m_shockAlpha[i] = 0.0f;
            }
        }
    }

    // 2. 16 frequency bands FFT analysis
    float totalBandMag = 0.0f;
    float bandMag[MATRIX_WIDTH];
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
        float raw = mic.getBand(x);
        float scaled = constrain(raw * sensMult, 0.0f, 1.0f);
        bandMag[x] = scaled;
        totalBandMag += scaled;

        float targetHeight = powf(scaled, 0.82f) * 15.9f;
        if (targetHeight > m_peakHeight[x]) {
            m_peakHeight[x] = targetHeight;
        } else {
            m_peakHeight[x] = fmaxf(0.0f, m_peakHeight[x] - dt * 20.0f);
        }
    }
    float avgSound = totalBandMag / static_cast<float>(MATRIX_WIDTH);
    float soundPresence = constrain(avgSound * 8.0f + mic.getNormalizedLevel() * sensMult * 2.0f, 0.0f, 1.0f);

    // 3. Render matrix pixels
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
        float normX = static_cast<float>(x) / static_cast<float>(MATRIX_WIDTH);
        float theta = normX * 2.0f * M_PI;
        float colH = m_peakHeight[x];
        float colMag = bandMag[x];

        for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
            float distFromCenter = fabsf(static_cast<float>(y) - 7.5f);

            // A. Silence background: undulating synthwave cyber-grid (purple / cyan / violet)
            float gridWaveX = (sinf(theta * 3.0f + m_phase) + 1.0f) * 0.5f;
            float gridWaveY = (sinf(static_cast<float>(y) * 0.5f - m_phase * 1.5f) + 1.0f) * 0.5f;
            float gridPattern = gridWaveX * 0.5f + gridWaveY * 0.5f;

            // Horizontal neon scanlines
            float scanline = (sinf(static_cast<float>(y) * 1.2f - m_phase * 3.0f) > 0.7f) ? 0.35f : 0.08f;
            float silenceIntensity = fminf(1.0f, gridPattern * 0.6f + scanline);

            uint8_t silencePalIdx = static_cast<uint8_t>(x * 12 + y * 8 + static_cast<uint8_t>(m_phase * 24.0f));
            CRGB silenceColor = ColorFromPalette(m_palette, silencePalIdx, static_cast<uint8_t>(silenceIntensity * 160.0f), LINEARBLEND);

            // B. Sound columns: 16 FFT columns dynamic height and glow
            CRGB soundColor = CRGB::Black;
            float yF = static_cast<float>(y);
            if (yF <= colH) {
                float hRatio = yF / fmaxf(colH, 1.0f);
                uint8_t colPalIdx = static_cast<uint8_t>(x * 16 + static_cast<uint8_t>(hRatio * 90.0f));
                uint8_t colBri = static_cast<uint8_t>(constrain(static_cast<int>((0.5f + 0.5f * colMag) * 255.0f), 0, 255));
                soundColor = ColorFromPalette(m_palette, colPalIdx, colBri, LINEARBLEND);

                // Peak tip highlight
                if (fabsf(yF - colH) < 1.2f) {
                    soundColor += CRGB(100, 180, 255);
                }
            } else {
                // Dim ambient glow above active column
                float distAbove = yF - colH;
                if (distAbove < 4.0f) {
                    float glow = (1.0f - distAbove * 0.25f) * colMag * 0.4f;
                    soundColor = ColorFromPalette(m_palette, x * 16, static_cast<uint8_t>(glow * 255.0f), LINEARBLEND);
                }
            }

            // Blend between silence grid and reactive columns
            CRGB pixel = blend(silenceColor, soundColor, static_cast<uint8_t>(soundPresence * 255.0f));

            // C. Shockwave rings from center (Y=7.5) to edges (Y=0, Y=15)
            for (int s = 0; s < MAX_SHOCKWAVES; s++) {
                if (m_shockRadius[s] >= 0.0f && m_shockAlpha[s] > 0.01f) {
                    float distToShock = fabsf(distFromCenter - m_shockRadius[s]);
                    if (distToShock < 1.5f) {
                        float ringFactor = (1.0f - distToShock / 1.5f) * m_shockAlpha[s];
                        uint8_t ringBri = static_cast<uint8_t>(ringFactor * 255.0f);
                        CRGB ringColor = ColorFromPalette(m_palette, static_cast<uint8_t>(180 + s * 30), ringBri, LINEARBLEND);
                        ringColor += CRGB(ringBri / 2, ringBri / 2, ringBri); // Punchy electric tint
                        pixel += ringColor;
                    }
                }
            }

            matrix.setPixel(x, y, pixel);
        }
    }
}
