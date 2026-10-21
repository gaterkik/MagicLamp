#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "hal/led_matrix.h"

enum RadioMode : uint8_t {
    RADIO_MODE_WIFI = 0,
    RADIO_MODE_ZIGBEE = 1
};

/**
 * @file zigbee_manager.h
 * @brief Radio mode management (Wi-Fi <-> Zigbee) and visual indication waves on ESP32-C6.
 */
class ZigbeeManager {
public:
    ZigbeeManager();
    ~ZigbeeManager() = default;

    /**
     * @brief Initialize radio manager and read saved mode from NVS Preferences.
     * @param matrix Pointer to LedMatrix for visual indication animations.
     */
    void begin(LedMatrix* matrix = nullptr);

    /**
     * @brief Get current radio mode (0 = WIFI, 1 = ZIGBEE).
     */
    uint8_t getMode() const { return m_mode; }

    /**
     * @brief Check if currently in Zigbee mode.
     */
    bool isZigbee() const { return m_mode == RADIO_MODE_ZIGBEE; }

    /**
     * @brief Check if currently in Wi-Fi mode.
     */
    bool isWifi() const { return m_mode == RADIO_MODE_WIFI; }

    /**
     * @brief Get radio mode string representation ("wifi" or "zigbee").
     */
    const char* getModeString() const {
        return (m_mode == RADIO_MODE_ZIGBEE) ? "zigbee" : "wifi";
    }

    /**
     * @brief Set radio mode (0 = WIFI, 1 = ZIGBEE) with NVS persistence and matrix animation.
     * @param mode Target radio mode.
     * @param playAnim Whether to play matrix wave animation (default true).
     * @return true if mode was changed, false if already in that mode.
     */
    bool setMode(uint8_t mode, bool playAnim = true);

    /**
     * @brief Toggle radio mode (Wi-Fi <-> Zigbee) with NVS persistence and matrix animation.
     * @return New radio mode.
     */
    uint8_t toggleMode();

    /**
     * @brief Play visual indication wave animation on matrix:
     * 3 waves running up the cylinder:
     * - Teal / Cyan for Wi-Fi
     * - Purple / Magenta for Zigbee
     * @param mode Radio mode to animate for.
     */
    void playSwitchAnimation(uint8_t mode);

private:
    uint8_t m_mode;
    LedMatrix* m_matrix;
};
