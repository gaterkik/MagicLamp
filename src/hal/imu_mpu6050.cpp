#include "imu_mpu6050.h"
#include <Wire.h>
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
      m_useWire(false),
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

void ImuMpu6050::i2cStart() {
    i2cSetHigh(m_activeSda);
    i2cSetHigh(m_activeScl);
    delayMicroseconds(5);
    i2cSetLow(m_activeSda);
    delayMicroseconds(5);
    i2cSetLow(m_activeScl);
    delayMicroseconds(5);
}

void ImuMpu6050::i2cStop() {
    i2cSetLow(m_activeSda);
    delayMicroseconds(5);
    i2cSetHigh(m_activeScl);
    delayMicroseconds(5);
    i2cSetHigh(m_activeSda);
    delayMicroseconds(5);
}

bool ImuMpu6050::i2cWriteByte(uint8_t data) {
    for (uint8_t i = 0; i < 8; i++) {
        if (data & 0x80) {
            i2cSetHigh(m_activeSda);
        } else {
            i2cSetLow(m_activeSda);
        }
        data <<= 1;
        delayMicroseconds(5);
        i2cSetHigh(m_activeScl);
        delayMicroseconds(5);
        i2cSetLow(m_activeScl);
        delayMicroseconds(5);
    }

    // Release SDA and read ACK / NACK from slave on 9th clock pulse
    i2cSetHigh(m_activeSda);
    delayMicroseconds(5);
    i2cSetHigh(m_activeScl);
    delayMicroseconds(5);
    bool ack = (digitalRead(m_activeSda) == LOW);
    i2cSetLow(m_activeScl);
    delayMicroseconds(5);

    return ack;
}

uint8_t ImuMpu6050::i2cReadByte(bool ack) {
    uint8_t data = 0;
    i2cSetHigh(m_activeSda); // Release SDA for slave output

    for (uint8_t i = 0; i < 8; i++) {
        data <<= 1;
        delayMicroseconds(5);
        i2cSetHigh(m_activeScl);
        delayMicroseconds(5);
        if (digitalRead(m_activeSda)) {
            data |= 1;
        }
        i2cSetLow(m_activeScl);
        delayMicroseconds(5);
    }

    // Send Master ACK or NACK
    if (ack) {
        i2cSetLow(m_activeSda);  // Master ACK = LOW
    } else {
        i2cSetHigh(m_activeSda); // Master NACK = HIGH
    }
    delayMicroseconds(5);
    i2cSetHigh(m_activeScl);
    delayMicroseconds(5);
    i2cSetLow(m_activeScl);
    delayMicroseconds(5);
    i2cSetHigh(m_activeSda); // Release SDA

    return data;
}

void ImuMpu6050::recoverBus(uint8_t sda, uint8_t scl) {
    m_activeSda = sda;
    m_activeScl = scl;
    i2cSetHigh(sda);
    i2cSetHigh(scl);
    delayMicroseconds(5);

    // 9 clock pulses on SCL to clear stuck slaves
    for (uint8_t i = 0; i < 9; i++) {
        i2cSetLow(scl);
        delayMicroseconds(5);
        i2cSetHigh(scl);
        delayMicroseconds(5);
    }

    // Issue standard I2C STOP condition
    i2cStop();
    delayMicroseconds(50);
}

bool ImuMpu6050::checkLinesPullup(uint8_t sda, uint8_t scl) {
    pinMode(sda, INPUT_PULLDOWN);
    pinMode(scl, INPUT_PULLDOWN);
    delayMicroseconds(50);
    int sdaLvl = digitalRead(sda);
    int sclLvl = digitalRead(scl);
    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, INPUT_PULLUP);
    delayMicroseconds(10);

    if (sdaLvl == LOW || sclLvl == LOW) {
        snprintf(m_statusStr, sizeof(m_statusStr), "Check SDA/SCL connection");
        return false;
    }
    return true;
}

bool ImuMpu6050::initSensor(uint8_t addr, uint8_t sda, uint8_t scl) {
    m_i2cAddr = addr;
    m_activeSda = sda;
    m_activeScl = scl;

    // Read WHO_AM_I register (0x75)
    uint8_t who = 0;
    i2cStart();
    if (i2cWriteByte((addr << 1) | 0)) {
        if (i2cWriteByte(REG_WHO_AM_I)) {
            i2cStart(); // Repeated start
            if (i2cWriteByte((addr << 1) | 1)) {
                who = i2cReadByte(false); // NACK on last byte
            }
        }
    }
    i2cStop();
    m_whoAmI = who;

    // Wake up sensor: write 0x00 to PWR_MGMT_1 (0x6B)
    i2cStart();
    if (i2cWriteByte((addr << 1) | 0)) {
        i2cWriteByte(REG_PWR_MGMT_1);
        i2cWriteByte(0x00);
    }
    i2cStop();
    delayMicroseconds(5000);

    // Configure DLPF (~44 Hz bandwidth: CONFIG = 0x03)
    i2cStart();
    if (i2cWriteByte((addr << 1) | 0)) {
        i2cWriteByte(REG_CONFIG);
        i2cWriteByte(0x03);
    }
    i2cStop();

    // Configure Accelerometer Range (+/- 2g: ACCEL_CONFIG = 0x00, 16384 LSB/g)
    i2cStart();
    if (i2cWriteByte((addr << 1) | 0)) {
        i2cWriteByte(REG_ACCEL_CONFIG);
        i2cWriteByte(0x00);
    }
    i2cStop();

    // Verify acceleration read
    int16_t ax = 0, ay = 0, az = 0;
    if (readRawData(ax, ay, az)) {
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
    }

    m_fallback = false;
    m_consecutiveErrors = 0;

    snprintf(m_statusStr, sizeof(m_statusStr),
             "Online (addr:0x%02X, who:0x%02X, SDA:%d, SCL:%d)",
             m_i2cAddr, m_whoAmI, m_activeSda, m_activeScl);
    Serial.printf("[IMU] Sensor detected: %s\n", m_statusStr);
    return true;
}

bool ImuMpu6050::probeAddresses(uint8_t sda, uint8_t scl, const uint8_t* addrs, size_t count) {
    m_activeSda = sda;
    m_activeScl = scl;

    if (!checkLinesPullup(sda, scl)) {
        return false;
    }

    for (size_t i = 0; i < count; i++) {
        uint8_t addr = addrs[i];
        i2cStart();
        bool ack = i2cWriteByte((addr << 1) | 0);
        i2cStop();
        if (ack) {
            return initSensor(addr, sda, scl);
        }
    }
    return false;
}

bool ImuMpu6050::quickProbe() {
    static const uint8_t MPU_ADDRS[] = { 0x68, 0x69 };
    // Probe primary pair first
    if (probeAddresses(m_sdaPin, m_sclPin, MPU_ADDRS, 2)) {
        return true;
    }
    // Probe mirror pair
    if (probeAddresses(m_sclPin, m_sdaPin, MPU_ADDRS, 2)) {
        return true;
    }
    return false;
}

bool ImuMpu6050::scanPair(uint8_t sda, uint8_t scl) {
    m_activeSda = sda;
    m_activeScl = scl;
    recoverBus(sda, scl);

    if (!checkLinesPullup(sda, scl)) {
        return false;
    }

    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        i2cStart();
        bool ack = i2cWriteByte((addr << 1) | 0); // Write address + R/W=0
        i2cStop();

        if (ack) {
            return initSensor(addr, sda, scl);
        }
    }

    return false;
}

bool ImuMpu6050::scanPins() {
    // 1. Line check with pulldown test
    if (!checkLinesPullup(m_sdaPin, m_sclPin)) {
        Serial.printf("[IMU] %s. Activating Graceful Fallback mode.\n", m_statusStr);
        m_fallback = true;
        updateFallback(millis());
        return false;
    }

    // 2. Scan primary pair (SDA=m_sdaPin, SCL=m_sclPin) across 0x08..0x77
    if (scanPair(m_sdaPin, m_sclPin)) {
        return true;
    }

    // 3. Scan mirror pair (SDA=m_sclPin, SCL=m_sdaPin) across 0x08..0x77
    if (scanPair(m_sclPin, m_sdaPin)) {
        return true;
    }

    // 4. Lines have pullup but no chip responded
    snprintf(m_statusStr, sizeof(m_statusStr), "No I2C ACK on 5/6 (SDA:HIGH, SCL:HIGH)");
    Serial.printf("[IMU] %s. Activating Graceful Fallback mode.\n", m_statusStr);

    m_fallback = true;
    updateFallback(millis());
    return false;
}

bool ImuMpu6050::begin() {
    m_lastAutoScanTime = millis();

    // 1. Primary attempt: pure hardware Wire.begin(m_sdaPin, m_sclPin, 100000) WITHOUT pinMode()
    Serial.printf("[IMU] Hardware Wire probe on SDA=%d, SCL=%d (100 kHz)...\n", m_sdaPin, m_sclPin);
    Wire.begin(m_sdaPin, m_sclPin, 100000);
    delay(10);

    bool wireFound = false;
    uint8_t detectedAddr = 0;
    static const uint8_t candidateAddrs[] = { 0x68, 0x69 };
    for (uint8_t addr : candidateAddrs) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Wire.beginTransmission(addr);
            Wire.write(REG_WHO_AM_I);
            if (Wire.endTransmission(false) == 0) {
                if (Wire.requestFrom((uint8_t)addr, (size_t)1) == 1) {
                    uint8_t who = Wire.read();
                    detectedAddr = addr;
                    m_whoAmI = who;
                    wireFound = true;
                    break;
                }
            }
        }
    }

    if (wireFound) {
        m_i2cAddr = detectedAddr;
        m_activeSda = m_sdaPin;
        m_activeScl = m_sclPin;
        m_useWire = true;
        m_fallback = false;
        m_consecutiveErrors = 0;

        // Wake up sensor: write 0x00 to REG_PWR_MGMT_1
        Wire.beginTransmission(m_i2cAddr);
        Wire.write(REG_PWR_MGMT_1);
        Wire.write(0x00);
        Wire.endTransmission();
        delay(5);

        // Configure DLPF (~44 Hz: CONFIG = 0x03)
        Wire.beginTransmission(m_i2cAddr);
        Wire.write(REG_CONFIG);
        Wire.write(0x03);
        Wire.endTransmission();

        // Configure Accel Range (+/- 2g: ACCEL_CONFIG = 0x00)
        Wire.beginTransmission(m_i2cAddr);
        Wire.write(REG_ACCEL_CONFIG);
        Wire.write(0x00);
        Wire.endTransmission();

        // Initial read
        int16_t ax = 0, ay = 0, az = 0;
        if (readRawData(ax, ay, az)) {
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
        }

        snprintf(m_statusStr, sizeof(m_statusStr),
                 "Online (Hardware Wire, addr:0x%02X, who:0x%02X, SDA:%d, SCL:%d)",
                 m_i2cAddr, m_whoAmI, m_activeSda, m_activeScl);
        Serial.printf("[IMU] %s\n", m_statusStr);
        return true;
    }

    // No response on Hardware Wire: release the bus and pins cleanly
    Serial.println(F("[IMU] No ACK on Hardware Wire. Calling Wire.end() and switching to Open-Drain bit-bang..."));
    Wire.end();
    delay(10);
    m_useWire = false;

    // 2. Reserve Open-Drain Bit-Bang scanner
    if (quickProbe()) {
        return true;
    }
    return scanPins();
}

bool ImuMpu6050::readRawData(int16_t& ax, int16_t& ay, int16_t& az) {
    if (m_useWire) {
        Wire.beginTransmission(m_i2cAddr);
        Wire.write(REG_ACCEL_XOUT_H);
        if (Wire.endTransmission(false) != 0) {
            return false;
        }
        if (Wire.requestFrom((uint8_t)m_i2cAddr, (size_t)6) != 6) {
            return false;
        }
        int16_t xh = Wire.read();
        int16_t xl = Wire.read();
        int16_t yh = Wire.read();
        int16_t yl = Wire.read();
        int16_t zh = Wire.read();
        int16_t zl = Wire.read();
        ax = static_cast<int16_t>((xh << 8) | xl);
        ay = static_cast<int16_t>((yh << 8) | yl);
        az = static_cast<int16_t>((zh << 8) | zl);
        return true;
    }

    i2cStart();
    if (!i2cWriteByte((m_i2cAddr << 1) | 0)) {
        i2cStop();
        return false;
    }
    if (!i2cWriteByte(REG_ACCEL_XOUT_H)) {
        i2cStop();
        return false;
    }
    i2cStart(); // Repeated start
    if (!i2cWriteByte((m_i2cAddr << 1) | 1)) {
        i2cStop();
        return false;
    }

    uint8_t xh = i2cReadByte(true);
    uint8_t xl = i2cReadByte(true);
    uint8_t yh = i2cReadByte(true);
    uint8_t yl = i2cReadByte(true);
    uint8_t zh = i2cReadByte(true);
    uint8_t zl = i2cReadByte(false); // NACK on last byte
    i2cStop();

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
        // Periodic background auto-scan every 5 seconds if sensor is offline (probes only 0x68/0x69 in < 0.3 ms)
        if (now - m_lastAutoScanTime >= 5000) {
            m_lastAutoScanTime = now;
            if (quickProbe()) {
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
            if (m_useWire) {
                Wire.end();
                m_useWire = false;
            }
            m_fallback = true;
            quickProbe();
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
    // Fixed stable gravity vector pointing strictly down (0, -1, 0)
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

String ImuMpu6050::runDiagnostics() {
    String out = "{";
    out += "\"status\":\"" + String(m_statusStr) + "\",";
    out += "\"connected\":" + String(isConnected() ? "true" : "false") + ",";
    out += "\"fallback\":" + String(m_fallback ? "true" : "false") + ",";
    out += "\"who_am_i\":\"0x" + String(m_whoAmI, HEX) + "\",";
    out += "\"active_sda\":" + String(m_activeSda) + ",";
    out += "\"active_scl\":" + String(m_activeScl) + ",";
    out += "\"gravity\":{\"x\":" + String(m_gx, 3) + ",\"y\":" + String(m_gy, 3) + ",\"z\":" + String(m_gz, 3) + "},";

    // Test Hardware Wire 100kHz
    if (m_useWire) Wire.end();
    delay(5);
    Wire.begin(m_sdaPin, m_sclPin, 100000);
    delay(10);
    String addrs100k = "[";
    bool first = true;
    for (uint8_t a = 0x08; a <= 0x77; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            if (!first) addrs100k += ",";
            addrs100k += "\"0x" + String(a, HEX) + "\"";
            first = false;
        }
    }
    addrs100k += "]";
    out += "\"wire_100kHz_devices\":" + addrs100k + ",";

    // Test Hardware Wire 10kHz
    Wire.setClock(10000);
    delay(10);
    String addrs10k = "[";
    first = true;
    for (uint8_t a = 0x08; a <= 0x77; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            if (!first) addrs10k += ",";
            addrs10k += "\"0x" + String(a, HEX) + "\"";
            first = false;
        }
    }
    addrs10k += "]";
    out += "\"wire_10kHz_devices\":" + addrs10k + ",";

    Wire.end();
    delay(5);

    // Line check with pulldown test
    pinMode(m_sdaPin, INPUT_PULLDOWN);
    pinMode(m_sclPin, INPUT_PULLDOWN);
    delayMicroseconds(50);
    bool sdaExtPullup = (digitalRead(m_sdaPin) == HIGH);
    bool sclExtPullup = (digitalRead(m_sclPin) == HIGH);
    pinMode(m_sdaPin, INPUT_PULLUP);
    pinMode(m_sclPin, INPUT_PULLUP);
    out += "\"sda_ext_pullup\":" + String(sdaExtPullup ? "true" : "false") + ",";
    out += "\"scl_ext_pullup\":" + String(sclExtPullup ? "true" : "false");

    out += "}";

    // Re-initialize driver to normal operating state
    begin();

    return out;
}

