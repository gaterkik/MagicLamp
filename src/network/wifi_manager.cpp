#include "wifi_manager.h"

WiFiManager::WiFiManager()
    : _state(WifiState::IDLE),
      _connectStartTime(0),
      _lastReconnectAttempt(0) {}

void WiFiManager::begin() {
    // 1. Read stored credentials from NVS preferences
    Preferences prefs;
    prefs.begin("wifi", true);
    _ssid = prefs.getString("ssid", "");
    _password = prefs.getString("pass", "");
    prefs.end();

    // 2. If NVS is empty, fall back to secrets.h
    if (_ssid.isEmpty() && strlen(WIFI_SSID) > 0) {
        _ssid = WIFI_SSID;
        _password = WIFI_PASSWORD;
    }

    // 3. If credentials exist, initiate non-blocking STA connection; otherwise start AP
    if (!_ssid.isEmpty()) {
        Serial.printf("[WiFi] Initializing STA connection to '%s'...\n", _ssid.c_str());
        startStaConnect();
    } else {
        Serial.println(F("[WiFi] No Wi-Fi credentials configured. Starting SoftAP setup mode."));
        startApMode();
    }
}

void WiFiManager::startStaConnect() {
    _dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
    delay(50);

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setHostname(HOSTNAME);
    WiFi.begin(_ssid.c_str(), _password.c_str());

    _connectStartTime = millis();
    _state = WifiState::STA_CONNECTING;
    Serial.printf("[WiFi] Connecting to '%s' (timeout %u ms)...\n", _ssid.c_str(), CONNECT_TIMEOUT_MS);
}

void WiFiManager::startApMode() {
    WiFi.disconnect(true);
    delay(50);

    WiFi.mode(WIFI_AP);
    IPAddress apIP(192, 168, 4, 1);
    IPAddress netMsk(255, 255, 255, 0);

    WiFi.softAPConfig(apIP, apIP, netMsk);
    WiFi.softAP(AP_NAME);

    _dnsServer.stop();
    _dnsServer.start(DNS_PORT, "*", apIP);

    _state = WifiState::AP_MODE;
    Serial.printf("[WiFi] SoftAP active: '%s' | IP: %s | Captive DNS active on port %d\n",
                  AP_NAME, apIP.toString().c_str(), DNS_PORT);
}

void WiFiManager::update() {
    switch (_state) {
        case WifiState::STA_CONNECTING: {
            if (WiFi.status() == WL_CONNECTED) {
                _state = WifiState::STA_CONNECTED;

                // Disable Wi-Fi modem sleep on ESP32-C6 to prevent socket drops and TLS failures
                WiFi.setSleep(false);
                // Disable SoftAP mode now that STA connection is active
                WiFi.enableAP(false);

                Serial.printf("[WiFi] Connected successfully! IP: %s, RSSI: %d dBm (Radio Sleep: OFF, AP: OFF)\n",
                              WiFi.localIP().toString().c_str(), WiFi.RSSI());

                // Configure NTP time synchronization (GMT+2: 7200 sec, fixed UTC+2)
                configTime(7200, 0, "pool.ntp.org", "time.google.com");
                Serial.println(F("[NTP] Configured time synchronization (GMT+2: 7200s, servers: pool.ntp.org, time.google.com)"));

                // Start mDNS
                if (MDNS.begin(HOSTNAME)) {
                    Serial.printf("[mDNS] Responder started: http://%s.local\n", HOSTNAME);
                    MDNS.addService("http", "tcp", 80);
                } else {
                    Serial.println(F("[mDNS] Failed to start responder"));
                }

                // Start ArduinoOTA
                ArduinoOTA.setHostname(HOSTNAME);
                ArduinoOTA.onStart([]() {
                    Serial.println(F("[ArduinoOTA] Start flashing..."));
                });
                ArduinoOTA.onEnd([]() {
                    Serial.println(F("\n[ArduinoOTA] Flash complete. Rebooting..."));
                });
                ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
                    Serial.printf("[ArduinoOTA] Progress: %u%%\r", (progress * 100) / total);
                });
                ArduinoOTA.onError([](ota_error_t error) {
                    Serial.printf("[ArduinoOTA] Error [%u]\n", error);
                });
                ArduinoOTA.begin();
                Serial.println(F("[ArduinoOTA] Service listening on port 3232."));
            } else if (millis() - _connectStartTime >= CONNECT_TIMEOUT_MS) {
                Serial.println(F("[WiFi] STA connection timeout reached. Switching to SoftAP mode."));
                startApMode();
            }
            break;
        }

        case WifiState::STA_CONNECTED: {
            ArduinoOTA.handle();

            if (WiFi.status() != WL_CONNECTED) {
                unsigned long now = millis();
                if (now - _lastReconnectAttempt >= RECONNECT_INTERVAL_MS) {
                    _lastReconnectAttempt = now;
                    Serial.println(F("[WiFi] Lost STA connection, attempting reconnect..."));
                    WiFi.reconnect();
                }
            }
            break;
        }

        case WifiState::AP_MODE: {
            _dnsServer.processNextRequest();
            break;
        }

        default:
            break;
    }
}

bool WiFiManager::isConnected() const {
    return (_state == WifiState::STA_CONNECTED && WiFi.status() == WL_CONNECTED);
}

bool WiFiManager::isApMode() const {
    return (_state == WifiState::AP_MODE);
}

String WiFiManager::getIpAddress() const {
    if (isConnected()) {
        return WiFi.localIP().toString();
    } else if (isApMode()) {
        return WiFi.softAPIP().toString();
    }
    return String("0.0.0.0");
}

int8_t WiFiManager::getRssi() const {
    if (isConnected()) {
        return WiFi.RSSI();
    }
    return 0;
}

String WiFiManager::getSsid() const {
    if (isApMode()) {
        return String(AP_NAME);
    }
    return _ssid;
}

void WiFiManager::saveCredentials(const String& ssid, const String& pass) {
    Preferences prefs;
    if (prefs.begin("wifi", false)) {
        prefs.putString("ssid", ssid);
        prefs.putString("pass", pass);
        prefs.end();
    }
    _ssid = ssid;
    _password = pass;
    Serial.printf("[WiFi] Saved new credentials for SSID '%s'. Reconnecting...\n", _ssid.c_str());

    if (!_ssid.isEmpty()) {
        startStaConnect();
    } else {
        startApMode();
    }
}

void WiFiManager::resetCredentials() {
    Preferences prefs;
    if (prefs.begin("wifi", false)) {
        prefs.clear();
        prefs.end();
    }
    _ssid = "";
    _password = "";
    Serial.println(F("[WiFi] Cleared Wi-Fi credentials in NVS. Starting SoftAP."));
    startApMode();
}

bool WiFiManager::getLocalTime(struct tm *info) const {
    if (!info) return false;
    return ::getLocalTime(info, 10);
}

String WiFiManager::getFormattedTime() const {
    struct tm timeinfo;
    if (::getLocalTime(&timeinfo, 10)) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        return String(buf);
    }
    return String("--:--");
}

bool WiFiManager::isTimeSynced() const {
    time_t now;
    time(&now);
    return (now > 100000);
}
