#include "effect_vm.h"
#include <LittleFS.h>
#include <math.h>

// =============================================================================
// Helper JSON Parsing Utilities
// =============================================================================

static bool extractJsonString(const String& json, const char* key, String& outVal) {
    String searchKey = String("\"") + key + "\"";
    int idx = json.indexOf(searchKey);
    if (idx < 0) return false;
    int colon = json.indexOf(':', idx + searchKey.length());
    if (colon < 0) return false;
    int q1 = json.indexOf('"', colon);
    if (q1 < 0) return false;
    int q2 = json.indexOf('"', q1 + 1);
    if (q2 < 0) return false;
    outVal = json.substring(q1 + 1, q2);
    return true;
}

static bool extractJsonInt(const String& json, const char* key, int& outVal) {
    String searchKey = String("\"") + key + "\"";
    int idx = json.indexOf(searchKey);
    if (idx < 0) return false;
    int colon = json.indexOf(':', idx + searchKey.length());
    if (colon < 0) return false;
    int valStart = colon + 1;
    while (valStart < (int)json.length() && (json[valStart] == ' ' || json[valStart] == '\t')) valStart++;
    outVal = json.substring(valStart).toInt();
    return true;
}

static uint32_t parseHexColor(const String& hexStr) {
    String clean = hexStr;
    clean.trim();
    if (clean.startsWith("#")) clean = clean.substring(1);
    if (clean.startsWith("0x") || clean.startsWith("0X")) clean = clean.substring(2);
    return strtoul(clean.c_str(), nullptr, 16);
}

// =============================================================================
// EffectVM Implementation
// =============================================================================

EffectVM::EffectVM()
    : m_id(""),
      m_name("EffectVM"),
      m_engineName(""),
      m_engineType(VMEngineType::UNKNOWN),
      m_scale(128),
      m_intensity(255),
      m_palette(RainbowColors_p),
      m_lastFrameTime(0),
      m_phase(0.0f),
      m_rocketActive(false),
      m_rocketX(8.0f),
      m_rocketY(0.0f),
      m_rocketSpeed(12.0f),
      m_burstHeight(12.0f),
      m_pulseRadius(0.0f),
      m_pulseSpeed(4.0f) {
    m_speed = 128;
}

bool EffectVM::loadFromFile(const char* filePath) {
    if (!LittleFS.exists(filePath)) {
        Serial.printf("[EffectVM] File not found: %s\n", filePath);
        return false;
    }

    File file = LittleFS.open(filePath, "r");
    if (!file) {
        Serial.printf("[EffectVM] Failed to open file: %s\n", filePath);
        return false;
    }

    String jsonStr = file.readString();
    file.close();

    bool ok = parseDescriptor(jsonStr);
    if (ok) {
        Serial.printf("[EffectVM] Loaded effect '%s' (%s, engine=%s) from %s\n",
                      m_id.c_str(), m_name.c_str(), m_engineName.c_str(), filePath);
    }
    return ok;
}

bool EffectVM::parseDescriptor(const String& jsonStr) {
    extractJsonString(jsonStr, "id", m_id);
    extractJsonString(jsonStr, "name", m_name);
    extractJsonString(jsonStr, "engine", m_engineName);

    int speedVal = 128;
    if (extractJsonInt(jsonStr, "speed", speedVal)) {
        m_speed = (uint8_t)constrain(speedVal, 1, 255);
    }

    int scaleVal = 128;
    if (extractJsonInt(jsonStr, "scale", scaleVal)) {
        m_scale = (uint8_t)constrain(scaleVal, 1, 255);
    }

    int intensityVal = 255;
    if (extractJsonInt(jsonStr, "intensity", intensityVal)) {
        m_intensity = (uint8_t)constrain(intensityVal, 1, 255);
    }

    // Determine engine type
    m_engineName.toLowerCase();
    if (m_engineName == "aurora") {
        m_engineType = VMEngineType::AURORA;
    } else if (m_engineName == "matrix_rain" || m_engineName == "matrix") {
        m_engineType = VMEngineType::MATRIX_RAIN;
    } else if (m_engineName == "lava_lamp" || m_engineName == "lava") {
        m_engineType = VMEngineType::LAVA_LAMP;
    } else if (m_engineName == "vortex") {
        m_engineType = VMEngineType::VORTEX;
    } else if (m_engineName == "fireworks") {
        m_engineType = VMEngineType::FIREWORKS;
    } else if (m_engineName == "cyber_pulse" || m_engineName == "cyber") {
        m_engineType = VMEngineType::CYBER_PULSE;
    } else {
        Serial.printf("[EffectVM] Unknown engine: %s\n", m_engineName.c_str());
        m_engineType = VMEngineType::UNKNOWN;
        return false;
    }

    // Palette parsing
    String paletteName = "";
    extractJsonString(jsonStr, "palette", paletteName);
    String colorsStr = "";
    extractJsonString(jsonStr, "colors", colorsStr);
    setupPalette(paletteName, colorsStr);

    return true;
}

void EffectVM::setupPalette(const String& paletteName, const String& colorsCsv) {
    String p = paletteName;
    p.toLowerCase();

    if (p == "party") {
        m_palette = PartyColors_p;
    } else if (p == "ocean") {
        m_palette = OceanColors_p;
    } else if (p == "heat" || p == "fire") {
        m_palette = HeatColors_p;
    } else if (p == "forest") {
        m_palette = ForestColors_p;
    } else if (p == "lava") {
        m_palette = LavaColors_p;
    } else if (p == "cloud") {
        m_palette = CloudColors_p;
    } else if (p == "aurora") {
        // Northern lights green-cyan-purple palette (16 steps)
        m_palette = CRGBPalette16(
            CRGB(0, 40, 20), CRGB(0, 80, 40), CRGB(0, 140, 60), CRGB(0, 180, 80),
            CRGB(0, 220, 110), CRGB(0, 255, 140), CRGB(0, 255, 180), CRGB(0, 200, 220),
            CRGB(0, 160, 255), CRGB(40, 100, 255), CRGB(80, 40, 220), CRGB(120, 0, 180),
            CRGB(80, 0, 180), CRGB(0, 160, 255), CRGB(0, 255, 120), CRGB(0, 40, 20)
        );
    } else if (p == "matrix") {
        // Green matrix code gradient (16 steps)
        m_palette = CRGBPalette16(
            CRGB(0, 0, 0), CRGB(0, 20, 0), CRGB(0, 40, 0), CRGB(0, 70, 0),
            CRGB(0, 100, 5), CRGB(0, 140, 10), CRGB(0, 180, 15), CRGB(0, 220, 20),
            CRGB(0, 255, 40), CRGB(80, 255, 100), CRGB(180, 255, 200), CRGB(80, 255, 100),
            CRGB(0, 255, 40), CRGB(0, 180, 15), CRGB(0, 90, 5), CRGB(0, 20, 0)
        );
    } else if (p == "cyber" || p == "cyberpunk") {
        // Synthwave neon pink, deep indigo, bright cyan (16 steps)
        m_palette = CRGBPalette16(
            CRGB(20, 0, 40), CRGB(60, 0, 80), CRGB(120, 0, 160), CRGB(180, 0, 190),
            CRGB(255, 0, 128), CRGB(255, 40, 160), CRGB(200, 0, 220), CRGB(140, 0, 255),
            CRGB(80, 0, 255), CRGB(0, 120, 255), CRGB(0, 200, 255), CRGB(0, 240, 255),
            CRGB(180, 240, 255), CRGB(0, 240, 255), CRGB(140, 0, 255), CRGB(255, 0, 128)
        );
    } else if (!colorsCsv.isEmpty()) {
        // Parse CSV hex colors e.g. "#ff0055,#00ffff,#ffff00"
        int start = 0;
        CRGB pal[16];
        int count = 0;
        while (start < (int)colorsCsv.length() && count < 16) {
            int comma = colorsCsv.indexOf(',', start);
            String item = (comma >= 0) ? colorsCsv.substring(start, comma) : colorsCsv.substring(start);
            uint32_t c = parseHexColor(item);
            pal[count++] = CRGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
            if (comma < 0) break;
            start = comma + 1;
        }
        if (count > 0) {
            for (int i = 0; i < 16; i++) {
                m_palette[i] = pal[i % count];
            }
        }
    } else {
        m_palette = RainbowColors_p;
    }
}

void EffectVM::init(LedMatrix& matrix) {
    matrix.clear(false);
    m_lastFrameTime = millis();
    m_phase = 0.0f;

    // 1. Matrix Rain Init
    for (int x = 0; x < MATRIX_WIDTH; x++) {
        m_drops[x].y = (float)(random(0, MATRIX_HEIGHT * 2));
        m_drops[x].speed = 4.0f + (float)random(0, 8);
        m_drops[x].length = 4 + random(0, 5);
    }

    // 2. Lava Lamp Init
    for (int i = 0; i < 5; i++) {
        m_blobs[i].y = (float)(i * 3 + 1);
        m_blobs[i].targetY = (i % 2 == 0) ? 14.0f : 1.0f;
        m_blobs[i].speed = 1.0f + (float)random(0, 15) * 0.1f;
        m_blobs[i].radius = 1.8f + (float)random(0, 10) * 0.1f;
        m_blobs[i].hue = (uint8_t)(i * 45);
    }

    // 3. Fireworks Init
    m_rocketActive = false;
    for (int i = 0; i < MAX_SPARKS; i++) {
        m_sparks[i].active = false;
    }

    // 4. Cyber Pulse Init
    m_pulseRadius = 0.0f;
    m_pulseSpeed = 6.0f;
}

void EffectVM::update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) {
    uint32_t now = millis();
    float dt = (now - m_lastFrameTime) / 1000.0f;
    if (dt <= 0.0f || dt > 0.1f) dt = 0.016f; // clamp for stability
    m_lastFrameTime = now;

    float speedMult = (float)m_speed / 128.0f;
    m_phase += speedMult * dt * 2.0f;

    switch (m_engineType) {
        case VMEngineType::AURORA:
            updateAurora(matrix, dt * speedMult);
            break;
        case VMEngineType::MATRIX_RAIN:
            updateMatrixRain(matrix, dt * speedMult);
            break;
        case VMEngineType::LAVA_LAMP:
            updateLavaLamp(matrix, dt * speedMult);
            break;
        case VMEngineType::VORTEX:
            updateVortex(matrix, dt * speedMult);
            break;
        case VMEngineType::FIREWORKS:
            updateFireworks(matrix, dt * speedMult);
            break;
        case VMEngineType::CYBER_PULSE:
            updateCyberPulse(matrix, mic, dt * speedMult);
            break;
        default:
            matrix.clear(false);
            break;
    }
}

// -----------------------------------------------------------------------------
// 1. Engine: Aurora (Northern Lights on seamless cylinder)
// -----------------------------------------------------------------------------
void EffectVM::updateAurora(LedMatrix& matrix, float dt) {
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
        // Angle in radians around cylinder (0..2*PI)
        float theta = (float)x * (2.0f * M_PI / (float)MATRIX_WIDTH);

        // Undulating ribbons using polar noise & harmonic sines
        float ribbon1 = sinf(theta * 2.0f + m_phase * 1.5f) * 3.5f + 7.5f;
        float ribbon2 = cosf(theta * 3.0f - m_phase * 1.0f) * 2.5f + 9.0f;

        for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
            // Distance to ribbons
            float d1 = fabsf((float)y - ribbon1);
            float d2 = fabsf((float)y - ribbon2);

            float brightness1 = fmaxf(0.0f, 1.0f - d1 * 0.28f);
            float brightness2 = fmaxf(0.0f, 1.0f - d2 * 0.35f);
            float totalB = fminf(1.0f, brightness1 + brightness2);

            if (totalB > 0.05f) {
                // Perlin polar noise sample
                uint8_t noiseVal = inoise8((uint16_t)(x * m_scale), (uint16_t)(y * m_scale), (uint16_t)(m_phase * 100.0f));
                uint8_t palIdx = noiseVal + (y * 6) + (uint8_t)(m_phase * 20.0f);
                uint8_t bri = (uint8_t)(totalB * m_intensity);
                CRGB color = ColorFromPalette(m_palette, palIdx, bri, LINEARBLEND);
                matrix.setPixel(x, y, color);
            } else {
                matrix.setPixel(x, y, CRGB::Black);
            }
        }
    }
}

// -----------------------------------------------------------------------------
// 2. Engine: Matrix Rain (Digital falling code streams)
// -----------------------------------------------------------------------------
void EffectVM::updateMatrixRain(LedMatrix& matrix, float dt) {
    matrix.fadeToBlackBy(35);

    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
        RainDrop& drop = m_drops[x];
        drop.y -= drop.speed * dt * 2.5f;

        // When drop completely falls off bottom
        if (drop.y < -(float)drop.length) {
            drop.y = (float)(MATRIX_HEIGHT - 1 + random(1, 6));
            drop.speed = 4.0f + (float)random(0, 10);
            drop.length = 3 + random(0, 6);
        }

        int headY = (int)roundf(drop.y);
        for (int i = 0; i <= drop.length; i++) {
            int curY = headY + i;
            if (curY >= 0 && curY < MATRIX_HEIGHT) {
                if (i == 0) {
                    // Bright white/cyan head
                    matrix.setPixel(x, curY, CRGB(200, 255, 230));
                } else {
                    // Fading tail from palette
                    float tailFrac = 1.0f - ((float)i / (float)drop.length);
                    uint8_t bri = (uint8_t)(tailFrac * m_intensity);
                    uint8_t palIdx = (uint8_t)(i * 20 + x * 10);
                    CRGB color = ColorFromPalette(m_palette, palIdx, bri, LINEARBLEND);
                    matrix.setPixel(x, curY, color);
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
// 3. Engine: Lava Lamp (Thermal wax blob convection)
// -----------------------------------------------------------------------------
void EffectVM::updateLavaLamp(LedMatrix& matrix, float dt) {
    // Move blobs towards targetY with thermal buoyancy
    for (int i = 0; i < 5; i++) {
        LavaBlob& b = m_blobs[i];
        if (b.y < b.targetY) {
            b.y += b.speed * dt * 1.5f;
            if (b.y >= b.targetY) {
                b.targetY = 0.5f + (float)random(0, 4);
                b.speed = 1.0f + (float)random(0, 15) * 0.1f;
            }
        } else {
            b.y -= b.speed * dt * 1.5f;
            if (b.y <= b.targetY) {
                b.targetY = 11.5f + (float)random(0, 4);
                b.speed = 1.0f + (float)random(0, 15) * 0.1f;
            }
        }
    }

    // Render metaballs on 16x16 cylinder
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
        for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
            float field = 0.0f;
            for (int i = 0; i < 5; i++) {
                float blobX = (float)i * (16.0f / 5.0f);
                float dx = fabsf((float)x - blobX);
                if (dx > 8.0f) dx = 16.0f - dx; // Seamless circle wrapping
                float dy = (float)y - m_blobs[i].y;
                float dist2 = dx * dx + dy * dy;
                field += (m_blobs[i].radius * m_blobs[i].radius) / (dist2 + 0.8f);
            }

            if (field > 0.85f) {
                float norm = fminf(1.0f, (field - 0.85f) * 1.5f);
                uint8_t palIdx = (uint8_t)(field * 40.0f + m_phase * 10.0f);
                uint8_t bri = (uint8_t)(norm * m_intensity);
                CRGB color = ColorFromPalette(m_palette, palIdx, bri, LINEARBLEND);
                matrix.setPixel(x, y, color);
            } else {
                matrix.setPixel(x, y, CRGB::Black);
            }
        }
    }
}

// -----------------------------------------------------------------------------
// 4. Engine: Vortex (Cosmic spiral arms wrapping the cylinder)
// -----------------------------------------------------------------------------
void EffectVM::updateVortex(LedMatrix& matrix, float dt) {
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
        float normX = (float)x / (float)MATRIX_WIDTH; // 0..1
        float theta = normX * 2.0f * M_PI;

        for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
            float normY = ((float)y - 7.5f) / 7.5f; // -1..+1

            // Spiral angle with cylinder twist
            float twist = (float)m_scale / 40.0f;
            float spiral = theta * 3.0f + normY * twist * M_PI - m_phase * 2.5f;
            float armVal = (sinf(spiral) + 1.0f) * 0.5f; // 0..1

            // Core glow and vertical envelope
            float envelope = fmaxf(0.0f, 1.0f - normY * normY * 0.7f);
            float intensity = armVal * envelope;

            if (intensity > 0.15f) {
                uint8_t palIdx = (uint8_t)((armVal * 128.0f) + (normY * 40.0f) + (m_phase * 15.0f));
                uint8_t bri = (uint8_t)(intensity * m_intensity);
                CRGB color = ColorFromPalette(m_palette, palIdx, bri, LINEARBLEND);
                matrix.setPixel(x, y, color);
            } else {
                matrix.setPixel(x, y, CRGB::Black);
            }
        }
    }
}

// -----------------------------------------------------------------------------
// 5. Engine: Fireworks (Rocket launch, circular burst and gravity particles)
// -----------------------------------------------------------------------------
void EffectVM::updateFireworks(LedMatrix& matrix, float dt) {
    matrix.fadeToBlackBy(45);

    // Rocket phase
    if (!m_rocketActive) {
        // Launch a new rocket
        m_rocketActive = true;
        m_rocketX = (float)random(0, MATRIX_WIDTH);
        m_rocketY = 0.0f;
        m_rocketSpeed = 9.0f + (float)random(0, 5);
        m_burstHeight = 9.0f + (float)random(0, 6);
    } else {
        m_rocketY += m_rocketSpeed * dt;
        int rx = (int)m_rocketX % MATRIX_WIDTH;
        int ry = (int)m_rocketY;
        if (ry < MATRIX_HEIGHT) {
            matrix.setPixel(rx, ry, CRGB(255, 220, 160));
        }

        // Burst trigger
        if (m_rocketY >= m_burstHeight) {
            m_rocketActive = false;
            uint8_t baseHue = random8();

            // Explode sparks
            for (int i = 0; i < MAX_SPARKS; i++) {
                float angle = (float)i * (2.0f * M_PI / (float)MAX_SPARKS);
                float speed = 2.5f + (float)random(0, 30) * 0.1f;
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

    // Update and draw explosion sparks
    for (int i = 0; i < MAX_SPARKS; i++) {
        if (!m_sparks[i].active) continue;

        m_sparks[i].x += m_sparks[i].vx * dt;
        m_sparks[i].y += m_sparks[i].vy * dt;
        m_sparks[i].vy -= 4.5f * dt; // Gravity
        m_sparks[i].life -= dt * 1.2f;

        // Wrap X around cylinder
        while (m_sparks[i].x < 0.0f) m_sparks[i].x += (float)MATRIX_WIDTH;
        while (m_sparks[i].x >= (float)MATRIX_WIDTH) m_sparks[i].x -= (float)MATRIX_WIDTH;

        if (m_sparks[i].life <= 0.0f || m_sparks[i].y < 0.0f || m_sparks[i].y >= MATRIX_HEIGHT) {
            m_sparks[i].active = false;
        } else {
            int px = (int)m_sparks[i].x;
            int py = (int)m_sparks[i].y;
            CRGB col = m_sparks[i].color;
            col.nscale8_video((uint8_t)(m_sparks[i].life * (float)m_intensity));
            matrix.setPixel(px, py, col);
        }
    }
}

// -----------------------------------------------------------------------------
// 6. Engine: Cyber Pulse (Synthwave/cyberpunk neon rings with sound/wave reactivity)
// -----------------------------------------------------------------------------
void EffectVM::updateCyberPulse(LedMatrix& matrix, const MicI2S& mic, float dt) {
    float audioBeat = mic.getNormalizedLevel(); // 0..1
    m_pulseRadius += (m_pulseSpeed + audioBeat * 12.0f) * dt;
    if (m_pulseRadius > 14.0f) {
        m_pulseRadius = 0.0f;
    }

    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
        float normX = (float)x / (float)MATRIX_WIDTH;
        float theta = normX * 2.0f * M_PI;

        for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
            // Distance from center Y
            float dy = fabsf((float)y - 7.5f);
            float distToPulse = fabsf(dy - m_pulseRadius);

            // Ring sharpness
            float ring = fmaxf(0.0f, 1.0f - distToPulse * 0.7f);

            // Circumferential ripple
            float wave = (sinf(theta * 4.0f + m_phase * 2.0f) + 1.0f) * 0.3f;
            float total = fminf(1.0f, ring + wave * (0.4f + audioBeat * 0.6f));

            if (total > 0.1f) {
                uint8_t palIdx = (uint8_t)(total * 180.0f + y * 8 + (uint8_t)(m_phase * 20.0f));
                uint8_t bri = (uint8_t)(total * m_intensity);
                CRGB color = ColorFromPalette(m_palette, palIdx, bri, LINEARBLEND);
                matrix.setPixel(x, y, color);
            } else {
                matrix.setPixel(x, y, CRGB::Black);
            }
        }
    }
}
