#include "imu_mpu6050.h"
#include <math.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_rom_sys.h>

// MPU-6050 / MPU-6500 (GY-6500) Register Map
static constexpr uint8_t REG_CONFIG        = 0x1A;
static constexpr uint8_t REG_ACCEL_CONFIG  = 0x1C;
static constexpr uint8_t REG_ACCEL_XOUT_H  = 0x3B;
static constexpr uint8_t REG_PWR_MGMT_1    = 0x6B;
static constexpr uint8_t REG_PWR_MGMT_2    = 0x6C;
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

void ImuMpu6050::i2cSclHigh() {
    i2cSetHigh(m_activeScl);
    int64_t startUs = esp_timer_get_time();
    while (digitalRead(m_activeScl) == LOW) {
        if ((esp_timer_get_time() - startUs) > 500) {
            break; // Clock stretching timeout 500 us
        }
        esp_rom_delay_us(1);
    }
}

void ImuMpu6050::i2cStart() {
    i2cSetHigh(m_activeSda);
    i2cSclHigh();
    delayMicroseconds(10);
    i2cSetLow(m_activeSda);
    delayMicroseconds(10);
    i2cSetLow(m_activeScl);
    delayMicroseconds(10);
}

void ImuMpu6050::i2cStop() {
    i2cSetLow(m_activeSda);
    delayMicroseconds(10);
    i2cSclHigh();
    delayMicroseconds(10);
    i2cSetHigh(m_activeSda);
    delayMicroseconds(10);
}

bool ImuMpu6050::i2cWriteByte(uint8_t data) {
    for (uint8_t i = 0; i < 8; i++) {
        if (data & 0x80) {
            i2cSetHigh(m_activeSda);
        } else {
            i2cSetLow(m_activeSda);
        }
        data <<= 1;
        delayMicroseconds(10);
        i2cSclHigh();
        delayMicroseconds(10);
        i2cSetLow(m_activeScl);
        delayMicroseconds(10);
    }

    // Release SDA and read ACK / NACK from slave on 9th clock pulse
    i2cSetHigh(m_activeSda);
    delayMicroseconds(10);
    i2cSclHigh();
    delayMicroseconds(10);
    bool ack = (digitalRead(m_activeSda) == LOW);
    i2cSetLow(m_activeScl);
    delayMicroseconds(10);

    return ack;
}

uint8_t ImuMpu6050::i2cReadByte(bool ack) {
    uint8_t data = 0;
    i2cSetHigh(m_activeSda); // Release SDA for slave output

    for (uint8_t i = 0; i < 8; i++) {
        data <<= 1;
        delayMicroseconds(10);
        i2cSclHigh();
        delayMicroseconds(10);
        if (digitalRead(m_activeSda)) {
            data |= 1;
        }
        i2cSetLow(m_activeScl);
        delayMicroseconds(10);
    }

    // Send Master ACK or NACK
    if (ack) {
        i2cSetLow(m_activeSda);  // Master ACK = LOW
    } else {
        i2cSetHigh(m_activeSda); // Master NACK = HIGH
    }
    delayMicroseconds(10);
    i2cSclHigh();
    delayMicroseconds(10);
    i2cSetLow(m_activeScl);
    delayMicroseconds(10);
    i2cSetHigh(m_activeSda); // Release SDA

    return data;
}

bool ImuMpu6050::writeRegister(uint8_t reg, uint8_t val) {
    i2cStart();
    if (!i2cWriteByte((m_i2cAddr << 1) | 0)) {
        i2cStop();
        return false;
    }
    if (!i2cWriteByte(reg)) {
        i2cStop();
        return false;
    }
    if (!i2cWriteByte(val)) {
        i2cStop();
        return false;
    }
    i2cStop();
    return true;
}

void ImuMpu6050::recoverBus(uint8_t sda, uint8_t scl) {
    m_activeSda = sda;
    m_activeScl = scl;
    i2cSetHigh(sda);
    i2cSclHigh();
    delayMicroseconds(10);

    // 9 clock pulses on SCL to clear stuck slaves
    for (uint8_t i = 0; i < 9; i++) {
        i2cSetLow(scl);
        delayMicroseconds(10);
        i2cSclHigh();
        delayMicroseconds(10);
    }

    // Issue standard I2C STOP condition
    i2cStop();
    delayMicroseconds(50);
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

    // 1) Reset chip: write 0x80 to PWR_MGMT_1 (0x6B), wait 50 ms
    writeRegister(REG_PWR_MGMT_1, 0x80);
    delay(50);

    // 2) Auto clock select PLL, Sleep = 0: write 0x01 to PWR_MGMT_1 (0x6B)
    writeRegister(REG_PWR_MGMT_1, 0x01);
    delay(10);

    // 3) Enable all axes of accel and gyro: write 0x00 to PWR_MGMT_2 (0x6C)
    writeRegister(REG_PWR_MGMT_2, 0x00);
    delay(5);

    // 4) Set +/- 2g range: write 0x00 to ACCEL_CONFIG (0x1C)
    writeRegister(REG_ACCEL_CONFIG, 0x00);
    delay(5);

    // Configure DLPF (~44 Hz bandwidth: CONFIG = 0x03)
    writeRegister(REG_CONFIG, 0x03);

    // Initial read verification
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

    const char* chipName = (m_whoAmI == 0x70) ? "MPU-6500" :
                           (m_whoAmI == 0x68) ? "MPU-6050" : "IMU";
    snprintf(m_statusStr, sizeof(m_statusStr),
             "Online (%s, addr:0x%02X, who:0x%02X, SDA:%d, SCL:%d)",
             chipName, m_i2cAddr, m_whoAmI, m_activeSda, m_activeScl);
    Serial.printf("[IMU] %s\n", m_statusStr);
    return true;
}

bool ImuMpu6050::probePair(uint8_t sda, uint8_t scl) {
    recoverBus(sda, scl);
    static const uint8_t candidateAddrs[] = { 0x68, 0x69 };
    for (uint8_t addr : candidateAddrs) {
        i2cStart();
        bool ack = i2cWriteByte((addr << 1) | 0);
        i2cStop();
        if (ack) {
            if (initSensor(addr, sda, scl)) {
                return true;
            }
        }
    }
    return false;
}

bool ImuMpu6050::quickProbe() {
    if (probePair(m_sdaPin, m_sclPin)) return true;
    if (probePair(m_sclPin, m_sdaPin)) return true;
    return false;
}

bool ImuMpu6050::begin() {
    m_lastAutoScanTime = millis();

    // Enable internal pullups for ESP32-C6 GPIO 5 and GPIO 6
    pinMode(5, INPUT_PULLUP);
    pinMode(6, INPUT_PULLUP);
    gpio_set_pull_mode(GPIO_NUM_5, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_NUM_6, GPIO_PULLUP_ONLY);

    // 1. Probe primary pair (SDA=6, SCL=5)
    if (probePair(m_sdaPin, m_sclPin)) {
        return true;
    }

    // 2. Probe mirror pair (SDA=5, SCL=6)
    if (probePair(m_sclPin, m_sdaPin)) {
        return true;
    }

    // Sensor not found -> fallback mode
    int gpio5Idle = digitalRead(5);
    int gpio6Idle = digitalRead(6);
    snprintf(m_statusStr, sizeof(m_statusStr),
             "No IMU ACK on 5/6 (GPIO5:%s, GPIO6:%s)",
             gpio5Idle ? "HIGH" : "LOW", gpio6Idle ? "HIGH" : "LOW");
    Serial.printf("[IMU] %s. Activating Graceful Fallback mode.\n", m_statusStr);
    m_fallback = true;
    updateFallback(millis());
    return false;
}

bool ImuMpu6050::readRawData(int16_t& ax, int16_t& ay, int16_t& az) {
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
        // Periodic background probe every 5 seconds
        if (now - m_lastAutoScanTime >= 5000) {
            m_lastAutoScanTime = now;
            if (quickProbe()) {
                Serial.printf("[IMU] Sensor recovered during background probe: %s\n", m_statusStr);
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
    pinMode(5, INPUT_PULLUP);
    pinMode(6, INPUT_PULLUP);
    gpio_set_pull_mode(GPIO_NUM_5, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_NUM_6, GPIO_PULLUP_ONLY);
    delayMicroseconds(20);

    int gpio5Idle = digitalRead(5);
    int gpio6Idle = digitalRead(6);

    // If offline, attempt quick probe before returning diagnostics
    if (m_fallback) {
        quickProbe();
    }

    String out = "{";
    out += "\"status\":\"" + String(m_statusStr) + "\",";
    out += "\"connected\":" + String(isConnected() ? "true" : "false") + ",";
    out += "\"fallback\":" + String(m_fallback ? "true" : "false") + ",";
    out += "\"who_am_i\":\"0x" + String(m_whoAmI, HEX) + "\",";
    out += "\"sensor_type\":\"" + String((m_whoAmI == 0x70) ? "MPU-6500 (GY-6500)" : ((m_whoAmI == 0x68) ? "MPU-6050 (GY-521)" : "IMU")) + "\",";
    out += "\"sda_pin\":" + String(m_activeSda) + ",";
    out += "\"scl_pin\":" + String(m_activeScl) + ",";
    out += "\"active_sda\":" + String(m_activeSda) + ",";
    out += "\"active_scl\":" + String(m_activeScl) + ",";
    out += "\"GPIO5_idle\":" + String(gpio5Idle) + ",";
    out += "\"GPIO6_idle\":" + String(gpio6Idle) + ",";
    out += "\"raw_ax\":" + String(m_rawAx, 4) + ",";
    out += "\"raw_ay\":" + String(m_rawAy, 4) + ",";
    out += "\"raw_az\":" + String(m_rawAz, 4) + ",";
    out += "\"gravity\":{\"x\":" + String(m_gx, 3) + ",\"y\":" + String(m_gy, 3) + ",\"z\":" + String(m_gz, 3) + "}";
    out += "}";

    return out;
}
