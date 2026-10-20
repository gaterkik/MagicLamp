#include "math_effects.h"
#include <math.h>

static inline int wrapX(int x) {
    return (x % MATRIX_WIDTH + MATRIX_WIDTH) % MATRIX_WIDTH;
}

static inline float distRingX(float x1, float x2) {
    float dx = fabsf(x1 - x2);
    return fminf(dx, static_cast<float>(MATRIX_WIDTH) - dx);
}

// =============================================================================
// 4: EffectGameOfLife (Conway's Life with cylinder toroidal topology & shake)
// =============================================================================

void EffectGameOfLife::init(LedMatrix& matrix) {
    m_currentSeedHue = random8();
    seedRandom();
    m_prevHash1 = 0;
    m_prevHash2 = 0;
    memset(m_historyHash, 0, sizeof(m_historyHash));
    m_historyIdx = 0;
    m_stagnantSince = 0;
    m_seedCounter = 0;
    matrix.clear(false);
}

uint16_t EffectGameOfLife::computeHash() const {
    uint16_t hash = 0x811c;
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            if (m_grid[y][x]) {
                hash ^= static_cast<uint16_t>((y << 4) | x);
                hash *= 0x0103;
            }
        }
    }
    return hash;
}

void EffectGameOfLife::seedRandom() {
    m_currentSeedHue = random8();
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            bool alive = (random8(100) < 32);
            m_grid[y][x] = alive;
            m_age[y][x] = alive ? (1 + random8(6)) : 0;
            m_hue[y][x] = m_currentSeedHue + random8(24);
        }
    }
}

void EffectGameOfLife::spawnSeed() {
    m_currentSeedHue += 59; // Fresh distinct hue for newly seeded pattern
    m_seedCounter = (m_seedCounter + 1) % 4;

    // Scan for lowest-density area on cylinder
    int minDensity = 999;
    int bestX = random8(MATRIX_WIDTH);
    int bestY = 2 + random8(MATRIX_HEIGHT - 6);

    for (int attempt = 0; attempt < 12; attempt++) {
        int cx = random8(MATRIX_WIDTH);
        int cy = 2 + random8(MATRIX_HEIGHT - 6);
        int count = 0;
        for (int dy = -2; dy <= 2; dy++) {
            int ny = cy + dy;
            if (ny < 0 || ny >= MATRIX_HEIGHT) continue;
            for (int dx = -2; dx <= 2; dx++) {
                int nx = wrapX(cx + dx);
                if (m_grid[ny][nx]) count++;
            }
        }
        if (count < minDensity) {
            minDensity = count;
            bestX = cx;
            bestY = cy;
            if (count == 0) break;
        }
    }

    // Pattern definitions
    // 0: Glider (3x3)
    static const uint8_t PAT_GLIDER[3][3] = {
        {0, 1, 0},
        {0, 0, 1},
        {1, 1, 1}
    };
    // 1: R-pentomino (3x3)
    static const uint8_t PAT_RPENTOMINO[3][3] = {
        {0, 1, 1},
        {1, 1, 0},
        {0, 1, 0}
    };
    // 2: Lightweight Spaceship LWSS (4x5)
    static const uint8_t PAT_LWSS[4][5] = {
        {0, 1, 0, 0, 1},
        {1, 0, 0, 0, 0},
        {1, 0, 0, 0, 1},
        {1, 1, 1, 1, 0}
    };
    // 3: Acorn (3x7)
    static const uint8_t PAT_ACORN[3][7] = {
        {0, 1, 0, 0, 0, 0, 0},
        {0, 0, 0, 1, 0, 0, 0},
        {1, 1, 0, 0, 1, 1, 1}
    };

    if (m_seedCounter == 0) {
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                if (PAT_GLIDER[r][c]) {
                    int y = bestY + r;
                    if (y >= 0 && y < MATRIX_HEIGHT) {
                        int x = wrapX(bestX + c);
                        m_grid[y][x] = true;
                        m_age[y][x] = 1; // start young (soft glow)
                        m_hue[y][x] = m_currentSeedHue;
                    }
                }
            }
        }
    } else if (m_seedCounter == 1) {
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                if (PAT_RPENTOMINO[r][c]) {
                    int y = bestY + r;
                    if (y >= 0 && y < MATRIX_HEIGHT) {
                        int x = wrapX(bestX + c);
                        m_grid[y][x] = true;
                        m_age[y][x] = 1;
                        m_hue[y][x] = m_currentSeedHue;
                    }
                }
            }
        }
    } else if (m_seedCounter == 2) {
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 5; c++) {
                if (PAT_LWSS[r][c]) {
                    int y = bestY + r;
                    if (y >= 0 && y < MATRIX_HEIGHT) {
                        int x = wrapX(bestX + c);
                        m_grid[y][x] = true;
                        m_age[y][x] = 1;
                        m_hue[y][x] = m_currentSeedHue;
                    }
                }
            }
        }
    } else {
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 7; c++) {
                if (PAT_ACORN[r][c]) {
                    int y = bestY + r;
                    if (y >= 0 && y < MATRIX_HEIGHT) {
                        int x = wrapX(bestX + c);
                        m_grid[y][x] = true;
                        m_age[y][x] = 1;
                        m_hue[y][x] = m_currentSeedHue;
                    }
                }
            }
        }
    }
}

void EffectGameOfLife::step(uint32_t now) {
    int aliveCount = 0;

    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            int neighbors = 0;
            uint8_t parentHue = m_currentSeedHue;

            for (int dy = -1; dy <= 1; dy++) {
                int ny = y + dy;
                if (ny < 0 || ny >= MATRIX_HEIGHT) continue;
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = wrapX(x + dx);
                    if (m_grid[ny][nx]) {
                        neighbors++;
                        parentHue = m_hue[ny][nx];
                    }
                }
            }

            bool wasAlive = m_grid[y][x];
            bool isAlive = wasAlive ? (neighbors == 2 || neighbors == 3) : (neighbors == 3);
            m_next[y][x] = isAlive;

            if (isAlive) {
                aliveCount++;
                if (wasAlive) {
                    m_nextAge[y][x] = (m_age[y][x] < 24) ? (m_age[y][x] + 1) : 24;
                    m_nextHue[y][x] = m_hue[y][x];
                } else {
                    m_nextAge[y][x] = 1; // freshly born
                    m_nextHue[y][x] = parentHue;
                }
            } else {
                m_nextAge[y][x] = 0;
                m_nextHue[y][x] = 0;
            }
        }
    }

    memcpy(m_grid, m_next, sizeof(m_grid));
    memcpy(m_age, m_nextAge, sizeof(m_age));
    memcpy(m_hue, m_nextHue, sizeof(m_hue));

    // Stagnation detection: extinction (<4 cells), static (period 1), or oscillation (period 2)
    uint16_t currentHash = computeHash();
    bool isStatic = (currentHash == m_prevHash1);
    bool isPeriod2 = (currentHash == m_prevHash2);
    bool isExtinct = (aliveCount < 4);

    if (isExtinct || isStatic || isPeriod2) {
        if (m_stagnantSince == 0) {
            m_stagnantSince = now;
        }

        // If stagnant for > 4 seconds or completely extinct, softly inject a new seed pattern
        if ((now - m_stagnantSince >= 4000) || (aliveCount == 0)) {
            spawnSeed();
            m_stagnantSince = now; // reset timer to allow newly seeded cells to flourish
        }
    } else {
        m_stagnantSince = 0;
    }

    m_prevHash2 = m_prevHash1;
    m_prevHash1 = currentHash;
    m_historyHash[m_historyIdx] = currentHash;
    m_historyIdx = (m_historyIdx + 1) % 8;
}

void EffectGameOfLife::update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) {
    uint32_t now = millis();

    // Physical shake triggers instant re-seeding of the entire colony
    if (const_cast<ImuMpu6050&>(imu).isShaken()) {
        seedRandom();
        m_lastStepTime = now;
        m_stagnantSince = 0;
    }

    // Step evolution interval scaled by speed (400 ms at speed 1 to 40 ms at speed 255)
    uint32_t delayMs = 400 - static_cast<uint32_t>(m_speed - 1) * (400 - 40) / 254;
    if (now - m_lastStepTime >= delayMs) {
        m_lastStepTime = now;
        step(now);
    }

    // Render with smooth age-based colors and soft bloom for newly born cells
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            if (m_grid[y][x]) {
                uint8_t hue = m_hue[y][x];
                // Smooth bloom: age 1 is soft (90), age 2 is 130, age 3 is 170, age 4 is 210, age >=5 is 255
                uint8_t bri = (m_age[y][x] <= 1) ? 90 :
                              (m_age[y][x] <= 2) ? 130 :
                              (m_age[y][x] <= 3) ? 170 :
                              (m_age[y][x] <= 4) ? 210 : 255;
                matrix.setPixel(x, y, CHSV(hue, 220, bri));
            } else {
                matrix.setPixel(x, y, CRGB::Black);
            }
        }
    }
}

// =============================================================================
// 5: EffectLorenzAttractor (3D Chaotic attractor projected on cylinder)
// =============================================================================

void EffectLorenzAttractor::init(LedMatrix& matrix) {
    m_x = 0.1f;
    m_y = 0.0f;
    m_z = 0.0f;
    matrix.clear(false);
}

void EffectLorenzAttractor::update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) {
    // Gentle persistence trail
    matrix.fadeToBlackBy(36);

    // Chaotic differential equation integration scaled by speed
    float speedMult = static_cast<float>(m_speed) / 128.0f;
    float dt = 0.005f * speedMult;
    int substeps = constrain(static_cast<int>(6 * speedMult), 2, 12);
    constexpr float sigma = 10.0f;
    constexpr float rho = 28.0f;
    constexpr float beta = 8.0f / 3.0f;

    for (int substep = 0; substep < substeps; substep++) {
        float dx = sigma * (m_y - m_x) * dt;
        float dy = (m_x * (rho - m_z) - m_y) * dt;
        float dz = (m_x * m_y - beta * m_z) * dt;

        m_x += dx;
        m_y += dy;
        m_z += dz;

        // Angle around Z axis -> cylinder azimuth X (0..15)
        float angle = atan2f(m_y, m_x); // -PI .. +PI
        int px = static_cast<int>(floorf(((angle + 3.14159265f) / 6.2831853f) * MATRIX_WIDTH)) % MATRIX_WIDTH;

        // Z coordinate (typically 0..48) -> cylinder height Y (0..15)
        int py = static_cast<int>(floorf((m_z / 48.0f) * (MATRIX_HEIGHT - 1)));
        if (py < 0) py = 0;
        if (py >= MATRIX_HEIGHT) py = MATRIX_HEIGHT - 1;

        uint8_t hue = static_cast<uint8_t>(m_z * 5.0f + (millis() / 25));
        matrix.setPixel(px, py, CHSV(hue, 240, 255));
    }
}

// =============================================================================
// 6: EffectMetaballs (5 elastic metaballs with seamless circular distance metric)
// =============================================================================

void EffectMetaballs::init(LedMatrix& matrix) {
    for (int i = 0; i < NUM_BALLS; i++) {
        m_bx[i] = random8(MATRIX_WIDTH);
        m_by[i] = random8(MATRIX_HEIGHT);
        m_vx[i] = (random8(100) > 50 ? 0.08f : -0.08f) * (1.0f + (i * 0.18f));
        m_vy[i] = (random8(100) > 50 ? 0.07f : -0.07f) * (1.0f + (i * 0.18f));
    }
    m_colorOffset = 0;
    matrix.clear(false);
}

void EffectMetaballs::update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) {
    float speedMult = static_cast<float>(m_speed) / 128.0f;

    // Move metaball centers scaled by speed
    for (int i = 0; i < NUM_BALLS; i++) {
        m_bx[i] += m_vx[i] * speedMult;
        m_by[i] += m_vy[i] * speedMult;

        // Seamless wrap X around cylinder
        if (m_bx[i] < 0.0f) m_bx[i] += static_cast<float>(MATRIX_WIDTH);
        if (m_bx[i] >= static_cast<float>(MATRIX_WIDTH)) m_bx[i] -= static_cast<float>(MATRIX_WIDTH);

        // Elastic bounce on top/bottom Y boundaries
        if (m_by[i] <= 0.0f) {
            m_by[i] = 0.0f;
            m_vy[i] = -m_vy[i];
        } else if (m_by[i] >= static_cast<float>(MATRIX_HEIGHT - 1)) {
            m_by[i] = static_cast<float>(MATRIX_HEIGHT - 1);
            m_vy[i] = -m_vy[i];
        }
    }

    uint32_t now = millis();
    m_colorOffset = static_cast<uint8_t>((now * speedMult) / 35.0f);

    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            float potential = 0.0f;
            for (int i = 0; i < NUM_BALLS; i++) {
                // Shortest ring distance around cylinder
                float dx = distRingX(static_cast<float>(x), m_bx[i]);
                float dy = static_cast<float>(y) - m_by[i];
                float distSq = dx * dx + dy * dy + 0.5f;
                potential += 1.7f / distSq;
            }

            uint8_t bri = static_cast<uint8_t>(fminf(255.0f, potential * 70.0f));
            uint8_t hue = static_cast<uint8_t>(potential * 55.0f + m_colorOffset);
            matrix.setPixel(x, y, CHSV(hue, 230, bri));
        }
    }
}

// =============================================================================
// 7: EffectSandHourglass (Granular physics with GY-521 3D gravity vector)
// =============================================================================

void EffectSandHourglass::init(LedMatrix& matrix) {
    memset(m_grid, 0, sizeof(m_grid));
    // Fill lower 6 rows (~96 sand grains) with glowing multi-tonal sand colors
    for (int y = 0; y < 6; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            // Assign varying hues: golden, amber, ruby, turquoise
            uint8_t hue = 25 + (x * 4 + y * 8) % 35;
            m_grid[y][x] = hue ? hue : 1;
        }
    }
    m_lastPhysTime = millis();
    matrix.clear(false);
}

void EffectSandHourglass::scatterSand() {
    // Scatter grains randomly across matrix on shake
    uint8_t grains[NUM_LEDS];
    int count = 0;
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            if (m_grid[y][x]) {
                grains[count++] = m_grid[y][x];
            }
        }
    }

    memset(m_grid, 0, sizeof(m_grid));
    for (int i = 0; i < count; i++) {
        int rx = random8(MATRIX_WIDTH);
        int ry = random8(MATRIX_HEIGHT);
        while (m_grid[ry][rx] != 0) {
            rx = random8(MATRIX_WIDTH);
            ry = random8(MATRIX_HEIGHT);
        }
        m_grid[ry][rx] = grains[i];
    }
}

void EffectSandHourglass::updatePhysics(const ImuMpu6050& imu) {
    float gx = imu.getGx();
    float gy = imu.getGy();

    // Determine fall directions based on 3D gravity vector:
    // If gy > 0.15: inverted lamp -> particles fall upwards toward Y=15
    // If gy <= 0.15: upright lamp -> particles fall downwards toward Y=0
    bool fallUp = (gy > 0.15f);

    // Lateral drift along X ring based on tilt
    int lateralDir = 0;
    if (gx > 0.20f) lateralDir = 1;
    else if (gx < -0.20f) lateralDir = -1;

    if (!fallUp) {
        // Fall down towards Y = 0
        for (int y = 1; y < MATRIX_HEIGHT; y++) {
            for (int x = 0; x < MATRIX_WIDTH; x++) {
                if (m_grid[y][x]) {
                    uint8_t particle = m_grid[y][x];
                    // 1. Direct fall straight down
                    if (m_grid[y - 1][x] == 0) {
                        m_grid[y][x] = 0;
                        m_grid[y - 1][x] = particle;
                    } else {
                        // 2. Diagonal slide down slope
                        int slideX = (lateralDir != 0) ? wrapX(x + lateralDir) : (random8(2) ? wrapX(x - 1) : wrapX(x + 1));
                        if (m_grid[y - 1][slideX] == 0) {
                            m_grid[y][x] = 0;
                            m_grid[y - 1][slideX] = particle;
                        }
                    }
                }
            }
        }
    } else {
        // Inverted: fall up towards Y = 15
        for (int y = MATRIX_HEIGHT - 2; y >= 0; y--) {
            for (int x = 0; x < MATRIX_WIDTH; x++) {
                if (m_grid[y][x]) {
                    uint8_t particle = m_grid[y][x];
                    // 1. Direct fall straight up
                    if (m_grid[y + 1][x] == 0) {
                        m_grid[y][x] = 0;
                        m_grid[y + 1][x] = particle;
                    } else {
                        // 2. Diagonal slide up slope
                        int slideX = (lateralDir != 0) ? wrapX(x + lateralDir) : (random8(2) ? wrapX(x - 1) : wrapX(x + 1));
                        if (m_grid[y + 1][slideX] == 0) {
                            m_grid[y][x] = 0;
                            m_grid[y + 1][slideX] = particle;
                        }
                    }
                }
            }
        }
    }
}

void EffectSandHourglass::update(LedMatrix& matrix, const ImuMpu6050& imu, const MicI2S& mic) {
    uint32_t now = millis();

    // Physical shake triggers explosive dispersal of sand grains
    if (const_cast<ImuMpu6050&>(imu).isShaken()) {
        scatterSand();
    }

    // Step physics at ~25 Hz (40 ms)
    if (now - m_lastPhysTime >= 40) {
        m_lastPhysTime = now;
        updatePhysics(imu);
    }

    // Render sand particles
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            if (m_grid[y][x]) {
                uint8_t hue = m_grid[y][x];
                uint8_t shimmer = sin8(static_cast<uint8_t>(x * 25 + y * 18 + (now / 18)));
                uint8_t bri = map(shimmer, 0, 255, 200, 255);
                matrix.setPixel(x, y, CHSV(hue, 220, bri));
            } else {
                matrix.setPixel(x, y, CRGB::Black);
            }
        }
    }
}
