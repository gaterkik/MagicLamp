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
    float breathFactor = 0.90f + 0.10f * (static_cast<float>(breath) / 255.0f);

    // Warm candle / 2700K incandescent base (R:255, G:147, B:41)
    // Scaled so average LED current at master brightness 255 does not exceed 9.5 mA (total < 2450 mA)
    // This guarantees FastLED power limiter NEVER caps brightness, eliminating the 50% plateau!
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        // Frosted lantern organic vertical gradient:
        // Warm candle glow at bottom Y=0 diffusing to soft ambient at Y=15
        float relY = static_cast<float>(y) / static_cast<float>(MATRIX_HEIGHT - 1);
        float vertFactor = 0.50f + 0.50f * (1.0f - relY * 0.75f);

        uint8_t r = static_cast<uint8_t>(constrain(static_cast<int>(95.0f * vertFactor * breathFactor), 0, 255));
        uint8_t g = static_cast<uint8_t>(constrain(static_cast<int>(54.0f * vertFactor * breathFactor), 0, 255));
        uint8_t b = static_cast<uint8_t>(constrain(static_cast<int>(15.0f * vertFactor * breathFactor), 0, 255));
        CRGB col = CRGB(r, g, b);

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
    time_t rawtime;
    time(&rawtime);
    bool validTime = (rawtime > 100000);

    // 1. Check schedule if active and system time is synchronized
    if (validTime && m_schedule && m_schedule->enabled) {
        time_t localTime = rawtime + TIMEZONE_OFFSET_SEC;
        struct tm* timeinfo = gmtime(&localTime);
        if (timeinfo) {
            int curSecOfDay = timeinfo->tm_hour * 3600 + timeinfo->tm_min * 60 + timeinfo->tm_sec;

            // Dawn window
            int dawnTargetMin = m_schedule->dawnHour * 60 + m_schedule->dawnMinute;
            int dawnDurSec = (m_schedule->dawnDurationMin > 0 ? m_schedule->dawnDurationMin : 10) * 60;
            int dawnTargetSec = dawnTargetMin * 60;
            int dawnStartSec = (dawnTargetSec >= dawnDurSec) ? (dawnTargetSec - dawnDurSec) : (86400 + dawnTargetSec - dawnDurSec);

            bool inDawn = false;
            float dawnProgress = 0.0f;
            if (dawnStartSec <= dawnTargetSec) {
                if (curSecOfDay >= dawnStartSec && curSecOfDay <= dawnTargetSec) {
                    inDawn = true;
                    dawnProgress = static_cast<float>(curSecOfDay - dawnStartSec) / static_cast<float>(dawnDurSec);
                }
            } else {
                if (curSecOfDay >= dawnStartSec || curSecOfDay <= dawnTargetSec) {
                    inDawn = true;
                    int elapsed = (curSecOfDay >= dawnStartSec) ? (curSecOfDay - dawnStartSec) : (86400 - dawnStartSec + curSecOfDay);
                    dawnProgress = static_cast<float>(elapsed) / static_cast<float>(dawnDurSec);
                }
            }

            // Dusk window
            int duskStartMin = m_schedule->duskHour * 60 + m_schedule->duskMinute;
            int duskDurSec = (m_schedule->duskDurationMin > 0 ? m_schedule->duskDurationMin : 30) * 60;
            int duskStartSec = duskStartMin * 60;
            int duskEndSec = (duskStartSec + duskDurSec) % 86400;

            bool inDusk = false;
            float duskProgress = 0.0f;
            if (duskStartSec <= (duskStartSec + duskDurSec) && (duskStartSec + duskDurSec) < 86400) {
                if (curSecOfDay >= duskStartSec && curSecOfDay <= duskStartSec + duskDurSec) {
                    inDusk = true;
                    duskProgress = static_cast<float>(curSecOfDay - duskStartSec) / static_cast<float>(duskDurSec);
                }
            } else {
                if (curSecOfDay >= duskStartSec || curSecOfDay <= duskEndSec) {
                    inDusk = true;
                    int elapsed = (curSecOfDay >= duskStartSec) ? (curSecOfDay - duskStartSec) : (86400 - duskStartSec + curSecOfDay);
                    duskProgress = static_cast<float>(elapsed) / static_cast<float>(duskDurSec);
                }
            }

            if (inDawn) {
                // Dawn sunrise: smooth brightness rise from dark amber to warm sun
                dawnProgress = constrain(dawnProgress, 0.0f, 1.0f);
                for (int y = 0; y < MATRIX_HEIGHT; y++) {
                    float relY = static_cast<float>(y) / static_cast<float>(MATRIX_HEIGHT - 1);
                    float horizonDist = fabsf(relY - dawnProgress * 0.85f);
                    uint8_t warmIntensity = static_cast<uint8_t>(fmaxf(0.0f, (1.0f - horizonDist * 1.6f) * 255.0f));

                    CRGB horizonColor = blend(CRGB(50, 8, 2), CRGB(255, 175, 45), static_cast<uint8_t>(dawnProgress * 255.0f));
                    CRGB upperColor = blend(CRGB(2, 4, 18), CRGB(100, 160, 240), static_cast<uint8_t>(dawnProgress * 255.0f));
                    CRGB col = blend(upperColor, horizonColor, warmIntensity);

                    // Fade in overall brightness from dark amber to full
                    uint8_t masterScale = static_cast<uint8_t>(25 + dawnProgress * 230.0f);
                    col.nscale8_video(masterScale);

                    for (int x = 0; x < MATRIX_WIDTH; x++) {
                        matrix.setPixel(x, y, col);
                    }
                }
                return;
            } else if (inDusk) {
                // Dusk: soft amber light 2000K with smooth fade out until off at end
                duskProgress = constrain(duskProgress, 0.0f, 1.0f);
                float duskBriFactor = 1.0f - duskProgress; // 1.0 down to 0.0
                uint8_t briScale = static_cast<uint8_t>(duskBriFactor * 255.0f);

                for (int y = 0; y < MATRIX_HEIGHT; y++) {
                    float relY = static_cast<float>(y) / static_cast<float>(MATRIX_HEIGHT - 1);
                    float rowFactor = 0.5f + 0.5f * (1.0f - relY * 0.7f);
                    CRGB duskColor(
                        static_cast<uint8_t>(255 * rowFactor),
                        static_cast<uint8_t>(110 * rowFactor),
                        static_cast<uint8_t>(20 * rowFactor)
                    );
                    duskColor.nscale8_video(briScale);

                    for (int x = 0; x < MATRIX_WIDTH; x++) {
                        matrix.setPixel(x, y, duskColor);
                    }
                }
                return;
            }
        }
    }

    // 2. Manual / Outside schedule mode: diurnal sky gradient
    float cyclePhase = 0.0f; // 0.0 .. 1.0 (0=Midnight, 0.25=Dawn, 0.5=Noon, 0.75=Dusk)
    if (validTime) {
        time_t localTime = rawtime + TIMEZONE_OFFSET_SEC;
        struct tm* timeinfo = gmtime(&localTime);
        if (timeinfo) {
            uint32_t secOfDay = timeinfo->tm_hour * 3600 + timeinfo->tm_min * 60 + timeinfo->tm_sec;
            cyclePhase = static_cast<float>(secOfDay) / 86400.0f;
        }
    } else {
        constexpr uint32_t demoCycleDurationMs = 60000;
        cyclePhase = static_cast<float>((now - m_cycleStartTime) % demoCycleDurationMs) / static_cast<float>(demoCycleDurationMs);
    }

    // Solar altitude: -1.0 (Midnight) .. 0.0 (Dawn/Dusk horizon) .. +1.0 (Noon)
    float sunAltitude = -cosf(cyclePhase * 6.2831853f);

    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        float relY = static_cast<float>(y) / static_cast<float>(MATRIX_HEIGHT - 1);
        CRGB skyColor;

        if (sunAltitude < -0.15f) {
            float nightDarkness = fminf(1.0f, (-sunAltitude - 0.15f) / 0.85f);
            uint8_t blueVal = static_cast<uint8_t>(fmaxf(10.0f, 40.0f * (1.0f - relY * 0.7f) * nightDarkness));
            skyColor = CRGB(2, 4, blueVal);
        } else if (sunAltitude >= -0.15f && sunAltitude < 0.25f) {
            float dawnProgress = (sunAltitude + 0.15f) / 0.40f;
            float horizonDist = fabsf(relY - dawnProgress * 0.8f);
            uint8_t warmIntensity = static_cast<uint8_t>(fmaxf(0.0f, (1.0f - horizonDist * 1.5f) * 255.0f));

            CRGB horizonWarm = blend(CRGB(180, 20, 10), CRGB(255, 140, 20), static_cast<uint8_t>(dawnProgress * 255.0f));
            CRGB upperSky = blend(CRGB(10, 15, 60), CRGB(80, 140, 220), static_cast<uint8_t>(dawnProgress * 255.0f));

            skyColor = blend(upperSky, horizonWarm, warmIntensity);
        } else {
            float dayProgress = (sunAltitude - 0.25f) / 0.75f;
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

    // Perlin noise mask for columns X (0..15) wrapping around cylinder
    // Calculates tongue heights (Y=0..12) and draft/cooling mask
    uint8_t tongueHeights[MATRIX_WIDTH];
    uint8_t colDraft[MATRIX_WIDTH];
    uint16_t timeZ = static_cast<uint16_t>(millis() / 5);

    for (int x = 0; x < MATRIX_WIDTH; x++) {
        uint8_t angle = (x * 256) / MATRIX_WIDTH;
        uint8_t nx = cos8(angle);
        uint8_t ny = sin8(angle);
        uint8_t noise = inoise8(nx * 2, ny * 2, timeZ); // 0..255
        colDraft[x] = noise;
        // Tongue height varies between 5 and 12 across columns
        tongueHeights[x] = map(noise, 20, 235, 5, 12);
        tongueHeights[x] = constrain(tongueHeights[x], 4, 12);
    }

    // Step 1: Column-specific cooling based on Perlin draft and tongue height
    for (int y = 0; y <= 12; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            uint8_t cool;
            if (y > tongueHeights[x]) {
                // Rapid cooling above individual flame tongue tip
                cool = 35 + (y - tongueHeights[x]) * 25;
            } else {
                // Progressive cooling inside flame tongue, modulated by column draft
                cool = random8(8, 16) + (y * 2);
                uint8_t draftBonus = colDraft[x] / 30;
                if (cool > draftBonus) {
                    cool -= draftBonus;
                } else {
                    cool = 4;
                }
            }

            if (m_currentHeat[y][x] > cool) {
                m_currentHeat[y][x] -= cool;
            } else {
                m_currentHeat[y][x] = 0;
            }
        }
    }

    // Step 2: Convective vertical heat rise UPWARD within Y=0..12
    // NO horizontal 3-point blur (eliminating water ripples / waves)
    for (int y = 12; y >= 1; y--) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            uint16_t heatBelow = m_currentHeat[y - 1][x];
            // Pure vertical rise with viscosity/damping
            m_currentHeat[y][x] = static_cast<uint8_t>((heatBelow * 4 + m_currentHeat[y][x]) / 5);
        }
    }

    // Upper zone Y = 13..15 is strictly dark
    for (int y = 13; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            m_currentHeat[y][x] = 0;
            m_prevHeat[y][x] = 0;
        }
    }

    // Step 3: Randomly ignite new heat at bottom row Y=0, biased by column draft
    for (int x = 0; x < MATRIX_WIDTH; x++) {
        uint8_t igniteChance = map(colDraft[x], 0, 255, 40, 75);
        if (random8(100) < igniteChance) {
            uint8_t minHeat = map(colDraft[x], 0, 255, 150, 210);
            uint8_t spark = random8(minHeat, 255);
            m_currentHeat[0][x] = static_cast<uint8_t>((m_currentHeat[0][x] + spark) / 2);
        }
    }

    // Step 4: Sporadically detach bright airborne ember sparks from active flame tips
    // Max 1-2 sparks simultaneously (MAX_FIRE_SPARKS = 2)
    if (random8(100) < 18) {
        for (int i = 0; i < MAX_FIRE_SPARKS; i++) {
            if (!m_sparks[i].active) {
                int sparkCol = random8(MATRIX_WIDTH);
                m_sparks[i].active = true;
                m_sparks[i].x = static_cast<float>(sparkCol);
                m_sparks[i].y = static_cast<float>(tongueHeights[sparkCol]); // launch from tongue tip (Y=8..12)
                m_sparks[i].vx = (static_cast<float>(random8(100)) - 50.0f) / 350.0f; // drift
                m_sparks[i].vy = 0.16f + (static_cast<float>(random8(50)) / 250.0f);   // upward flight
                m_sparks[i].brightness = 255.0f;
                // Decay so spark extinguishes gracefully in the dark zone (Y=13..15)
                m_sparks[i].decay = 6.0f + (static_cast<float>(random8(40)) / 10.0f); // decay 6.0..10.0
                // Color: strictly deep red-orange / crimson embers (NO white, B=0)
                m_sparks[i].color = CRGB(random8(180, 221), random8(15, 36), 0);
                break;
            }
        }
    }
}

void EffectFire::update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) {
    uint32_t now = millis();
    // Simulation step interval scaled by m_speed (default 128 -> ~50ms)
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

    // Render interpolated heat map: flame body Y=0..12, dark upper zone Y=13..15
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            if (y > 12) {
                matrix.setPixel(x, y, CRGB::Black);
            } else {
                float h0 = static_cast<float>(m_prevHeat[y][x]);
                float h1 = static_cast<float>(m_currentHeat[y][x]);
                uint8_t heatVal = static_cast<uint8_t>(h0 + (h1 - h0) * fraction);
                matrix.setPixel(x, y, HeatColor(heatVal));
            }
        }
    }

    // Render flying sparks through the flame and upper dark zone (Y=13..15)
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
        // Ember cooling: shifts from CRGB(220, 35, 0) down to CRGB(100, 5, 0), zero blue/white
        uint8_t r = map(bri, 0, 255, 100, m_sparks[i].color.r);
        uint8_t g = map(bri, 0, 255, 5, m_sparks[i].color.g);
        CRGB sparkColor = CRGB(r, g, 0);
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
                tailColor.nscale8_video(70);
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
    m_zAccum = 0.0f;
    m_colorAccum = 0.0f;
    m_lastPerlinTime = millis();
    matrix.clear(false);
}

void EffectPerlinNoise::update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) {
    uint32_t now = millis();
    float dt = (now - m_lastPerlinTime) / 1000.0f;
    if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;
    m_lastPerlinTime = now;

    // Scale z/t shift proportionally to speed (1..255, default 128)
    float speedMult = static_cast<float>(m_speed) / 128.0f;
    m_zAccum += dt * 66.0f * speedMult;
    m_colorAccum += dt * 28.0f * speedMult;

    uint32_t zTime = static_cast<uint32_t>(m_zAccum);
    uint8_t colorShift = static_cast<uint8_t>(m_colorAccum);

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
