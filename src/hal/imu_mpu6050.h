#pragma once

#include <Arduino.h>
#include "config.h"

#ifndef MPU6050_I2C_ADDR
#define MPU6050_I2C_ADDR 0x68
#endif

#ifndef IMU_SAMPLE_INTERVAL_MS
#define IMU_SAMPLE_INTERVAL_MS 25
#endif

#ifndef IMU_SHAKE_THRESHOLD
#define IMU_SHAKE_THRESHOLD 1.8f
#endif

/**
 * @file imu_mpu6050.h
 * @brief Software Open-Drain Bit-Bang I2C driver for GY-521 (MPU-6050) with graceful fallback.
 * 
 * Pins: SDA = GPIO 6, SCL = GPIO 5 (default).
 * Fully independent of Arduino <Wire.h> to prevent ESP32-C6 pin manager conflicts.
 * 
 * Features:
 * - Software Open-Drain Bit-Bang I2C with 5 µs half-period (100 kHz).
 * - Full address scan (0x08..0x77) on (6,5) and mirror (5,6).
 * - Hardware detection via WHO_AM_I register (0x75).
 * - Graceful fallback mode (0, -1, 0) if sensor is absent or disconnected.
 * - Non-blocking accelerometer polling every 20-50 ms.
 * - Smoothed gravity vector (gx, gy, gz) via low-pass filtering.
 * - Shake gesture recognition (isShaken()).
 */
class ImuMpu6050 {
public:
    /**
     * @brief Construct a new ImuMpu6050 driver instance.
     * @param sdaPin GPIO pin for I2C SDA (default PIN_I2C_SDA = 6).
     * @param sclPin GPIO pin for I2C SCL (default PIN_I2C_SCL = 5).
     * @param i2cAddr I2C address (default 0x68).
     * @param sampleIntervalMs Polling period in ms (default 25 ms / 40 Hz).
     * @param shakeThreshold Acceleration threshold in G for shake detection (default 1.8 G).
     */
    ImuMpu6050(uint8_t sdaPin = PIN_I2C_SDA,
               uint8_t sclPin = PIN_I2C_SCL,
               uint8_t i2cAddr = MPU6050_I2C_ADDR,
               uint32_t sampleIntervalMs = IMU_SAMPLE_INTERVAL_MS,
               float shakeThreshold = IMU_SHAKE_THRESHOLD);

    /**
     * @brief Initialize Bit-Bang I2C and scan for GY-521.
     * If the sensor is not found, fallback mode is engaged.
     * @return true if hardware was detected and initialized, false if fallback mode engaged.
     */
    bool begin();

    /**
     * @brief Non-blocking periodic update. Must be called in loop().
     */
    void update();

    /**
     * @brief Check whether driver is in graceful fallback simulation mode.
     * @return true if in fallback mode, false if hardware sensor is connected.
     */
    bool isFallback() const { return m_fallback; }

    /**
     * @brief Check whether physical hardware sensor is connected and responding.
     * @return true if hardware sensor is active, false if fallback.
     */
    bool isConnected() const { return !m_fallback; }

    /**
     * @brief Check if a shake gesture was detected, and clear the flag.
     * @return true if a shake occurred since last call.
     */
    bool isShaken();

    /**
     * @brief Get smoothed gravity vector X component (in G, -2.0 .. +2.0).
     */
    float getGx() const { return m_gx; }

    /**
     * @brief Get smoothed gravity vector Y component (in G, -2.0 .. +2.0).
     */
    float getGy() const { return m_gy; }

    /**
     * @brief Get smoothed gravity vector Z component (in G, -2.0 .. +2.0).
     */
    float getGz() const { return m_gz; }

    /**
     * @brief Get all three gravity vector components.
     * @param x Reference to store X.
     * @param y Reference to store Y.
     * @param z Reference to store Z.
     */
    void getGravity(float& x, float& y, float& z) const {
        x = m_gx;
        y = m_gy;
        z = m_gz;
    }

    /**
     * @brief Get instantaneous raw acceleration X component (in G).
     */
    float getRawAx() const { return m_rawAx; }

    /**
     * @brief Get instantaneous raw acceleration Y component (in G).
     */
    float getRawAy() const { return m_rawAy; }

    /**
     * @brief Get instantaneous raw acceleration Z component (in G).
     */
    float getRawAz() const { return m_rawAz; }

    /**
     * @brief Manually trigger a shake event (for testing or external controls).
     */
    void triggerShake();

    /**
     * @brief Get detailed diagnostic status string:
     * "Online (addr:0x..., who:0x..., SDA:..., SCL:...)" or
     * "No I2C ACK on 5/6 (SDA:HIGH, SCL:HIGH)".
     */
    const char* getStatus() const { return m_statusStr; }
    const char* getStatusString() const { return m_statusStr; }

    /**
     * @brief Get WHO_AM_I response byte from sensor.
     */
    uint8_t getWhoAmI() const { return m_whoAmI; }

    /**
     * @brief Run comprehensive hardware I2C diagnostics on 100 kHz and 10 kHz.
     * @return JSON string with diagnostics report.
     */
    String runDiagnostics();

private:
    uint8_t m_sdaPin;
    uint8_t m_sclPin;
    uint8_t m_activeSda;
    uint8_t m_activeScl;
    uint8_t m_i2cAddr;
    uint8_t m_whoAmI;
    uint32_t m_sampleIntervalMs;
    float m_shakeThreshold;

    bool m_useWire;
    bool m_fallback;
    uint8_t m_consecutiveErrors;
    uint32_t m_lastSampleTime;
    uint32_t m_lastAutoScanTime;
    uint32_t m_lastShakeTime;
    bool m_shaken;
    char m_statusStr[96];

    float m_gx;
    float m_gy;
    float m_gz;

    float m_rawAx;
    float m_rawAy;
    float m_rawAz;

    // Bit-Bang Open-Drain I2C primitives
    inline void i2cSetHigh(uint8_t pin) {
        pinMode(pin, INPUT_PULLUP);
    }
    inline void i2cSetLow(uint8_t pin) {
        digitalWrite(pin, LOW);
        pinMode(pin, OUTPUT);
    }
    void i2cStart();
    void i2cStop();
    bool i2cWriteByte(uint8_t data);
    uint8_t i2cReadByte(bool ack);

    void recoverBus(uint8_t sda, uint8_t scl);
    bool checkLinesPullup(uint8_t sda, uint8_t scl);
    bool initSensor(uint8_t addr, uint8_t sda, uint8_t scl);
    bool probeAddresses(uint8_t sda, uint8_t scl, const uint8_t* addrs, size_t count);
    bool quickProbe();
    bool scanPair(uint8_t sda, uint8_t scl);
    bool scanPins();
    bool readRawData(int16_t& ax, int16_t& ay, int16_t& az);
    void updateFallback(uint32_t now);
};
