#pragma once

#include <Arduino.h>
#include <Wire.h>
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
 * @brief Non-blocking I2C driver for GY-521 (MPU-6050) with graceful fallback.
 * 
 * Pins: SDA = GPIO 6, SCL = GPIO 5 (400 kHz fast I2C).
 * I2C address: 0x68 (MPU6050_I2C_ADDR).
 * 
 * Features:
 * - Hardware detection via WHO_AM_I register (0x75 == 0x68).
 * - Graceful fallback mode if sensor is absent or disconnected.
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
     * @brief Initialize Wire I2C bus and MPU-6050 registers.
     * If the sensor is not found or fails to respond, fallback mode is engaged.
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
     * "Online (SDA:..., SCL:..., addr:0x..., who:0x...)",
     * "No pull-up / Check 3.3V on GY-521 (SDA:LOW, SCL:HIGH)", or
     * "Not Found (SDA/SCL HIGH, no ACK on GPIO 4,5,6,8,9)".
     */
    const char* getStatus() const { return m_statusStr; }
    const char* getStatusString() const { return m_statusStr; }

    /**
     * @brief Get WHO_AM_I response byte from sensor.
     */
    uint8_t getWhoAmI() const { return m_whoAmI; }

private:
    uint8_t m_sdaPin;
    uint8_t m_sclPin;
    uint8_t m_activeSda;
    uint8_t m_activeScl;
    uint8_t m_i2cAddr;
    uint8_t m_whoAmI;
    uint32_t m_sampleIntervalMs;
    float m_shakeThreshold;

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

    void recoverBus(uint8_t sda, uint8_t scl);
    bool scanPins();
    bool tryInitOnPins(uint8_t sda, uint8_t scl);
    bool readRawData(int16_t& ax, int16_t& ay, int16_t& az);
    void updateFallback(uint32_t now);
};
