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
    outTag.trim();
    if (outTag.isEmpty()) return false;

    // 2. Extract firmware.bin asset download URL if present in JSON buffer
    outUrl = "";
    int binIdx = json.indexOf("firmware.bin");
    if (binIdx >= 0) {
        int objStart = json.lastIndexOf('{', binIdx);
        int objEnd = json.indexOf('}', binIdx);
        if (objStart >= 0 && objEnd > objStart) {
            String assetObj = json.substring(objStart, objEnd + 1);
            int urlKeyIdx = assetObj.indexOf("\"browser_download_url\"");
            if (urlKeyIdx >= 0) {
                int urlColon = assetObj.indexOf(':', urlKeyIdx);
                if (urlColon >= 0) {
                    int urlQuoteStart = assetObj.indexOf('"', urlColon);
                    if (urlQuoteStart >= 0) {
                        int urlQuoteEnd = assetObj.indexOf('"', urlQuoteStart + 1);
                        if (urlQuoteEnd > urlQuoteStart) {
                            outUrl = assetObj.substring(urlQuoteStart + 1, urlQuoteEnd);
                        }
                    }
                }
            }
        }

        // Global search in json for browser_download_url containing firmware.bin
        if (outUrl.isEmpty()) {
            int urlKeyIdx = json.indexOf("\"browser_download_url\"");
            while (urlKeyIdx >= 0) {
                int urlColon = json.indexOf(':', urlKeyIdx);
                if (urlColon >= 0) {
                    int urlQuoteStart = json.indexOf('"', urlColon);
                    if (urlQuoteStart >= 0) {
                        int urlQuoteEnd = json.indexOf('"', urlQuoteStart + 1);
                        if (urlQuoteEnd > urlQuoteStart) {
                            String candidate = json.substring(urlQuoteStart + 1, urlQuoteEnd);
                            if (candidate.indexOf("firmware.bin") >= 0) {
                                outUrl = candidate;
                                break;
                            }
                        }
                    }
                }
                urlKeyIdx = json.indexOf("\"browser_download_url\"", urlKeyIdx + 22);
            }
        }
    }

    // 3. Deterministic direct fallback URL if browser_download_url was truncated or missing
    if (outUrl.isEmpty() || outUrl.indexOf("firmware.bin") < 0) {
        outUrl = "https://github.com/gaterkik/MagicLamp/releases/latest/download/firmware.bin";
    }

    return true;
}

void OtaGithub::runTask() {
    Serial.println(F("[OTA] Checking GitHub releases..."));
    setStatus("Connecting to GitHub...");

    int httpCode = 0;
    String payload;
    bool requestSuccess = false;

    for (uint8_t attempt = 1; attempt <= MAX_RETRY_ATTEMPTS; attempt++) {
        WiFiClientSecure client;
        client.setInsecure(); // Bypass CA certificate check for resilient updates
        client.setTimeout(HTTP_TIMEOUT_MS);
        client.setHandshakeTimeout(15);

        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.setTimeout(HTTP_TIMEOUT_MS);

        Serial.printf("[OTA] GitHub API request (attempt %u/%u)...\n", attempt, MAX_RETRY_ATTEMPTS);
        if (!http.begin(client, GITHUB_API_URL)) {
            Serial.printf("[OTA] Failed to initialize HTTP client for API URL (attempt %u/%u).\n",
                          attempt, MAX_RETRY_ATTEMPTS);
            if (attempt < MAX_RETRY_ATTEMPTS) {
                vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
                continue;
            }
            setStatus("HTTP begin failed");
            _isChecking = false;
            _isTaskRunning = false;
            _taskHandle = nullptr;
            vTaskDelete(NULL);
            return;
        }

        http.addHeader("User-Agent", USER_AGENT);
        http.addHeader("Accept", "application/vnd.github.v3+json");

        httpCode = http.GET();
        if (httpCode < 0) {
            Serial.printf("[OTA] GitHub API connection error %d on attempt %u/%u: %s\n",
                          httpCode, attempt, MAX_RETRY_ATTEMPTS, http.errorToString(httpCode).c_str());
            http.end();
            client.stop();
            if (attempt < MAX_RETRY_ATTEMPTS) {
                char retryBuf[64];
                snprintf(retryBuf, sizeof(retryBuf), "Retry API (%u/%u)...", attempt + 1, MAX_RETRY_ATTEMPTS);
                setStatus(retryBuf);
                vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
                continue;
            }
        } else {
            if (httpCode == HTTP_CODE_OK) {
                payload = http.getString();
                requestSuccess = true;
            }
            http.end();
            client.stop();
            break;
        }
    }

    if (!requestSuccess) {
        if (httpCode == HTTP_CODE_NOT_FOUND) {
            setStatus("Релизов на GitHub пока нет (HTTP 404)");
            Serial.println(F("[OTA] GitHub API returned HTTP 404: Релизов на GitHub пока нет."));
        } else if (httpCode < 0) {
            char errBuf[96];
            snprintf(errBuf, sizeof(errBuf), "API conn error %d", httpCode);
            setStatus(errBuf);
            Serial.printf("[OTA] GitHub API connection failed after %u attempts, error: %d\n",
                          MAX_RETRY_ATTEMPTS, httpCode);
        } else {
            char errBuf[96];
            snprintf(errBuf, sizeof(errBuf), "API HTTP error %d", httpCode);
            setStatus(errBuf);
            Serial.printf("[OTA] GitHub API request failed, HTTP code: %d\n", httpCode);
        }
        _isChecking = false;
        _isTaskRunning = false;
        _taskHandle = nullptr;
        vTaskDelete(NULL);
        return;
    }

    String remoteTag;
    String downloadUrl;
    if (!parseReleaseJson(payload, remoteTag, downloadUrl)) {
        setStatus("Parse error (no tag_name)");
        Serial.println(F("[OTA] Error parsing tag_name in release JSON."));
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
        snprintf(msg, sizeof(msg), "Установлена актуальная версия (v%s)", FIRMWARE_VERSION);
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
    Serial.printf("[OTA] Step 1: Resolving binary redirect from %s\n", downloadUrl.c_str());
    setStatus("Resolving redirect...");

    String targetUrl = downloadUrl;
    const char* headerKeys[] = {"Location"};

    // 1. Resolve redirect from GitHub to S3 (objects.githubusercontent.com)
    for (uint8_t attempt = 1; attempt <= MAX_RETRY_ATTEMPTS; attempt++) {
        WiFiClientSecure redirectClient;
        redirectClient.setInsecure();
        redirectClient.setTimeout(HTTP_TIMEOUT_MS);
        redirectClient.setHandshakeTimeout(15);

        HTTPClient redirectHttp;
        redirectHttp.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
        redirectHttp.setTimeout(HTTP_TIMEOUT_MS);
        redirectHttp.collectHeaders(headerKeys, 1);

        Serial.printf("[OTA] Checking download redirect (attempt %u/%u)...\n", attempt, MAX_RETRY_ATTEMPTS);
        if (!redirectHttp.begin(redirectClient, downloadUrl)) {
            Serial.printf("[OTA] redirectHttp.begin failed (attempt %u/%u)\n", attempt, MAX_RETRY_ATTEMPTS);
            if (attempt < MAX_RETRY_ATTEMPTS) {
                vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
                continue;
            }
            setStatus("Redirect init error");
            return;
        }

        redirectHttp.addHeader("User-Agent", USER_AGENT);
        int redirectCode = redirectHttp.GET();

        if (redirectCode < 0) {
            Serial.printf("[OTA] Redirect probe error %d on attempt %u/%u: %s\n",
                          redirectCode, attempt, MAX_RETRY_ATTEMPTS, redirectHttp.errorToString(redirectCode).c_str());
            redirectHttp.end();
            redirectClient.stop();
            if (attempt < MAX_RETRY_ATTEMPTS) {
                vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
                continue;
            }
            char errBuf[64];
            snprintf(errBuf, sizeof(errBuf), "Redirect error %d", redirectCode);
            setStatus(errBuf);
            return;
        }

        if (redirectCode == 301 || redirectCode == 302 || downloadUrl.indexOf("github.com") >= 0) {
            String redirectUrl = redirectHttp.header("Location");
            if (!redirectUrl.isEmpty()) {
                targetUrl = redirectUrl;
                Serial.printf("[OTA] HTTP %d redirect caught! Clean target URL: %s\n",
                              redirectCode, targetUrl.c_str());
            }
        } else {
            Serial.printf("[OTA] Direct download URL response code: %d\n", redirectCode);
        }

        // Close initial session cleanly before starting the new download session
        redirectHttp.end();
        redirectClient.stop();
        break;
    }

    // Brief pause to ensure socket cleanup and memory reclamation
    vTaskDelay(pdMS_TO_TICKS(200));

    // 2. Step 2: Clean separate TLS session for downloading binary
    Serial.printf("[OTA] Step 2: Connecting directly to download URL: %s\n", targetUrl.c_str());
    setStatus("Downloading firmware...");

    int downloadCode = 0;
    int contentLength = 0;

    WiFiClientSecure downloadClient;
    downloadClient.setInsecure();
    downloadClient.setTimeout(HTTP_TIMEOUT_MS);
    downloadClient.setHandshakeTimeout(15);

    HTTPClient downloadHttp;
    downloadHttp.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    downloadHttp.setTimeout(HTTP_TIMEOUT_MS);

    for (uint8_t attempt = 1; attempt <= MAX_RETRY_ATTEMPTS; attempt++) {
        Serial.printf("[OTA] Connecting to binary stream (attempt %u/%u)...\n", attempt, MAX_RETRY_ATTEMPTS);
        if (!downloadHttp.begin(downloadClient, targetUrl)) {
            Serial.printf("[OTA] downloadHttp.begin failed (attempt %u/%u)\n", attempt, MAX_RETRY_ATTEMPTS);
            if (attempt < MAX_RETRY_ATTEMPTS) {
                vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
                continue;
            }
            setStatus("Download init error");
            return;
        }

        downloadHttp.addHeader("User-Agent", USER_AGENT);
        downloadCode = downloadHttp.GET();

        if (downloadCode < 0) {
            Serial.printf("[OTA] Download GET failed with error %d on attempt %u/%u: %s\n",
                          downloadCode, attempt, MAX_RETRY_ATTEMPTS, downloadHttp.errorToString(downloadCode).c_str());
            downloadHttp.end();
            downloadClient.stop();
            if (attempt < MAX_RETRY_ATTEMPTS) {
                char retryBuf[64];
                snprintf(retryBuf, sizeof(retryBuf), "Retry DL (%u/%u)...", attempt + 1, MAX_RETRY_ATTEMPTS);
                setStatus(retryBuf);
                vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
                continue;
            }
        } else {
            break;
        }
    }

    if (downloadCode != HTTP_CODE_OK) {
        char errBuf[64];
        snprintf(errBuf, sizeof(errBuf), "Download HTTP %d", downloadCode);
        setStatus(errBuf);
        Serial.printf("[OTA] Download failed, HTTP code: %d\n", downloadCode);
        downloadHttp.end();
        downloadClient.stop();
        return;
    }

    contentLength = downloadHttp.getSize();
    Serial.printf("[OTA] Image size: %d bytes\n", contentLength);

    if (contentLength <= 0) {
        setStatus("Invalid binary size");
        Serial.println(F("[OTA] Error: Invalid Content-Length"));
        downloadHttp.end();
        downloadClient.stop();
        return;
    }

    if (!Update.begin(contentLength, U_FLASH)) {
        char errBuf[64];
        snprintf(errBuf, sizeof(errBuf), "OTA begin err: %s", Update.errorString());
        setStatus(errBuf);
        Serial.printf("[OTA] Update.begin failed: %s\n", Update.errorString());
        downloadHttp.end();
        downloadClient.stop();
        return;
    }

    _progress = 0;
    Update.onProgress([this](size_t progress, size_t total) {
        if (total > 0) {
            _progress = (int)((progress * 100) / total);
        }
    });

    setStatus("Flashing firmware...");
    Serial.println(F("[OTA] Step 3: Streaming binary directly into flash via Update.writeStream..."));

    size_t written = Update.writeStream(downloadHttp.getStream());
    Serial.printf("[OTA] Stream write finished. Written: %u / %d bytes\n",
                  (unsigned int)written, contentLength);

    if (written != (size_t)contentLength) {
        char errBuf[64];
        snprintf(errBuf, sizeof(errBuf), "Size mismatch %u/%d", (unsigned int)written, contentLength);
        setStatus(errBuf);
        Serial.printf("[OTA] Flash write mismatch: written %u != %d\n", (unsigned int)written, contentLength);
        Update.abort();
        downloadHttp.end();
        downloadClient.stop();
        return;
    }

    if (!Update.end(true)) {
        char errBuf[64];
        snprintf(errBuf, sizeof(errBuf), "OTA end err: %s", Update.errorString());
        setStatus(errBuf);
        Serial.printf("[OTA] Update.end failed: %s\n", Update.errorString());
        downloadHttp.end();
        downloadClient.stop();
        return;
    }

    downloadHttp.end();
    downloadClient.stop();

    if (Update.isFinished()) {
        _progress = 100;
        setStatus("Update complete! Rebooting...");
        Serial.println(F("[OTA] Firmware successfully installed. Rebooting ESP32..."));
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP.restart();
    } else {
        setStatus("Update not finished");
        Serial.println(F("[OTA] Error: Update.isFinished() returned false."));
    }
}

