#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "hal/led_matrix.h"

class EffectManager;
class WiFiManager;
class OtaGithub;

enum RadioMode : uint8_t {
    RADIO_MODE_WIFI = 0,
    RADIO_MODE_ZIGBEE = 1
};

/**
 * @file zigbee_manager.h
 * @brief Radio mode management (Wi-Fi <-> Zigbee), automated transitions,
 *        and Alice / Zigbee command mapping on ESP32-C6.
 */
class ZigbeeManager {
public:
    ZigbeeManager();
    ~ZigbeeManager() = default;

    /**
     * @brief Initialize radio manager and read saved mode from NVS Preferences.
     * Sets 60s boot Wi-Fi window.
     * @param matrix Pointer to LedMatrix for visual indication animations.
     */
    void begin(LedMatrix* matrix = nullptr);

    /**
     * @brief Periodic update called in loop(). Handles automatic Wi-Fi <-> Zigbee transitions
     * and daily 04:00 GMT+2 OTA window.
     */
    void update(WiFiManager* wifi, EffectManager* eff, LedMatrix* matrix, OtaGithub* ota);

    /**
     * @brief Get configured radio mode from NVS (0 = WIFI, 1 = ZIGBEE).
     */
    uint8_t getMode() const { return m_mode; }

    /**
     * @brief Get active radio mode currently running (0 = WIFI, 1 = ZIGBEE).
     */
    uint8_t getActiveRadio() const { return m_activeRadio; }

    /**
     * @brief Check if configured mode is Zigbee.
     */
    bool isZigbee() const { return m_mode == RADIO_MODE_ZIGBEE; }

    /**
     * @brief Check if configured mode is Wi-Fi.
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
    bool setMode(uint8_t mode, bool playAnim = true, EffectManager* eff = nullptr, WiFiManager* wifi = nullptr);

    /**
     * @brief Toggle radio mode (Wi-Fi <-> Zigbee) with NVS persistence and matrix animation.
     * @return New radio mode.
     */
    uint8_t toggleMode(EffectManager* eff = nullptr, WiFiManager* wifi = nullptr);

    /**
     * @brief Note incoming HTTP activity on WebServer.
     * Extends active Wi-Fi window by 10 minutes.
     */
    void noteHttpActivity();

    /**
     * @brief Request temporary active Wi-Fi session (e.g. 10 minutes on 4 clicks, or 45s for 04:00 check).
     * @param durationMs Session duration in ms.
     * @param wifi Pointer to WiFiManager to start connection if needed.
     * @param eff Pointer to EffectManager to check power state for animation.
     * @param playAnim Whether to play wave animation.
     */
    void requestWifiSession(uint32_t durationMs, WiFiManager* wifi, EffectManager* eff, bool playAnim = true);

    /**
     * @brief Play visual indication wave animation on matrix:
     * 3 waves running up the cylinder:
     * - Teal / Cyan for Wi-Fi
     * - Purple / Magenta for Zigbee
     * @param mode Radio mode to animate for.
     */
    void playSwitchAnimation(uint8_t mode);

    // Zigbee / Alice Commands Support
    void handleZigbeeOnOff(bool on, EffectManager& effectMgr);
    void handleZigbeeBrightness(uint8_t brightness, EffectManager& effectMgr);
    void handleZigbeeColor(uint8_t hue, uint8_t saturation, EffectManager& effectMgr, LedMatrix& matrix);
    void handleZigbeeColorTemp(uint16_t mireds, EffectManager& effectMgr, LedMatrix& matrix);

    /**
     * @brief Start 180-second Zigbee pairing / commissioning mode with Alice purple breathing indicator.
     */
    void startPairing(EffectManager* eff = nullptr);

    /**
     * @brief Stop Zigbee pairing mode and turn off breathing indicator.
     */
    void stopPairing(EffectManager* eff = nullptr);

    /**
     * @brief Check if Zigbee pairing / commissioning is active.
     */
    bool isPairing() const { return m_pairingActive; }

private:
    uint8_t m_mode;             // Configured radio mode in NVS
    uint8_t m_activeRadio;      // Currently active physical radio
    LedMatrix* m_matrix;

    uint32_t m_lastHttpActivityMs;
    uint32_t m_wifiStayActiveUntilMs;
    bool m_nightlyCheckDone;

    bool m_pairingActive;
    uint32_t m_pairingStartMs;

    static constexpr uint32_t BOOT_WIFI_WINDOW_MS = 60000;      // 60 seconds
    static constexpr uint32_t HTTP_WIFI_EXTENSION_MS = 600000;  // 10 minutes
    static constexpr uint32_t NIGHTLY_CHECK_WINDOW_MS = 45000;  // 45 seconds
    static constexpr uint32_t ZIGBEE_PAIRING_TIMEOUT_MS = 180000; // 180 seconds pairing window
};
