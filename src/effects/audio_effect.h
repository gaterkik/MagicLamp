#pragma once

#include "effect_manager.h"

/**
 * @file audio_effect.h
 * @brief 16-band circular spectrum analyzer audio-reactive effect for MagicLamp.
 * 
 * Features:
 * - 16 frequency bands mapped 1:1 to the 16 cylinder columns (X = 0..15).
 * - Smooth vertical columns with green->yellow->red gradient.
 * - Floating gravity-decay peak hold markers.
 * - Bass beat detector triggering musical pulse flashes and energy ripples.
 */

class EffectAudioReactive : public Effect {
public:
    void init(LedMatrix& matrix) override;
    void update(LedMatrix& matrix, const MicI2S& mic) override;
    const char* getName() const override { return "AudioReactive"; }

private:
    float m_peakY[MATRIX_WIDTH];
    uint8_t m_beatFlash;
    float m_rippleY;
};
