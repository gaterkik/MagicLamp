#pragma once

#include "effect_manager.h"

/**
 * @file math_effects.h
 * @brief Mathematical and physical simulation effects on 16x16 cylinder:
 * 4. EffectGameOfLife (Conway's Life with cylinder toroidal topology & auto-reseed)
 * 5. EffectLorenzAttractor (3D Chaotic attractor projected on cylinder with fading trail)
 * 6. EffectMetaballs (5 elastic metaballs with seamless circular distance metric)
 */

class EffectGameOfLife : public Effect {
public:
    void init(LedMatrix& matrix) override;
    void update(LedMatrix& matrix, const MicI2S& mic) override;
    const char* getName() const override { return "GameOfLife"; }

private:
    bool m_grid[MATRIX_HEIGHT][MATRIX_WIDTH];
    bool m_next[MATRIX_HEIGHT][MATRIX_WIDTH];
    uint8_t m_age[MATRIX_HEIGHT][MATRIX_WIDTH];
    uint8_t m_nextAge[MATRIX_HEIGHT][MATRIX_WIDTH];
    uint8_t m_hue[MATRIX_HEIGHT][MATRIX_WIDTH];
    uint8_t m_nextHue[MATRIX_HEIGHT][MATRIX_WIDTH];

    uint32_t m_lastStepTime = 0;
    uint32_t m_stagnantSince = 0;
    uint16_t m_prevHash1 = 0;
    uint16_t m_prevHash2 = 0;
    uint16_t m_historyHash[8];
    uint8_t m_historyIdx = 0;

    uint8_t m_currentSeedHue = 0;
    uint8_t m_seedCounter = 0;

    void seedRandom();
    void spawnSeed();
    void step(uint32_t now);
    uint16_t computeHash() const;
};

class EffectLorenzAttractor : public Effect {
public:
    void init(LedMatrix& matrix) override;
    void update(LedMatrix& matrix, const MicI2S& mic) override;
    const char* getName() const override { return "LorenzAttractor"; }

private:
    float m_x = 0.1f;
    float m_y = 0.0f;
    float m_z = 0.0f;
};

class EffectMetaballs : public Effect {
public:
    void init(LedMatrix& matrix) override;
    void update(LedMatrix& matrix, const MicI2S& mic) override;
    const char* getName() const override { return "Metaballs"; }

private:
    static constexpr int NUM_BALLS = 5;
    float m_bx[NUM_BALLS];
    float m_by[NUM_BALLS];
    float m_vx[NUM_BALLS];
    float m_vy[NUM_BALLS];
    uint8_t m_colorOffset = 0;
};
