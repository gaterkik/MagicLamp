#include "core_effects.h"
#include <math.h>
#include <time.h>

static inline int wrapX(int x) {
    return (x % MATRIX_WIDTH + MATRIX_WIDTH) % MATRIX_WIDTH;
}

// =============================================================================
// 0: EffectNightlight (Soft, warm incandescent glow with subtle breathing)
// =============================================================================

void EffectNightlight::init(LedMatrix& matrix) {
    matrix.clear(false);
}

void EffectNightlight::update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) {
    uint32_t now = millis();
    // Gentle breathing modulation (~4 second cycle)
    uint8_t breath = sin8(static_cast<uint8_t>(now / 16));
    uint8_t briScale = map(breath, 0, 255, 215, 255);

    // Warm candle / 2700K incandescent base
    CRGB baseWarm = CRGB(255, 147, 41);
    baseWarm.nscale8_video(briScale);

    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        // Subtle vertical gradient (slightly warmer/richer at bottom Y=0)
        uint8_t rowDim = 255 - (y * 4);
        CRGB col = baseWarm;
        col.nscale8_video(rowDim);

        for (int x = 0; x < MATRIX_WIDTH; x++) {
            matrix.setPixel(x, y, col);
        }
    }
}

// =============================================================================
// 1: EffectDawnDusk (Vertical dawn/dusk sky gradient Y=0..15 with GMT+2 support)
// =============================================================================

void EffectDawnDusk::init(LedMatrix& matrix) {
    m_cycleStartTime = millis();
    matrix.clear(false);
}

void EffectDawnDusk::update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) {
    uint32_t now = millis();
    float cyclePhase = 0.0f; // 0.0 .. 1.0 (0=Midnight, 0.25=Dawn, 0.5=Noon, 0.75=Dusk)

    time_t rawtime;
    time(&rawtime);
    if (rawtime > 100000) {
        // Valid system time present: apply GMT+2 offset (+7200 seconds)
        time_t localTime = rawtime + TIMEZONE_OFFSET_SEC;
        struct tm* timeinfo = gmtime(&localTime);
        if (timeinfo) {
            uint32_t secOfDay = timeinfo->tm_hour * 3600 + timeinfo->tm_min * 60 + timeinfo->tm_sec;
            cyclePhase = static_cast<float>(secOfDay) / 86400.0f;
        }
    } else {
        // Standalone demonstration: smooth 60-second full diurnal cycle
        constexpr uint32_t demoCycleDurationMs = 60000;
        cyclePhase = static_cast<float>((now - m_cycleStartTime) % demoCycleDurationMs) / static_cast<float>(demoCycleDurationMs);
    }

    // Solar altitude: -1.0 (Midnight) .. 0.0 (Dawn/Dusk horizon) .. +1.0 (Noon)
    float sunAltitude = -cosf(cyclePhase * 6.2831853f);

    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        float relY = static_cast<float>(y) / static_cast<float>(MATRIX_HEIGHT - 1); // 0.0 at bottom, 1.0 at top
        CRGB skyColor;

        if (sunAltitude < -0.15f) {
            // Night sky: deep navy with soft bottom horizon glow
            float nightDarkness = fminf(1.0f, (-sunAltitude - 0.15f) / 0.85f);
            uint8_t blueVal = static_cast<uint8_t>(fmaxf(10.0f, 40.0f * (1.0f - relY * 0.7f) * nightDarkness));
            skyColor = CRGB(2, 4, blueVal);
        } else if (sunAltitude >= -0.15f && sunAltitude < 0.25f) {
            // Dawn / Sunrise: starts from bottom Y=0 (deep crimson/orange) expanding to gold/sky blue
            float dawnProgress = (sunAltitude + 0.15f) / 0.40f; // 0..1
            float horizonDist = fabsf(relY - dawnProgress * 0.8f);
            uint8_t warmIntensity = static_cast<uint8_t>(fmaxf(0.0f, (1.0f - horizonDist * 1.5f) * 255.0f));

            CRGB horizonWarm = blend(CRGB(180, 20, 10), CRGB(255, 140, 20), static_cast<uint8_t>(dawnProgress * 255.0f));
            CRGB upperSky = blend(CRGB(10, 15, 60), CRGB(80, 140, 220), static_cast<uint8_t>(dawnProgress * 255.0f));

            skyColor = blend(upperSky, horizonWarm, warmIntensity);
        } else {
            // Daytime / Zenith: bright golden warmth at top, clear sky
            float dayProgress = (sunAltitude - 0.25f) / 0.75f; // 0..1
            CRGB sunZenith = CRGB(255, 230, 160);
            CRGB blueAtmosphere = CRGB(100, 170, 255);
            skyColor = blend(blueAtmosphere, sunZenith, static_cast<uint8_t>(relY * dayProgress * 255.0f));
        }

        for (int x = 0; x < MATRIX_WIDTH; x++) {
            matrix.setPixel(x, y, skyColor);
        }
    }
}

// =============================================================================
// 2: EffectFire (Campfire 3/4 height Y=0..10 with detached sparks flying Y=11..15)
// =============================================================================

void EffectFire::init(LedMatrix& matrix) {
    memset(m_currentHeat, 0, sizeof(m_currentHeat));
    memset(m_prevHeat, 0, sizeof(m_prevHeat));
    for (int i = 0; i < MAX_FIRE_SPARKS; i++) {
        m_sparks[i].active = false;
    }
    m_lastStepTime = millis();
    matrix.clear(false);
}

void EffectFire::stepSimulation() {
    // Copy current state to prevHeat for smooth interpolation
    memcpy(m_prevHeat, m_currentHeat, sizeof(m_currentHeat));

    // Step 1: Cool down cells for Y = 0..10 (progressive cooling extinguishing flame before Y=11)
    for (int y = 0; y <= 10; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            uint8_t cool = random8(8, 16) + (y * 2);
            if (m_currentHeat[y][x] > cool) {
                m_currentHeat[y][x] -= cool;
            } else {
                m_currentHeat[y][x] = 0;
            }
        }
    }

    // Step 2: Heat diffuses UPWARD within Y=0..10 and wraps around cylinder X seamlessly
    for (int y = 10; y >= 1; y--) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            int left = wrapX(x - 1);
            int right = wrapX(x + 1);
            // 3-point convolution with ring wrap on X
            uint16_t sum = m_currentHeat[y - 1][left] + m_currentHeat[y - 1][x] * 2 + m_currentHeat[y - 1][right];
            uint8_t diffused = static_cast<uint8_t>(sum / 4);
            // Viscous flame blending
            m_currentHeat[y][x] = static_cast<uint8_t>((diffused * 3 + m_currentHeat[y][x] * 2) / 5);
        }
    }

    // Upper zone Y = 11..15 is strictly dark
    for (int y = 11; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            m_currentHeat[y][x] = 0;
            m_prevHeat[y][x] = 0;
        }
    }

    // Step 3: Randomly ignite new heat at bottom row Y=0
    for (int x = 0; x < MATRIX_WIDTH; x++) {
        if (random8(100) < 55) {
            uint8_t spark = random8(160, 255);
            m_currentHeat[0][x] = static_cast<uint8_t>((m_currentHeat[0][x] + spark) / 2);
        }
    }

    // Step 4: Sporadically detach bright airborne sparks from flame tips (Y = 8..10)
    if (random8(100) < 30) {
        for (int i = 0; i < MAX_FIRE_SPARKS; i++) {
            if (!m_sparks[i].active) {
                int sparkCol = random8(MATRIX_WIDTH);
                m_sparks[i].active = true;
                m_sparks[i].x = static_cast<float>(sparkCol);
                m_sparks[i].y = 8.5f + (static_cast<float>(random8(20)) / 10.0f); // 8.5..10.5
                m_sparks[i].vx = (static_cast<float>(random8(100)) - 50.0f) / 300.0f; // -0.16..+0.16 drift
                m_sparks[i].vy = 0.18f + (static_cast<float>(random8(60)) / 250.0f);  // upward flight 0.18..0.42
                m_sparks[i].brightness = 255.0f;
                m_sparks[i].decay = 5.0f + (static_cast<float>(random8(35)) / 10.0f); // decay 5.0..8.5
                // Color: bright golden-yellow / amber white
                m_sparks[i].color = CRGB(255, random8(180, 250), random8(40, 120));
                break;
            }
        }
    }
}

void EffectFire::update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) {
    uint32_t now = millis();
    // Simulation step interval scaled by m_speed (default 128 -> ~50ms, 3x slower than 16ms)
    uint32_t stepInterval = map(m_speed, 1, 255, 120, 25);
    if (stepInterval < 10) stepInterval = 10;

    if (now - m_lastStepTime >= stepInterval) {
        stepSimulation();
        m_lastStepTime = now;
    }

    // Smooth linear interpolation between m_prevHeat and m_currentHeat at 60 FPS
    float fraction = static_cast<float>(now - m_lastStepTime) / static_cast<float>(stepInterval);
    if (fraction > 1.0f) fraction = 1.0f;
    if (fraction < 0.0f) fraction = 0.0f;

    // Render interpolated heat map: body Y=0..10, dark upper zone Y=11..15
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            if (y > 10) {
                matrix.setPixel(x, y, CRGB::Black);
            } else {
                float h0 = static_cast<float>(m_prevHeat[y][x]);
                float h1 = static_cast<float>(m_currentHeat[y][x]);
                uint8_t heatVal = static_cast<uint8_t>(h0 + (h1 - h0) * fraction);
                matrix.setPixel(x, y, HeatColor(heatVal));
            }
        }
    }

    // Render flying sparks through the upper dark zone (Y=11..15)
    for (int i = 0; i < MAX_FIRE_SPARKS; i++) {
        if (!m_sparks[i].active) continue;

        m_sparks[i].x += m_sparks[i].vx;
        if (m_sparks[i].x < 0.0f) m_sparks[i].x += MATRIX_WIDTH;
        if (m_sparks[i].x >= MATRIX_WIDTH) m_sparks[i].x -= MATRIX_WIDTH;

        m_sparks[i].y += m_sparks[i].vy;
        m_sparks[i].brightness -= m_sparks[i].decay;

        if (m_sparks[i].y >= MATRIX_HEIGHT || m_sparks[i].brightness <= 0.0f) {
            m_sparks[i].active = false;
            continue;
        }

        uint8_t bri = static_cast<uint8_t>(fmaxf(0.0f, fminf(255.0f, m_sparks[i].brightness)));
        CRGB sparkColor = m_sparks[i].color;
        sparkColor.nscale8_video(bri);

        int px = static_cast<int>(floorf(m_sparks[i].x)) % MATRIX_WIDTH;
        if (px < 0) px += MATRIX_WIDTH;
        int py = static_cast<int>(floorf(m_sparks[i].y));

        if (py >= 0 && py < MATRIX_HEIGHT) {
            CRGB curr = matrix.getPixel(px, py);
            matrix.setPixel(px, py, curr + sparkColor);

            // Subtle vertical trailing tail
            if (py > 0) {
                CRGB tailColor = sparkColor;
                tailColor.nscale8_video(80);
                matrix.setPixel(px, py - 1, matrix.getPixel(px, py - 1) + tailColor);
            }
        }
    }
}

// =============================================================================
// 3: EffectPerlinNoise (Seamless cylindrical 3D Perlin Noise)
// =============================================================================

void EffectPerlinNoise::init(LedMatrix& matrix) {
    // Precompute cylindrical angle projection coordinates to guarantee zero seam
    for (int x = 0; x < MATRIX_WIDTH; x++) {
        float angle = (static_cast<float>(x) / static_cast<float>(MATRIX_WIDTH)) * 6.2831853f;
        // Map unit circle [-1.0, 1.0] into uint16_t scale for FastLED inoise8
        m_xCoords[x] = static_cast<uint16_t>((cosf(angle) + 1.0f) * 1200.0f);
        m_yCoords[x] = static_cast<uint16_t>((sinf(angle) + 1.0f) * 1200.0f);
    }
    matrix.clear(false);
}

void EffectPerlinNoise::update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) {
    uint32_t now = millis();
    uint32_t zTime = now / 15;
    uint8_t colorShift = static_cast<uint8_t>(now / 35);

    for (int x = 0; x < MATRIX_WIDTH; x++) {
        uint16_t nx = m_xCoords[x];
        uint16_t ny = m_yCoords[x];

        for (int y = 0; y < MATRIX_HEIGHT; y++) {
            // Sample 3D Perlin noise across cylindrical space: (nx, ny, zTime + vertical wave)
            uint8_t noiseVal = inoise8(nx, ny, (y * 45) + zTime);
            uint8_t hue = noiseVal + colorShift;
            uint8_t bri = qadd8(noiseVal, 30); // keep clouds vivid

            matrix.setPixel(x, y, CHSV(hue, 220, bri));
        }
    }
}

// =============================================================================
// 9: EffectFireflies (15 Fireflies on seamless cylinder with bilinear rendering)
// =============================================================================

void EffectFireflies::init(LedMatrix& matrix) {
    m_lastUpdateTime = millis();
    matrix.clear(false);

    // Warm golden, lime, and amber color palette
    const CRGB palette[4] = {
        CRGB(255, 190, 20),  // Warm gold
        CRGB(150, 255, 25),  // Lime
        CRGB(255, 120, 10),  // Amber
        CRGB(220, 230, 20)   // Golden lime
    };

    for (uint8_t i = 0; i < NUM_FIREFLIES; i++) {
        m_fireflies[i].x = static_cast<float>(random8(MATRIX_WIDTH));
        m_fireflies[i].y = static_cast<float>(random8(MATRIX_HEIGHT));
        m_fireflies[i].vx = (static_cast<float>(random8(100)) - 50.0f) / 100.0f * 0.8f;
        m_fireflies[i].vy = (static_cast<float>(random8(100)) - 50.0f) / 100.0f * 0.6f;
        m_fireflies[i].phase = (static_cast<float>(random8(255)) / 255.0f) * 6.2831853f;
        m_fireflies[i].phaseSpeed = 0.02f + (static_cast<float>(random8(30)) / 1000.0f);
        m_fireflies[i].color = palette[i % 4];
    }
}

void EffectFireflies::update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) {
    uint32_t now = millis();
    float dt = (now - m_lastUpdateTime) / 1000.0f;
    if (dt > 0.1f) dt = 0.016f;
    m_lastUpdateTime = now;

    float speedFactor = static_cast<float>(m_speed) / 128.0f;

    // Clear frame buffer
    matrix.clear(false);

    for (uint8_t i = 0; i < NUM_FIREFLIES; i++) {
        Firefly& f = m_fireflies[i];

        // Organic random drift / steering
        f.vx += (static_cast<float>(random8(100)) - 50.0f) / 100.0f * 0.5f * dt * speedFactor;
        f.vy += (static_cast<float>(random8(100)) - 50.0f) / 100.0f * 0.5f * dt * speedFactor;

        // Limit maximum velocities
        constexpr float maxSpeedX = 1.6f;
        constexpr float maxSpeedY = 1.2f;
        f.vx = fmaxf(-maxSpeedX, fminf(maxSpeedX, f.vx));
        f.vy = fmaxf(-maxSpeedY, fminf(maxSpeedY, f.vy));

        // Update position
        f.x += f.vx * dt * 4.0f * speedFactor;
        f.y += f.vy * dt * 4.0f * speedFactor;

        // Seamless cylinder wrap around X
        while (f.x < 0.0f) f.x += static_cast<float>(MATRIX_WIDTH);
        while (f.x >= static_cast<float>(MATRIX_WIDTH)) f.x -= static_cast<float>(MATRIX_WIDTH);

        // Clamp & bounce on Y boundary (0..15)
        if (f.y < 0.1f) {
            f.y = 0.1f;
            f.vy = fabsf(f.vy) * 0.8f;
        } else if (f.y > static_cast<float>(MATRIX_HEIGHT - 1) - 0.1f) {
            f.y = static_cast<float>(MATRIX_HEIGHT - 1) - 0.1f;
            f.vy = -fabsf(f.vy) * 0.8f;
        }

        // Advance pulsation phase
        f.phase += f.phaseSpeed * speedFactor;
        if (f.phase >= 6.2831853f) {
            f.phase -= 6.2831853f;
        }

        // Bioluminescent organic glow curve
        float rawSine = 0.5f * (1.0f + sinf(f.phase));
        float glow = powf(rawSine, 2.5f);
        if (glow < 0.01f) {
            continue; // Skip rendering if dark
        }

        // Subpixel bilinear rendering: 4 neighboring pixels
        int x0 = static_cast<int>(floorf(f.x));
        int y0 = static_cast<int>(floorf(f.y));
        float fx = f.x - static_cast<float>(x0);
        float fy = f.y - static_cast<float>(y0);

        int x1 = (x0 + 1) % MATRIX_WIDTH;
        int y1 = (y0 + 1 < MATRIX_HEIGHT) ? (y0 + 1) : (MATRIX_HEIGHT - 1);

        float w00 = (1.0f - fx) * (1.0f - fy);
        float w10 = fx * (1.0f - fy);
        float w01 = (1.0f - fx) * fy;
        float w11 = fx * fy;

        auto addGlow = [&](int px, int py, float weight) {
            if (py < 0 || py >= MATRIX_HEIGHT) return;
            px = (px % MATRIX_WIDTH + MATRIX_WIDTH) % MATRIX_WIDTH;
            float totalBri = glow * weight;
            if (totalBri <= 0.005f) return;

            uint8_t r = static_cast<uint8_t>(fminf(255.0f, f.color.r * totalBri));
            uint8_t g = static_cast<uint8_t>(fminf(255.0f, f.color.g * totalBri));
            uint8_t b = static_cast<uint8_t>(fminf(255.0f, f.color.b * totalBri));

            CRGB existing = matrix.getPixel(px, py);
            matrix.setPixel(px, py, CRGB(
                qadd8(existing.r, r),
                qadd8(existing.g, g),
                qadd8(existing.b, b)
            ));
        };

        addGlow(x0, y0, w00);
        addGlow(x1, y0, w10);
        addGlow(x0, y1, w01);
        addGlow(x1, y1, w11);
    }
}
