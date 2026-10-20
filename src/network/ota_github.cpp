#include "ota_github.h"

static void otaTaskTrampoline(void* param) {
    OtaGithub* updater = static_cast<OtaGithub*>(param);
    updater->runTask();
}

OtaGithub::OtaGithub()
    : _checkRequested(false),
      _isChecking(false),
      _isUpdating(false),
      _isTaskRunning(false),
      _progress(0),
      _taskHandle(nullptr),
      _lastPeriodicCheck(0),
      _initialCheckDone(false) {
    strncpy(_lastStatus, "Idle", sizeof(_lastStatus) - 1);
    strncpy(_latestVersion, FIRMWARE_VERSION, sizeof(_latestVersion) - 1);
}

void OtaGithub::init() {
    _lastPeriodicCheck = millis();
    _initialCheckDone = false;
    _checkRequested = false;
    setStatus("Initialized");
    Serial.printf("[OTA] GitHub OTA module initialized. Current version: v%s\n", FIRMWARE_VERSION);
}

void OtaGithub::triggerCheck() {
    _checkRequested = true;
    Serial.println(F("[OTA] Check triggered by request."));
}

void OtaGithub::setStatus(const char* status) {
    if (status) {
        strncpy(_lastStatus, status, sizeof(_lastStatus) - 1);
        _lastStatus[sizeof(_lastStatus) - 1] = '\0';
    }
}

void OtaGithub::update(LedMatrix* matrix) {
    unsigned long now = millis();

    // 1. Initial 30s timer check
    if (!_initialCheckDone && (now >= INITIAL_CHECK_DELAY_MS)) {
        _initialCheckDone = true;
        _lastPeriodicCheck = now;
        Serial.println(F("[OTA] 30s boot timer reached: requesting initial release check."));
        triggerCheck();
    }

    // 2. Periodic 30m timer check
    if (_initialCheckDone && (now - _lastPeriodicCheck >= PERIODIC_CHECK_INTERVAL_MS)) {
        _lastPeriodicCheck = now;
        Serial.println(F("[OTA] 30m periodic timer reached: requesting release check."));
        triggerCheck();
    }

    // 3. Process check trigger
    if (_checkRequested && !_isTaskRunning) {
        _checkRequested = false;
        if (WiFi.status() != WL_CONNECTED) {
            setStatus("Wi-Fi not connected");
            Serial.println(F("[OTA] Cannot check for updates: Wi-Fi not connected."));
        } else {
            BaseType_t res = xTaskCreate(
                otaTaskTrampoline,
                "ota_task",
                12288,
                this,
                1,
                &_taskHandle
            );
            if (res == pdPASS) {
                _isTaskRunning = true;
                _isChecking = true;
                setStatus("Checking updates...");
            } else {
                _taskHandle = nullptr;
                setStatus("Task create error");
                Serial.println(F("[OTA] Failed to create FreeRTOS OTA task!"));
            }
        }
    }

    // 4. Visual progress feedback while updating
    if (_isUpdating && matrix) {
        matrix->clear();

        // Bottom row progress bar (0..16 pixels)
        int fillLeds = (_progress * MATRIX_WIDTH) / 100;
        if (fillLeds > MATRIX_WIDTH) fillLeds = MATRIX_WIDTH;
        for (int x = 0; x < fillLeds; x++) {
            matrix->setPixel(x, 0, CRGB(0, 150, 255)); // Bright cyan
        }

        // Center pulsing amber dot to show active flashing
        uint8_t pulse = (sin8(millis() / 4) * 160) / 255 + 40;
        matrix->setPixel(7, 8, CRGB(pulse, pulse, 0));
        matrix->setPixel(8, 8, CRGB(pulse, pulse, 0));

        matrix->show();
    }
}

bool OtaGithub::isNewerVersion(const char* remoteVer, const char* currentVer) {
    if (!remoteVer || !currentVer) return false;

    // Skip optional 'v' or 'V' prefix
    if (*remoteVer == 'v' || *remoteVer == 'V') remoteVer++;
    if (*currentVer == 'v' || *currentVer == 'V') currentVer++;

    int rMajor = 0, rMinor = 0, rPatch = 0;
    int cMajor = 0, cMinor = 0, cPatch = 0;

    sscanf(remoteVer, "%d.%d.%d", &rMajor, &rMinor, &rPatch);
    sscanf(currentVer, "%d.%d.%d", &cMajor, &cMinor, &cPatch);

    if (rMajor != cMajor) return rMajor > cMajor;
    if (rMinor != cMinor) return rMinor > cMinor;
    return rPatch > cPatch;
}

bool OtaGithub::parseReleaseJson(const String& json, String& outTag, String& outUrl) {
    // 1. Extract tag_name
    int tagKeyIdx = json.indexOf("\"tag_name\"");
    if (tagKeyIdx < 0) return false;

    int colonIdx = json.indexOf(':', tagKeyIdx);
    if (colonIdx < 0) return false;

    int quoteStart = json.indexOf('"', colonIdx);
    if (quoteStart < 0) return false;

    int quoteEnd = json.indexOf('"', quoteStart + 1);
    if (quoteEnd < 0) return false;

    outTag = json.substring(quoteStart + 1, quoteEnd);

    // 2. Extract firmware.bin asset download URL
    int binIdx = json.indexOf("firmware.bin");
    if (binIdx < 0) return false;

    int objStart = json.lastIndexOf('{', binIdx);
    int objEnd = json.indexOf('}', binIdx);
    if (objStart < 0 || objEnd < 0 || objEnd <= objStart) return false;

    String assetObj = json.substring(objStart, objEnd + 1);
    int urlKeyIdx = assetObj.indexOf("\"browser_download_url\"");
    if (urlKeyIdx < 0) return false;

    int urlColon = assetObj.indexOf(':', urlKeyIdx);
    if (urlColon < 0) return false;

    int urlQuoteStart = assetObj.indexOf('"', urlColon);
    if (urlQuoteStart < 0) return false;

    int urlQuoteEnd = assetObj.indexOf('"', urlQuoteStart + 1);
    if (urlQuoteEnd < 0) return false;

    outUrl = assetObj.substring(urlQuoteStart + 1, urlQuoteEnd);
    return (!outTag.isEmpty() && !outUrl.isEmpty());
}

void OtaGithub::runTask() {
    Serial.println(F("[OTA] Checking GitHub releases..."));
    setStatus("Connecting to GitHub...");

    WiFiClientSecure client;
    client.setInsecure(); // Bypass CA certificate check for resilient updates

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000);

    if (!http.begin(client, GITHUB_API_URL)) {
        setStatus("HTTP begin failed");
        Serial.println(F("[OTA] Failed to initialize HTTP client for API URL."));
        _isChecking = false;
        _isTaskRunning = false;
        _taskHandle = nullptr;
        vTaskDelete(NULL);
        return;
    }

    http.addHeader("User-Agent", USER_AGENT);
    http.addHeader("Accept", "application/vnd.github.v3+json");

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        if (httpCode == HTTP_CODE_NOT_FOUND) {
            setStatus("Релизов на GitHub пока нет (HTTP 404)");
            Serial.println(F("[OTA] GitHub API returned HTTP 404: Релизов на GitHub пока нет."));
        } else {
            char errBuf[96];
            snprintf(errBuf, sizeof(errBuf), "API HTTP error %d", httpCode);
            setStatus(errBuf);
            Serial.printf("[OTA] GitHub API request failed, HTTP code: %d\n", httpCode);
        }
        http.end();
        _isChecking = false;
        _isTaskRunning = false;
        _taskHandle = nullptr;
        vTaskDelete(NULL);
        return;
    }

    String payload = http.getString();
    http.end();

    String remoteTag;
    String downloadUrl;
    if (!parseReleaseJson(payload, remoteTag, downloadUrl)) {
        setStatus("Parse error (no firmware.bin)");
        Serial.println(F("[OTA] Error parsing tag_name or firmware.bin asset in release JSON."));
        _isChecking = false;
        _isTaskRunning = false;
        _taskHandle = nullptr;
        vTaskDelete(NULL);
        return;
    }

    strncpy(_latestVersion, remoteTag.c_str(), sizeof(_latestVersion) - 1);
    _latestVersion[sizeof(_latestVersion) - 1] = '\0';
    Serial.printf("[OTA] Latest GitHub release: %s | URL: %s\n", remoteTag.c_str(), downloadUrl.c_str());

    // Compare versions
    if (!isNewerVersion(remoteTag.c_str(), FIRMWARE_VERSION)) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Up to date (v%s)", FIRMWARE_VERSION);
        setStatus(msg);
        Serial.printf("[OTA] Current version v%s is up to date (remote: %s).\n",
                      FIRMWARE_VERSION, remoteTag.c_str());
        _isChecking = false;
        _isTaskRunning = false;
        _taskHandle = nullptr;
        vTaskDelete(NULL);
        return;
    }

    // New version detected!
    Serial.printf("[OTA] New version available: %s > v%s. Starting update...\n",
                  remoteTag.c_str(), FIRMWARE_VERSION);
    setStatus("Downloading firmware...");
    _isChecking = false;
    _isUpdating = true;
    _progress = 0;

    performUpdate(downloadUrl);

    // If performUpdate returns, update either failed or was aborted
    _isUpdating = false;
    _isTaskRunning = false;
    _taskHandle = nullptr;
    vTaskDelete(NULL);
}

void OtaGithub::performUpdate(const String& downloadUrl) {
    Serial.printf("[OTA] Downloading binary from %s\n", downloadUrl.c_str());

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(30000);

    if (!http.begin(client, downloadUrl)) {
        setStatus("Download begin error");
        Serial.println(F("[OTA] HTTP begin failed for firmware download"));
        return;
    }

    http.addHeader("User-Agent", USER_AGENT);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        char errBuf[64];
        snprintf(errBuf, sizeof(errBuf), "Download HTTP %d", httpCode);
        setStatus(errBuf);
        Serial.printf("[OTA] Download failed, HTTP code: %d\n", httpCode);
        http.end();
        return;
    }

    int contentLength = http.getSize();
    Serial.printf("[OTA] Image size: %d bytes\n", contentLength);

    if (contentLength <= 0) {
        setStatus("Invalid binary size");
        Serial.println(F("[OTA] Error: Invalid Content-Length"));
        http.end();
        return;
    }

    if (!Update.begin(contentLength, U_FLASH)) {
        char errBuf[64];
        snprintf(errBuf, sizeof(errBuf), "OTA begin err: %s", Update.errorString());
        setStatus(errBuf);
        Serial.printf("[OTA] Update.begin failed: %s\n", Update.errorString());
        http.end();
        return;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[1024];
    int bytesWritten = 0;
    _progress = 0;
    unsigned long lastDataTime = millis();

    while (http.connected() && (bytesWritten < contentLength)) {
        size_t availableBytes = stream->available();
        if (availableBytes > 0) {
            size_t toRead = (availableBytes > sizeof(buffer)) ? sizeof(buffer) : availableBytes;
            int bytesRead = stream->readBytes(buffer, toRead);
            if (bytesRead > 0) {
                size_t written = Update.write(buffer, bytesRead);
                if (written != (size_t)bytesRead) {
                    setStatus("Flash write error");
                    Serial.println(F("[OTA] Error: Update.write size mismatch"));
                    Update.abort();
                    http.end();
                    return;
                }
                bytesWritten += bytesRead;
                _progress = (int)(((int64_t)bytesWritten * 100) / contentLength);
                lastDataTime = millis();
            }
        } else {
            if (millis() - lastDataTime > 10000) {
                setStatus("Stream timeout");
                Serial.println(F("[OTA] Error: Download stream timeout (10s no data)"));
                Update.abort();
                http.end();
                return;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    if (bytesWritten != contentLength) {
        char errBuf[64];
        snprintf(errBuf, sizeof(errBuf), "Size mismatch %d/%d", bytesWritten, contentLength);
        setStatus(errBuf);
        Serial.printf("[OTA] Error: bytesWritten (%d) != contentLength (%d)\n",
                      bytesWritten, contentLength);
        Update.abort();
        http.end();
        return;
    }

    if (!Update.end(true)) {
        char errBuf[64];
        snprintf(errBuf, sizeof(errBuf), "OTA end err: %s", Update.errorString());
        setStatus(errBuf);
        Serial.printf("[OTA] Update.end failed: %s\n", Update.errorString());
        http.end();
        return;
    }

    http.end();

    if (Update.isFinished()) {
        setStatus("Update complete! Rebooting...");
        Serial.println(F("[OTA] Firmware successfully installed. Rebooting ESP32..."));
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP.restart();
    } else {
        setStatus("Update not finished");
        Serial.println(F("[OTA] Error: Update.isFinished() returned false."));
    }
}
