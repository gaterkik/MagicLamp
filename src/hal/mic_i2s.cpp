#include "hal/mic_i2s.h"
#include <cmath>
#include <cstring>
#include "esp_err.h"

MicI2S::MicI2S(int8_t pinSck, int8_t pinWs, int8_t pinSd)
    : m_pinSck(pinSck),
      m_pinWs(pinWs),
      m_pinSd(pinSd),
      m_sampleRate(MIC_SAMPLE_RATE_HZ),
      m_running(false),
      m_rxHandle(nullptr),
      m_rawRms(0.0f),
      m_smoothRms(0.0f),
      m_peak(0.0f),
      m_smoothLevel(0.0f),
      m_dcOffset(0.0f),
      m_agcEnabled(true),
      m_sensitivity(128),
      m_gain(1.0f),
      m_noiseFloor(0.0005f),
      m_agcMax(0.02f),
      m_decayFactor(0.92f),
      m_beatDetected(false),
      m_bassEnergy(0.0f),
      m_bassAverage(0.0f),
      m_lastBeatTime(0),
      m_lastUpdateMs(0) {
    memset(m_sampleBuffer, 0, sizeof(m_sampleBuffer));
    memset(m_smoothBands, 0, sizeof(m_smoothBands));
    memset(m_fftReal, 0, sizeof(m_fftReal));
    memset(m_fftImag, 0, sizeof(m_fftImag));
}

MicI2S::~MicI2S() {
    end();
}

bool MicI2S::begin(uint32_t sampleRate) {
    if (m_running) {
        return true;
    }
    if (m_rxHandle != nullptr) {
        end();
    }

    m_sampleRate = sampleRate;

    // 1. Configure I2S channel controller for RX master simplex mode
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = MIC_DMA_DESC_NUM;
    chan_cfg.dma_frame_num = MIC_DMA_FRAME_NUM;
    chan_cfg.auto_clear_after_cb = false;
    chan_cfg.auto_clear_before_cb = false;

    esp_err_t err = i2s_new_channel(&chan_cfg, nullptr, &m_rxHandle);
    if (err != ESP_OK || m_rxHandle == nullptr) {
        Serial.printf("[MicI2S] ERROR: Failed to allocate I2S channel (0x%x: %s)\n",
                      err, esp_err_to_name(err));
        m_rxHandle = nullptr;
        return false;
    }

    // 2. Configure standard I2S (Philips format) for INMP441
    // INMP441 uses 24-bit data in 32-bit slot, Left channel when L/R pin is tied to GND.
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(m_sampleRate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = static_cast<gpio_num_t>(m_pinSck),
            .ws   = static_cast<gpio_num_t>(m_pinWs),
            .dout = I2S_GPIO_UNUSED,
            .din  = static_cast<gpio_num_t>(m_pinSd),
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    err = i2s_channel_init_std_mode(m_rxHandle, &std_cfg);
    if (err != ESP_OK) {
        Serial.printf("[MicI2S] ERROR: Failed to init standard I2S mode (0x%x: %s)\n",
                      err, esp_err_to_name(err));
        i2s_del_channel(m_rxHandle);
        m_rxHandle = nullptr;
        return false;
    }

    // 3. Enable the I2S RX channel
    err = i2s_channel_enable(m_rxHandle);
    if (err != ESP_OK) {
        Serial.printf("[MicI2S] ERROR: Failed to enable I2S channel (0x%x: %s)\n",
                      err, esp_err_to_name(err));
        i2s_del_channel(m_rxHandle);
        m_rxHandle = nullptr;
        return false;
    }

    m_running = true;
    m_lastUpdateMs = millis();
    Serial.printf("[MicI2S] Initialized INMP441 on SCK=%d, WS=%d, SD=%d @ %lu Hz (Standard Philips Mono)\n",
                  m_pinSck, m_pinWs, m_pinSd, static_cast<unsigned long>(m_sampleRate));
    return true;
}

void MicI2S::end() {
    if (m_rxHandle != nullptr) {
        if (m_running) {
            i2s_channel_disable(m_rxHandle);
            m_running = false;
        }
        i2s_del_channel(m_rxHandle);
        m_rxHandle = nullptr;
    }
}

void MicI2S::update() {
    if (!m_running || m_rxHandle == nullptr) {
        return;
    }

    size_t bytesRead = 0;
    int chunksRead = 0;
    const int maxChunks = MIC_DMA_DESC_NUM;

    while (chunksRead < maxChunks) {
        // Non-blocking read with timeout 0: returns immediately if no DMA buffer ready
        esp_err_t err = i2s_channel_read(m_rxHandle, m_sampleBuffer, sizeof(m_sampleBuffer), &bytesRead, 0);
        if (err == ESP_OK && bytesRead > 0) {
            size_t sampleCount = bytesRead / sizeof(int32_t);
            if (sampleCount > 0) {
                processSamples(m_sampleBuffer, sampleCount);
                chunksRead++;
            }
        } else {
            // Buffer empty or timeout: return without blocking
            break;
        }
    }

    unsigned long now = millis();
    if (chunksRead == 0) {
        // No new audio buffer received in this tick, update smooth decay
        updateDecay(now);
    } else {
        m_lastUpdateMs = now;
    }
}

void MicI2S::processSamples(const int32_t* samples, size_t count) {
    if (count == 0) return;

    float sumSq = 0.0f;
    float peakVal = 0.0f;

    for (size_t i = 0; i < count; ++i) {
        // INMP441 outputs 24-bit MSB-aligned data in a 32-bit slot
        // Arithmetic right-shift preserves sign extension
        int32_t raw = samples[i] >> 8;
        float s = static_cast<float>(raw);

        // Low-pass DC offset tracking (~10-20 Hz high-pass equivalent)
        m_dcOffset += (s - m_dcOffset) * 0.005f;
        float ac = s - m_dcOffset;

        // Normalized to +/- 1.0 (24-bit full-scale is 8,388,608)
        float norm = ac / 8388608.0f;
        float absVal = fabsf(norm);

        if (absVal > peakVal) {
            peakVal = absVal;
        }
        sumSq += norm * norm;
    }

    // Store AC samples for FFT spectrum calculation
    float acSamples[MIC_DMA_FRAME_NUM];
    size_t fftCount = (count > MIC_DMA_FRAME_NUM) ? MIC_DMA_FRAME_NUM : count;
    for (size_t i = 0; i < fftCount; ++i) {
        int32_t raw = samples[i] >> 8;
        acSamples[i] = (static_cast<float>(raw) - m_dcOffset) / 8388608.0f;
    }
    computeSpectrum(acSamples, fftCount);

    // Apply manual gain multiplier
    float rawRms = sqrtf(sumSq / static_cast<float>(count)) * m_gain;
    peakVal *= m_gain;

    if (rawRms > 1.0f) rawRms = 1.0f;
    if (peakVal > 1.0f) peakVal = 1.0f;

    m_rawRms = rawRms;
    m_peak = peakVal;

    // Smoothed RMS
    m_smoothRms += (m_rawRms - m_smoothRms) * 0.25f;

    float targetLevel = 0.0f;
    float sensFactor = static_cast<float>(m_sensitivity) / 128.0f;

    if (m_agcEnabled) {
        // 1. Noise floor tracking: adapts down quickly, adapts up very slowly
        if (m_rawRms < m_noiseFloor) {
            m_noiseFloor += (m_rawRms - m_noiseFloor) * 0.05f;
        } else {
            m_noiseFloor += (m_rawRms - m_noiseFloor) * 0.001f;
        }
        // Clamp noise floor to sensible minimum/maximum
        if (m_noiseFloor < 0.0001f) m_noiseFloor = 0.0001f;
        if (m_noiseFloor > 0.05f)   m_noiseFloor = 0.05f;

        // Noise gate silence threshold based on adaptive noise floor
        float noiseGate = m_noiseFloor * 1.35f + 0.0004f;

        if (m_rawRms <= noiseGate) {
            // Silence detected: drop target level to zero
            targetLevel = 0.0f;
        } else {
            // 2. Dynamic peak ceiling tracking (AGC): snaps up on loud sounds, decays slowly
            if (m_rawRms > m_agcMax) {
                m_agcMax += (m_rawRms - m_agcMax) * 0.4f;
            } else {
                // Decay slowly toward nominal ceiling (~0.015f)
                m_agcMax += (0.015f - m_agcMax) * 0.0008f;
            }
            if (m_agcMax < 0.01f) m_agcMax = 0.01f;
            if (m_agcMax > 1.0f)  m_agcMax = 1.0f;

            // 3. Normalized level scaled within dynamic range [noiseGate .. agcMax] with sensitivity
            float dynamicRange = m_agcMax - noiseGate;
            if (dynamicRange < 0.005f) dynamicRange = 0.005f;

            targetLevel = ((m_rawRms - noiseGate) / dynamicRange) * sensFactor;
        }
    } else {
        // Fixed scale without AGC (useful for calibrated metering)
        targetLevel = m_rawRms * 10.0f * sensFactor;
    }

    if (targetLevel < 0.0f) targetLevel = 0.0f;
    if (targetLevel > 1.0f) targetLevel = 1.0f;

    // Ballistics: fast attack, smooth decay
    if (targetLevel > m_smoothLevel) {
        m_smoothLevel = m_smoothLevel * 0.25f + targetLevel * 0.75f;
    } else {
        m_smoothLevel = m_smoothLevel * m_decayFactor + targetLevel * (1.0f - m_decayFactor);
    }
}

static void radix2Fft(float* real, float* imag, int n) {
    int j = 0;
    for (int i = 0; i < n - 1; i++) {
        if (i < j) {
            float tr = real[i]; real[i] = real[j]; real[j] = tr;
            float ti = imag[i]; imag[i] = imag[j]; imag[j] = ti;
        }
        int k = n >> 1;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k;
    }

    for (int len = 2; len <= n; len <<= 1) {
        float angle = -2.0f * 3.14159265f / len;
        float wlen_r = cosf(angle);
        float wlen_i = sinf(angle);
        for (int i = 0; i < n; i += len) {
            float w_r = 1.0f;
            float w_i = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                float u_r = real[i + k];
                float u_i = imag[i + k];
                float v_r = real[i + k + len / 2] * w_r - imag[i + k + len / 2] * w_i;
                float v_i = real[i + k + len / 2] * w_i + imag[i + k + len / 2] * w_r;
                real[i + k] = u_r + v_r;
                imag[i + k] = u_i + v_i;
                real[i + k + len / 2] = u_r - v_r;
                imag[i + k + len / 2] = u_i - v_i;
                float next_w_r = w_r * wlen_r - w_i * wlen_i;
                float next_w_i = w_r * wlen_i + w_i * wlen_r;
                w_r = next_w_r;
                w_i = next_w_i;
            }
        }
    }
}

void MicI2S::computeSpectrum(const float* acSamples, size_t count) {
    constexpr int N = 128;
    int nSamples = (count > N) ? N : count;

    // Apply Hann window and fill real/imag buffers
    for (int i = 0; i < nSamples; i++) {
        float win = 0.5f * (1.0f - cosf(2.0f * 3.14159265f * i / (N - 1)));
        m_fftReal[i] = acSamples[i] * win;
        m_fftImag[i] = 0.0f;
    }
    for (int i = nSamples; i < N; i++) {
        m_fftReal[i] = 0.0f;
        m_fftImag[i] = 0.0f;
    }

    // Run in-place FFT (~15-20 us on ESP32-C6 RISC-V 160MHz)
    radix2Fft(m_fftReal, m_fftImag, N);

    // Frequency bands grouping (16 bands covering ~125 Hz to 8000 Hz)
    static const uint8_t bandStart[NUM_SPECTRUM_BANDS] = { 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 15, 19, 24, 31, 40, 51 };
    static const uint8_t bandEnd[NUM_SPECTRUM_BANDS]   = { 1, 2, 3, 4, 5, 6, 7, 9, 11, 14, 18, 23, 30, 39, 50, 63 };

    float sensFactor = static_cast<float>(m_sensitivity) / 128.0f;
    float noiseGate = m_noiseFloor * 1.35f + 0.0004f;
    bool isSilent = (m_rawRms <= noiseGate);
    float agcGain = (0.025f / fmaxf(0.005f, m_agcMax)) * sensFactor * m_gain * 6.0f;

    for (uint8_t b = 0; b < NUM_SPECTRUM_BANDS; b++) {
        float normBand = 0.0f;
        if (!isSilent) {
            float sumMag = 0.0f;
            int nBins = (bandEnd[b] - bandStart[b] + 1);
            for (int k = bandStart[b]; k <= bandEnd[b]; k++) {
                float mag = sqrtf(m_fftReal[k] * m_fftReal[k] + m_fftImag[k] * m_fftImag[k]) / (N / 2);
                sumMag += mag;
            }
            float avgMag = sumMag / nBins;

            // Equalization curve weighting for high frequency compensation
            float eqWeight = 1.0f + b * 0.15f;
            normBand = avgMag * eqWeight * agcGain;
            if (normBand > 1.0f) normBand = 1.0f;
        }

        // Smooth band ballistics
        m_smoothBands[b] = m_smoothBands[b] * 0.65f + normBand * 0.35f;
    }

    // Musical bass beat detection in low bands (sub-bass / bass: bands 0..2)
    float bass = (m_smoothBands[0] * 1.5f + m_smoothBands[1] * 1.2f + m_smoothBands[2] * 0.8f) / 3.5f;
    m_bassEnergy = bass;

    if (m_bassAverage < 0.001f) {
        m_bassAverage = bass;
    }
    m_bassAverage = m_bassAverage * 0.94f + bass * 0.06f;

    uint32_t now = millis();
    if (bass > (m_bassAverage * 1.45f) && bass > 0.14f && (now - m_lastBeatTime > 200)) {
        m_beatDetected = true;
        m_lastBeatTime = now;
    }
}

void MicI2S::updateDecay(unsigned long currentMs) {
    if (m_lastUpdateMs == 0) {
        m_lastUpdateMs = currentMs;
        return;
    }

    unsigned long elapsed = currentMs - m_lastUpdateMs;
    m_lastUpdateMs = currentMs;

    if (elapsed > 0) {
        // Normalize decay per ~16ms tick (target frame time)
        float ticks = static_cast<float>(elapsed) / 16.0f;
        if (ticks > 10.0f) ticks = 10.0f; // Limit large jumps
        float decayFactorTicks = powf(m_decayFactor, ticks);

        if (m_smoothLevel > 0.0001f) {
            m_smoothLevel *= decayFactorTicks;
            if (m_smoothLevel < 0.001f) {
                m_smoothLevel = 0.0f;
            }
        }

        // Decay spectrum bands
        for (uint8_t b = 0; b < NUM_SPECTRUM_BANDS; b++) {
            if (m_smoothBands[b] > 0.0001f) {
                m_smoothBands[b] *= decayFactorTicks;
                if (m_smoothBands[b] < 0.001f) {
                    m_smoothBands[b] = 0.0f;
                }
            }
        }
    }
}
