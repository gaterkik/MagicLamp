#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <Zigbee.h>
#include "hal/led_matrix.h"

class EffectManager;
class WiFiManager;
class OtaGithub;

enum RadioMode : uint8_t {
    RADIO_MODE_WIFI = 0,
    RADIO_MODE_ZIGBEE = 1
};

#define ZIGBEE_LIGHT_ENDPOINT 1

/**
 * @file zigbee_manager.h
 * @brief Real IEEE 802.15.4 Zigbee 3.0 hardware stack with clean Boot-Switch via NVS & ESP.restart().
 */
class ZigbeeManager {
public:
    ZigbeeManager();
    ~ZigbeeManager() = default;

    /**
     * @brief Initialize radio manager and real Zigbee stack.
     * @param matrix Pointer to LedMatrix.
     * @param eff Pointer to EffectManager.
     */
    void begin(LedMatrix* matrix = nullptr, EffectManager* eff = nullptr);

    /**
     * @brief Periodic update called in loop(). Handles steering timeout (180s)
     * and network connection status.
     */
    void update(WiFiManager* wifi, EffectManager* eff, LedMatrix* matrix, OtaGithub* ota);

    uint8_t getMode() const { return m_mode; }
    uint8_t getActiveRadio() const { return m_activeRadio; }
    bool isZigbee() const { return m_mode == RADIO_MODE_ZIGBEE; }
    bool isWifi() const { return m_mode == RADIO_MODE_WIFI; }
    const char* getModeString() const {
        return (m_mode == RADIO_MODE_ZIGBEE) ? "zigbee" : "wifi";
    }

    uint8_t getProfile() const { return m_zbProfile; }
    void setProfile(uint8_t prof);

    /**
     * @brief Clean switch to Zigbee mode: plays purple-gold rings animation,
     * saves mode=1, force_pair=true to NVS, and executes ESP.restart().
     */
    void switchToZigbee(EffectManager* eff);

    /**
     * @brief Clean switch to Wi-Fi mode: saves diagnostic log to NVS and LittleFS,
     * plays green beacon animation, saves mode=0, and executes ESP.restart().
     */
    void switchToWifi(const char* reason, EffectManager* eff = nullptr);

    /**
     * @brief Toggle radio mode (Wi-Fi <-> Zigbee) with animation and clean reboot.
     */
    uint8_t toggleMode(EffectManager* eff = nullptr);

    // Multi-line cumulative flight recorder (LittleFS /zigbee.log + NVS ring buffer)
    static void appendLog(const char* msg, bool printSerial = true);
    static void clearLog();
    static String getLog();
    static void saveLog(const char* logMsg) { appendLog(logMsg); }

    // Alice/Zigbee command handlers
    static void handleZigbeeOnOff(bool on, EffectManager& effectMgr);
    static void handleZigbeeBrightness(uint8_t brightness, EffectManager& effectMgr);
    static void handleZigbeeColor(uint8_t hue, uint8_t saturation, EffectManager& effectMgr, LedMatrix& matrix);
    static void handleZigbeeColorTemp(uint16_t mireds, EffectManager& effectMgr, LedMatrix& matrix);

    void noteHttpActivity();
    void requestWifiSession(uint32_t durationMs, WiFiManager* wifi, EffectManager* eff, bool playAnim = true);

private:
    uint8_t m_mode;
    uint8_t m_activeRadio;
    uint8_t m_zbProfile;
    LedMatrix* m_matrix;

    uint32_t m_lastHttpActivityMs;
    uint32_t m_wifiStayActiveUntilMs;
    bool m_nightlyCheckDone;

    bool m_pairingActive;
    uint32_t m_pairingStartMs;
    bool m_connectedSuccessDone;

    static constexpr uint32_t BOOT_WIFI_WINDOW_MS = 60000;
    static constexpr uint32_t HTTP_WIFI_EXTENSION_MS = 600000;
    static constexpr uint32_t NIGHTLY_CHECK_WINDOW_MS = 45000;
    static constexpr uint32_t ZIGBEE_PAIRING_TIMEOUT_MS = 180000;
};
