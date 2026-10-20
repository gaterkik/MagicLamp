#pragma once

#include <Arduino.h>
#include <driver/i2s_std.h>
#include "config.h"

#ifndef MIC_SAMPLE_RATE_HZ
#define MIC_SAMPLE_RATE_HZ 16000
#endif

#ifndef MIC_DMA_FRAME_NUM
#define MIC_DMA_FRAME_NUM 128
#endif

#ifndef MIC_DMA_DESC_NUM
#define MIC_DMA_DESC_NUM 4
#endif

/**
 * @file mic_i2s.h
 * @brief Non-blocking I2S microphone driver for INMP441 on ESP32-C6.
 *
 * Hardware characteristics:
 * - INMP441 digital MEMS microphone connected via standard I2S bus.
 * - SCK (BCLK) = GPIO 18 (PIN_MIC_SCK)
 * - WS  (LRCLK) = GPIO 19 (PIN_MIC_WS)
 * - SD  (DIN)   = GPIO 20 (PIN_MIC_SD)
 * - L/R pin tied to GND (Left channel).
 * - ESP-IDF 5.x modern driver (<driver/i2s_std.h>) using DMA.
 * - Strictly non-blocking audio processing with 0-timeout DMA polling.
 * - Real-time metrics: RMS, peak amplitude, and normalized level [0.0..1.0] / [0..255].
 * - Built-in DC offset removal, smooth decay ballistics, and adaptive AGC.
 */
class MicI2S {
public:
    /**
     * @brief Construct a new MicI2S driver instance.
     * @param pinSck Clock pin (BCLK), default PIN_MIC_SCK (18).
     * @param pinWs Word select pin (LRCLK), default PIN_MIC_WS (19).
     * @param pinSd Serial data pin (DIN), default PIN_MIC_SD (20).
     */
    MicI2S(int8_t pinSck = PIN_MIC_SCK,
           int8_t pinWs = PIN_MIC_WS,
           int8_t pinSd = PIN_MIC_SD);

    ~MicI2S();

    /**
     * @brief Initialize I2S peripheral and start audio acquisition.
     * @param sampleRate Audio sample rate in Hz (default: MIC_SAMPLE_RATE_HZ, 16000 Hz).
     * @return true if initialization succeeded, false otherwise.
     */
    bool begin(uint32_t sampleRate = MIC_SAMPLE_RATE_HZ);

    /**
     * @brief Stop I2S peripheral and release resources.
     */
    void end();

    /**
     * @brief Non-blocking audio update function to be called from loop().
     * Drains available DMA buffers with 0 timeout, calculates RMS/peak metrics,
     * updates DC offset compensation, AGC, and exponential decay.
     * Strictly non-blocking, takes ~10-30 microseconds, never blocks LED matrix rendering (~60 FPS).
     */
    void update();

    /**
     * @brief Check whether driver is active and running.
     */
    bool isRunning() const { return m_running; }

    /**
     * @brief Get normalized volume level [0.0f .. 1.0f] with AGC and smooth decay.
     */
    float getNormalizedLevel() const { return m_smoothLevel; }

    /**
     * @brief Get volume level mapped to 8-bit range [0 .. 255] for LED effects.
     */
    uint8_t getLevel8() const {
        int val = static_cast<int>(m_smoothLevel * 255.0f + 0.5f);
        if (val < 0) return 0;
        if (val > 255) return 255;
        return static_cast<uint8_t>(val);
    }

    /**
     * @brief Get smoothed Root Mean Square (RMS) value in normalized range [0.0f .. 1.0f].
     */
    float getRms() const { return m_smoothRms; }

    /**
     * @brief Get raw RMS of the most recently processed audio buffer [0.0f .. 1.0f].
     */
    float getRawRms() const { return m_rawRms; }

    /**
     * @brief Get peak amplitude of the most recently processed audio buffer [0.0f .. 1.0f].
     */
    float getPeak() const { return m_peak; }

    /**
     * @brief Set microphone sensitivity (1..255, default: 128).
     */
    void setSensitivity(uint8_t sensitivity) {
        m_sensitivity = (sensitivity < 1) ? 1 : sensitivity;
    }

    /**
     * @brief Get microphone sensitivity (1..255).
     */
    uint8_t getSensitivity() const { return m_sensitivity; }

    /**
     * @brief Set manual gain multiplier (default: 1.0f).
     */
    void setGain(float gain) { m_gain = (gain > 0.0f) ? gain : 1.0f; }

    /**
     * @brief Get current manual gain multiplier.
     */
    float getGain() const { return m_gain; }

    /**
     * @brief Enable or disable Automatic Gain Control (AGC).
     */
    void setAgcEnabled(bool enable) { m_agcEnabled = enable; }

    /**
     * @brief Check if AGC is enabled.
     */
    bool isAgcEnabled() const { return m_agcEnabled; }

    /**
     * @brief Set decay factor for volume falloff.
     * @param decayFactor Multiplier applied per update tick during falloff (e.g. 0.85f - 0.98f).
     */
    void setDecayFactor(float decayFactor) {
        if (decayFactor > 0.0f && decayFactor < 1.0f) {
            m_decayFactor = decayFactor;
        }
    }

    /**
     * @brief Get decay factor.
     */
    float getDecayFactor() const { return m_decayFactor; }

    static constexpr uint8_t NUM_SPECTRUM_BANDS = 16;

    /**
     * @brief Get normalized magnitude for a specific frequency band (0..15).
     * @param band Band index (0 = sub-bass .. 15 = high treble).
     * @return Normalized magnitude in range [0.0f .. 1.0f].
     */
    float getBand(uint8_t band) const {
        if (band < NUM_SPECTRUM_BANDS) {
            return m_smoothBands[band];
        }
        return 0.0f;
    }

    /**
     * @brief Get pointer to all 16 frequency bands.
     */
    const float* getBands() const { return m_smoothBands; }

    /**
     * @brief Check whether a musical bass beat was detected since last call.
     * Clears the beat flag on read.
     * @return true if a beat occurred.
     */
    bool isBeat() {
        bool beat = m_beatDetected;
        m_beatDetected = false;
        return beat;
    }

    /**
     * @brief Get current instantaneous bass energy [0.0f .. 1.0f].
     */
    float getBassEnergy() const { return m_bassEnergy; }

private:
    int8_t m_pinSck;
    int8_t m_pinWs;
    int8_t m_pinSd;
    uint32_t m_sampleRate;
    bool m_running;

    i2s_chan_handle_t m_rxHandle;

    // Buffer for reading DMA samples
    int32_t m_sampleBuffer[MIC_DMA_FRAME_NUM];

    // Audio metrics
    float m_rawRms;
    float m_smoothRms;
    float m_peak;
    float m_smoothLevel;

    // 16-band spectrum & FFT
    float m_smoothBands[NUM_SPECTRUM_BANDS];
    float m_fftReal[MIC_DMA_FRAME_NUM];
    float m_fftImag[MIC_DMA_FRAME_NUM];

    // Beat detection
    bool m_beatDetected;
    float m_bassEnergy;
    float m_bassAverage;
    uint32_t m_lastBeatTime;

    // DC offset tracking
    float m_dcOffset;

    // AGC and dynamic range adaptation
    bool m_agcEnabled;
    uint8_t m_sensitivity;
    float m_gain;
    float m_noiseFloor;
    float m_agcMax;
    float m_decayFactor;

    unsigned long m_lastUpdateMs;

    void processSamples(const int32_t* samples, size_t count);
    void updateDecay(unsigned long currentMs);
    void computeSpectrum(const float* acSamples, size_t count);
};
