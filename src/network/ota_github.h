#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include "version.h"
#include "hal/led_matrix.h"

/**
 * @brief Background GitHub Release OTA updater for MagicLamp ESP32-C6.
 * 
 * Periodically checks GitHub releases for newer firmware versions,
 * parses assets for 'firmware.bin', follows HTTP 302 redirects,
 * downloads and flashes the update, and reboots cleanly.
 */
class OtaGithub {
public:
    OtaGithub();
    ~OtaGithub() = default;

    void init();
    void triggerCheck();
    void update(LedMatrix* matrix);

    const char* getLastStatus() const { return _lastStatus; }
    const char* getLatestVersion() const { return _latestVersion; }
    bool isUpdating() const { return _isUpdating; }
    bool isChecking() const { return _isChecking; }
    int getProgress() const { return _progress; }

    static bool isNewerVersion(const char* remoteVer, const char* currentVer);
    static bool parseReleaseJson(const String& json, String& outTag, String& outUrl);

    // FreeRTOS task entry point
    void runTask();

private:
    void setStatus(const char* status);
    void performUpdate(const String& downloadUrl);

    volatile bool _checkRequested = false;
    volatile bool _isChecking = false;
    volatile bool _isUpdating = false;
    volatile bool _isTaskRunning = false;
    volatile int _progress = 0;

    char _lastStatus[96] = "Idle";
    char _latestVersion[32] = "";

    TaskHandle_t _taskHandle = nullptr;
    unsigned long _lastPeriodicCheck = 0;
    bool _initialCheckDone = false;

    static constexpr uint32_t INITIAL_CHECK_DELAY_MS = 30000;       // 30 seconds after boot
    static constexpr uint32_t PERIODIC_CHECK_INTERVAL_MS = 1800000; // 30 minutes
    static constexpr const char* GITHUB_API_URL = "https://api.github.com/repos/gaterkik/MagicLamp/releases/latest";
    static constexpr const char* USER_AGENT = "MagicLamp-ESP32C6-OTA";
};
