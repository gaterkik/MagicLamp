#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "secrets.h"

enum class WifiState {
    IDLE,
    STA_CONNECTING,
    STA_CONNECTED,
    AP_MODE
};

/**
 * @brief Non-blocking Wi-Fi manager with NVS preferences, SoftAP fallback,
 *        Captive Portal DNS, mDNS (magiclamp.local), and ArduinoOTA support.
 */
class WiFiManager {
public:
    WiFiManager();
    ~WiFiManager() = default;

    void begin();
    void update();

    bool isConnected() const;
    bool isApMode() const;
    String getIpAddress() const;
    int8_t getRssi() const;
    String getSsid() const;

    void saveCredentials(const String& ssid, const String& pass);
    void resetCredentials();
    void resetSettings() { resetCredentials(); }

    bool getLocalTime(struct tm *info) const;
    String getFormattedTime() const;
    bool isTimeSynced() const;

    WifiState getState() const { return _state; }

private:
    void startStaConnect();
    void startApMode();

    WifiState _state = WifiState::IDLE;
    String _ssid;
    String _password;
    DNSServer _dnsServer;
    unsigned long _connectStartTime = 0;
    unsigned long _lastReconnectAttempt = 0;

    static constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;
    static constexpr uint32_t RECONNECT_INTERVAL_MS = 10000;
    static constexpr const char* AP_NAME = "MagicLamp-Setup";
    static constexpr const char* HOSTNAME = "magiclamp";
    static constexpr uint16_t DNS_PORT = 53;
};
