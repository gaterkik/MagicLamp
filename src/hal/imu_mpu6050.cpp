#include "imu_mpu6050.h"
#include <math.h>

// MPU-6050 Register Map
static constexpr uint8_t REG_CONFIG        = 0x1A;
static constexpr uint8_t REG_ACCEL_CONFIG  = 0x1C;
static constexpr uint8_t REG_ACCEL_XOUT_H  = 0x3B;
static constexpr uint8_t REG_PWR_MGMT_1    = 0x6B;
static constexpr uint8_t REG_WHO_AM_I      = 0x75;

ImuMpu6050::ImuMpu6050(uint8_t sdaPin,
                       uint8_t sclPin,
                       uint8_t i2cAddr,
                       uint32_t sampleIntervalMs,
                       float shakeThreshold)
    : m_sdaPin(sdaPin),
      m_sclPin(sclPin),
      m_activeSda(sdaPin),
      m_activeScl(sclPin),
      m_i2cAddr(i2cAddr),
      m_whoAmI(0),
      m_sampleIntervalMs(sampleIntervalMs),
      m_shakeThreshold(shakeThreshold),
      m_fallback(true),
      m_consecutiveErrors(0),
      m_lastSampleTime(0),
      m_lastAutoScanTime(0),
      m_lastShakeTime(0),
      m_shaken(false),
      m_gx(0.0f),
      m_gy(-1.0f),
      m_gz(0.0f),
      m_rawAx(0.0f),
      m_rawAy(-1.0f),
      m_rawAz(0.0f) {
    strncpy(m_statusStr, "Initializing...", sizeof(m_statusStr));
}

bool ImuMpu6050::checkElectricalLevels(uint8_t sda, uint8_t scl, int& sdaLevel, int& sclLevel) {
    Wire.end();
    pinMode(sda, INPUT);
    pinMode(scl, INPUT);
    delayMicroseconds(50);
    sdaLevel = digitalRead(sda);
    sclLevel = digitalRead(scl);
    return (sdaLevel == HIGH && sclLevel == HIGH);
}

bool ImuMpu6050::scanPins() {
    // 1. Electrical diagnostic on primary pins (SDA=6, SCL=5)
    int sdaLvl = 0, sclLvl = 0;
    bool pullupOk = checkElectricalLevels(m_sdaPin, m_sclPin, sdaLvl, sclLvl);

    Serial.printf("[IMU] Electrical check on primary pins SDA=%d (%s), SCL=%d (%s)\n",
                  m_sdaPin, sdaLvl == HIGH ? "HIGH" : "LOW",
                  m_sclPin, sclLvl == HIGH ? "HIGH" : "LOW");

    if (!pullupOk) {
        snprintf(m_statusStr, sizeof(m_statusStr),
                 "No pull-up / Check 3.3V on GY-521 (SDA:%s, SCL:%s)",
                 sdaLvl == HIGH ? "HIGH" : "LOW",
                 sclLvl == HIGH ? "HIGH" : "LOW");
        Serial.printf("[IMU] DIAGNOSTIC: %s\n", m_statusStr);
        m_fallback = true;
        updateFallback(millis());
        return false;
    }

    // 2. Levels are HIGH: Probe standard pair (SDA=6, SCL=5)
    Serial.printf("[IMU] Probing standard pair SDA=%d, SCL=%d (100kHz)...\n", m_sdaPin, m_sclPin);
    if (tryInitOnPins(m_sdaPin, m_sclPin)) {
        return true;
    }

    // 3. Probe mirror pair (SDA=5, SCL=6)
    uint8_t mirrorSda = m_sclPin;
    uint8_t mirrorScl = m_sdaPin;
    Serial.printf("[IMU] Not detected on standard pair. Probing mirror pair SDA=%d, SCL=%d...\n",
                  mirrorSda, mirrorScl);
    if (tryInitOnPins(mirrorSda, mirrorScl)) {
        return true;
    }

    // 4. Auto-search across free GPIOs: 4, 5, 6, 8, 9
    Serial.println(F("[IMU] Sensor not responding on 6/5. Scanning GPIO 4, 5, 6, 8, 9..."));
    const uint8_t candidatePins[] = {4, 5, 6, 8, 9};
    const size_t numPins = sizeof(candidatePins) / sizeof(candidatePins[0]);

    for (size_t i = 0; i < numPins; i++) {
        for (size_t j = 0; j < numPins; j++) {
            if (i == j) continue;
            uint8_t sda = candidatePins[i];
            uint8_t scl = candidatePins[j];

            // Already checked primary and mirror
            if ((sda == m_sdaPin && scl == m_sclPin) ||
                (sda == mirrorSda && scl == mirrorScl)) {
                continue;
            }

            int pSdaLvl = 0, pSclLvl = 0;
            if (checkElectricalLevels(sda, scl, pSdaLvl, pSclLvl)) {
                if (tryInitOnPins(sda, scl)) {
                    Serial.printf("[IMU] Sensor discovered on candidate pins: %s\n", m_statusStr);
                    return true;
                }
            }
        }
    }

    // 5. Not found on any combination of candidate pins
    snprintf(m_statusStr, sizeof(m_statusStr), "Not Found (SDA/SCL HIGH, no ACK on GPIO 4,5,6,8,9)");
    Serial.printf("[IMU] WARNING: %s. Activating Graceful Fallback mode.\n", m_statusStr);
    m_fallback = true;
    updateFallback(millis());
    return false;
}

bool ImuMpu6050::begin() {
    m_lastAutoScanTime = millis();
    return scanPins();
}

bool ImuMpu6050::tryInitOnPins(uint8_t sda, uint8_t scl) {
    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, INPUT_PULLUP);

    Wire.begin(sda, scl, 100000);
    Wire.setTimeOut(20);

    const uint8_t candidateAddrs[] = {0x68, 0x69};
    bool ackReceived = false;
    uint8_t detectedAddr = 0;

    for (uint8_t addr : candidateAddrs) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            detectedAddr = addr;
            ackReceived = true;
            break;
        }
    }

    if (!ackReceived) {
        Wire.end();
        return false;
    }

    // Read WHO_AM_I register (0x75) for diagnostics
    uint8_t whoami = 0;
    Wire.beginTransmission(detectedAddr);
    Wire.write(REG_WHO_AM_I);
    if (Wire.endTransmission(false) == 0) {
        if (Wire.requestFrom(static_cast<uint8_t>(detectedAddr), static_cast<size_t>(1)) == 1) {
            whoami = Wire.read();
        }
    }

    // Wake up device: write 0x00 to PWR_MGMT_1 (0x6B)
    Wire.beginTransmission(detectedAddr);
    Wire.write(REG_PWR_MGMT_1);
    Wire.write(0x00);
    if (Wire.endTransmission() != 0) {
        Wire.end();
        return false;
    }

    delayMicroseconds(5000);

    // Configure DLPF: ~44 Hz bandwidth (CONFIG = 0x03)
    Wire.beginTransmission(detectedAddr);
    Wire.write(REG_CONFIG);
    Wire.write(0x03);
    if (Wire.endTransmission() != 0) {
        Wire.end();
        return false;
    }

    // Configure Accelerometer Full Scale Range: +/- 2g (ACCEL_CONFIG = 0x00, 16384 LSB/g)
    Wire.beginTransmission(detectedAddr);
    Wire.write(REG_ACCEL_CONFIG);
    Wire.write(0x00);
    if (Wire.endTransmission() != 0) {
        Wire.end();
        return false;
    }

    // Test reading raw data to confirm healthy communications
    int16_t ax = 0, ay = 0, az = 0;
    Wire.beginTransmission(detectedAddr);
    Wire.write(REG_ACCEL_XOUT_H);
    if (Wire.endTransmission(false) != 0) {
        Wire.end();
        return false;
    }

    if (Wire.requestFrom(static_cast<uint8_t>(detectedAddr), static_cast<size_t>(6)) != 6) {
        Wire.end();
        return false;
    }

    uint8_t xh = Wire.read();
    uint8_t xl = Wire.read();
    uint8_t yh = Wire.read();
    uint8_t yl = Wire.read();
    uint8_t zh = Wire.read();
    uint8_t zl = Wire.read();

    ax = static_cast<int16_t>((xh << 8) | xl);
    ay = static_cast<int16_t>((yh << 8) | yl);
    az = static_cast<int16_t>((zh << 8) | zl);

    m_activeSda = sda;
    m_activeScl = scl;
    m_i2cAddr = detectedAddr;
    m_whoAmI = whoami;
    m_fallback = false;
    m_consecutiveErrors = 0;

    m_rawAx = static_cast<float>(ax) / 16384.0f;
    m_rawAy = static_cast<float>(ay) / 16384.0f;
    m_rawAz = static_cast<float>(az) / 16384.0f;
    m_gx = m_rawAx;
    m_gy = m_rawAy;
    m_gz = m_rawAz;
    float norm = sqrtf(m_gx * m_gx + m_gy * m_gy + m_gz * m_gz);
    if (norm > 0.001f) {
        m_gx /= norm;
        m_gy /= norm;
        m_gz /= norm;
    } else {
        m_gx = 0.0f;
        m_gy = -1.0f;
        m_gz = 0.0f;
    }

    snprintf(m_statusStr, sizeof(m_statusStr), "Online (SDA:%d, SCL:%d, addr:0x%02X, who:0x%02X)",
             m_activeSda, m_activeScl, m_i2cAddr, m_whoAmI);
    Serial.printf("[IMU] Sensor detected: %s\n", m_statusStr);
    return true;
}

bool ImuMpu6050::readRawData(int16_t& ax, int16_t& ay, int16_t& az) {
    Wire.beginTransmission(m_i2cAddr);
    Wire.write(REG_ACCEL_XOUT_H);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    if (Wire.requestFrom(static_cast<uint8_t>(m_i2cAddr), static_cast<size_t>(6)) != 6) {
        return false;
    }

    uint8_t xh = Wire.read();
    uint8_t xl = Wire.read();
    uint8_t yh = Wire.read();
    uint8_t yl = Wire.read();
    uint8_t zh = Wire.read();
    uint8_t zl = Wire.read();

    ax = static_cast<int16_t>((xh << 8) | xl);
    ay = static_cast<int16_t>((yh << 8) | yl);
    az = static_cast<int16_t>((zh << 8) | zl);

    return true;
}

void ImuMpu6050::update() {
    uint32_t now = millis();
    if (now - m_lastSampleTime < m_sampleIntervalMs) {
        return;
    }
    m_lastSampleTime = now;

    if (m_fallback) {
        updateFallback(now);
        // Periodic background auto-scan every 5 seconds if sensor is offline
        if (now - m_lastAutoScanTime >= 5000) {
            m_lastAutoScanTime = now;
            if (scanPins()) {
                Serial.printf("[IMU] Sensor recovered during background scan: %s\n", m_statusStr);
            }
        }
        return;
    }

    int16_t rawX = 0, rawY = 0, rawZ = 0;
    if (!readRawData(rawX, rawY, rawZ)) {
        m_consecutiveErrors++;
        if (m_consecutiveErrors >= 5) {
            Serial.println(F("[IMU] WARNING: Too many I2C read errors. Switching to Graceful Fallback mode."));
            m_fallback = true;
            Wire.end();
            scanPins();
            m_lastAutoScanTime = now;
            updateFallback(now);
        }
        return;
    }

    m_consecutiveErrors = 0;

    // Convert raw 16-bit counts to G units (+/- 2g scale -> 16384 LSB/g)
    m_rawAx = static_cast<float>(rawX) / 16384.0f;
    m_rawAy = static_cast<float>(rawY) / 16384.0f;
    m_rawAz = static_cast<float>(rawZ) / 16384.0f;

    // Exponential Moving Average (EMA) low-pass filter
    constexpr float alpha = 0.20f;
    float filteredX = m_gx * (1.0f - alpha) + m_rawAx * alpha;
    float filteredY = m_gy * (1.0f - alpha) + m_rawAy * alpha;
    float filteredZ = m_gz * (1.0f - alpha) + m_rawAz * alpha;

    // Normalized gravity vector calculation
    float norm = sqrtf(filteredX * filteredX + filteredY * filteredY + filteredZ * filteredZ);
    if (norm > 0.001f) {
        m_gx = filteredX / norm;
        m_gy = filteredY / norm;
        m_gz = filteredZ / norm;
    } else {
        m_gx = 0.0f;
        m_gy = -1.0f;
        m_gz = 0.0f;
    }

    // Shake gesture detection via high-frequency jerk and total acceleration magnitude
    float hx = m_rawAx - filteredX;
    float hy = m_rawAy - filteredY;
    float hz = m_rawAz - filteredZ;
    float highPassMag = sqrtf(hx * hx + hy * hy + hz * hz);
    float totalMag = sqrtf(m_rawAx * m_rawAx + m_rawAy * m_rawAy + m_rawAz * m_rawAz);

    if (highPassMag > 0.85f || totalMag > m_shakeThreshold) {
        if (now - m_lastShakeTime > 350) {
            m_shaken = true;
            m_lastShakeTime = now;
        }
    }
}

void ImuMpu6050::updateFallback(uint32_t /*now*/) {
    // Fixed stable gravity vector pointing strictly down without self-rotation
    m_gx = 0.0f;
    m_gy = -1.0f;
    m_gz = 0.0f;

    m_rawAx = 0.0f;
    m_rawAy = -1.0f;
    m_rawAz = 0.0f;
}

bool ImuMpu6050::isShaken() {
    bool shaken = m_shaken;
    m_shaken = false;
    return shaken;
}

void ImuMpu6050::triggerShake() {
    m_shaken = true;
    m_lastShakeTime = millis();
}
