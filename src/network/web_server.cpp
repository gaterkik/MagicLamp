#include "web_server.h"
#include "webui_html.h"
#include "version.h"
#include "effects/effect_manager.h"
#include "hal/led_matrix.h"
#include "hal/mic_i2s.h"
#include "hal/imu_mpu6050.h"
#include "wifi_manager.h"
#include "ota_github.h"
#include <Update.h>

LampWebServer::LampWebServer()
    : m_server(80),
      m_effectMgr(nullptr),
      m_matrix(nullptr),
      m_mic(nullptr),
      m_imu(nullptr),
      m_wifi(nullptr),
      m_ota(nullptr),
      m_rebootPending(false),
      m_rebootTime(0) {}

void LampWebServer::begin(EffectManager* effectMgr,
                          LedMatrix* matrix,
                          MicI2S* mic,
                          ImuMpu6050* imu,
                          WiFiManager* wifi,
                          OtaGithub* ota) {
    m_effectMgr = effectMgr;
    m_matrix = matrix;
    m_mic = mic;
    m_imu = imu;
    m_wifi = wifi;
    m_ota = ota;
    m_rebootPending = false;
    m_rebootTime = 0;

    registerRoutes();
    m_server.begin();
    Serial.println(F("[WebServer] HTTP server started on port 80."));
}

void LampWebServer::update() {
    m_server.handleClient();

    if (m_rebootPending && (millis() - m_rebootTime >= 500)) {
        Serial.println(F("[WebServer] Rebooting ESP32-C6 now..."));
        delay(100);
        ESP.restart();
    }
}

static bool getIntParam(WebServer& server, const String& json, const char* name, int& outVal) {
    if (server.hasArg(name)) {
        outVal = server.arg(name).toInt();
        return true;
    }
    if (!json.isEmpty()) {
        String key = String("\"") + name + "\"";
        int idx = json.indexOf(key);
        if (idx >= 0) {
            int colon = json.indexOf(':', idx + key.length());
            if (colon >= 0) {
                int valStart = colon + 1;
                while (valStart < (int)json.length() && (json[valStart] == ' ' || json[valStart] == '\t')) valStart++;
                outVal = json.substring(valStart).toInt();
                return true;
            }
        }
    }
    return false;
}

static bool getBoolParam(WebServer& server, const String& json, const char* name, bool& outVal) {
    if (server.hasArg(name)) {
        String val = server.arg(name);
        val.toLowerCase();
        outVal = (val == "true" || val == "1" || val == "on");
        return true;
    }
    if (!json.isEmpty()) {
        String key = String("\"") + name + "\"";
        int idx = json.indexOf(key);
        if (idx >= 0) {
            int colon = json.indexOf(':', idx + key.length());
            if (colon >= 0) {
                int valStart = colon + 1;
                while (valStart < (int)json.length() && (json[valStart] == ' ' || json[valStart] == '\t')) valStart++;
                if (json.substring(valStart).startsWith("true") || json.substring(valStart).startsWith("1")) {
                    outVal = true;
                    return true;
                } else if (json.substring(valStart).startsWith("false") || json.substring(valStart).startsWith("0")) {
                    outVal = false;
                    return true;
                }
            }
        }
    }
    return false;
}

static bool getStringParam(WebServer& server, const String& json, const char* name, String& outVal) {
    if (server.hasArg(name)) {
        outVal = server.arg(name);
        return true;
    }
    if (!json.isEmpty()) {
        String key = String("\"") + name + "\"";
        int idx = json.indexOf(key);
        if (idx >= 0) {
            int colon = json.indexOf(':', idx + key.length());
            if (colon >= 0) {
                int q1 = json.indexOf('"', colon);
                if (q1 >= 0) {
                    int q2 = json.indexOf('"', q1 + 1);
                    if (q2 >= 0) {
                        outVal = json.substring(q1 + 1, q2);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

void LampWebServer::registerRoutes() {
    m_server.enableCORS(true);

    // Root SPA
    m_server.on("/", HTTP_GET, [this]() { handleRoot(); });

    // REST API
    m_server.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    m_server.on("/api/control", HTTP_POST, [this]() { handleControl(); });
    m_server.on("/api/ota/check", HTTP_POST, [this]() { handleOtaCheck(); });
    m_server.on("/api/wifi", HTTP_POST, [this]() { handleWifi(); });

    // Browser OTA Upload Handler via Update.h
    m_server.on("/api/ota/upload", HTTP_POST, [this]() {
        if (!Update.hasError()) {
            m_server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Update successful, rebooting...\"}");
            m_rebootPending = true;
            m_rebootTime = millis();
        } else {
            m_server.send(500, "application/json", "{\"status\":\"error\",\"message\":\"Update failed\"}");
        }
    }, [this]() {
        HTTPUpload& upload = m_server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            Serial.printf("[WebOTA] Update started: %s\n", upload.filename.c_str());
            if (m_matrix) {
                m_matrix->clear(true);
            }
            if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) {
                Serial.printf("[WebOTA] Success: %u bytes written. Reboot pending.\n", upload.totalSize);
            } else {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_ABORTED) {
            Update.end();
            Serial.println(F("[WebOTA] Update aborted."));
        }
    });

    // Captive Portal Probe Endpoints
    auto captiveHandler = [this]() {
        String target = String("http://") + (m_wifi ? m_wifi->getIpAddress() : "192.168.4.1") + "/";
        m_server.sendHeader("Location", target, true);
        m_server.send(302, "text/plain", "");
    };

    m_server.on("/generate_204", captiveHandler);
    m_server.on("/gen_204", captiveHandler);
    m_server.on("/hotspot-detect.html", captiveHandler);
    m_server.on("/ncsi.txt", captiveHandler);
    m_server.on("/connecttest.txt", captiveHandler);
    m_server.on("/canonical.html", captiveHandler);

    // 404 / Captive redirect fallback
    m_server.onNotFound([this]() { handleNotFound(); });
}

void LampWebServer::handleRoot() {
    m_server.send_P(200, "text/html; charset=utf-8", WEBUI_HTML);
}

void LampWebServer::handleStatus() {
    char json[1024];
    float cpuTemp = temperatureRead();
    uint32_t uptimeSec = millis() / 1000UL;
    uint32_t freeHeapKb = ESP.getFreeHeap() / 1024UL;

    bool power = m_effectMgr ? m_effectMgr->isPoweredOn() : false;
    uint8_t brightness = m_effectMgr ? m_effectMgr->getBrightness() : 0;
    uint8_t effect = m_effectMgr ? m_effectMgr->getCurrentEffectIndex() : 0;
    const char* effectName = m_effectMgr ? m_effectMgr->getCurrentEffectName() : "Unknown";
    uint8_t speed = m_effectMgr ? m_effectMgr->getSpeed() : 128;
    uint8_t micSens = m_mic ? m_mic->getSensitivity() : (m_effectMgr ? m_effectMgr->getMicSensitivity() : 128);

    int8_t rssi = m_wifi ? m_wifi->getRssi() : 0;
    String ip = m_wifi ? m_wifi->getIpAddress() : "0.0.0.0";
    String ssid = m_wifi ? m_wifi->getSsid() : "";
    bool isAp = m_wifi ? m_wifi->isApMode() : false;
    bool isConnected = m_wifi ? m_wifi->isConnected() : false;

    const char* imuStatus = m_imu ? m_imu->getStatusString() : "Not Found on GPIO 5/6";
    const char* otaStatus = m_ota ? m_ota->getLastStatus() : "Idle";
    const char* latestVer = m_ota ? m_ota->getLatestVersion() : "";
    int otaProg = m_ota ? m_ota->getProgress() : 0;
    bool isUpdating = m_ota ? m_ota->isUpdating() : false;

    snprintf(json, sizeof(json),
        "{\"power\":%s,\"brightness\":%u,\"effect\":%u,\"effectName\":\"%s\","
        "\"speed\":%u,\"micSensitivity\":%u,\"cpuTemp\":%.1f,\"rssi\":%d,"
        "\"ip\":\"%s\",\"ssid\":\"%s\",\"isAp\":%s,\"isConnected\":%s,"
        "\"uptime\":%lu,\"freeHeap\":%lu,\"imuStatus\":\"%s\","
        "\"version\":\"%s\",\"otaStatus\":\"%s\",\"latestVersion\":\"%s\","
        "\"otaProgress\":%d,\"isUpdating\":%s}",
        power ? "true" : "false",
        brightness,
        effect,
        effectName,
        speed,
        micSens,
        cpuTemp,
        rssi,
        ip.c_str(),
        ssid.c_str(),
        isAp ? "true" : "false",
        isConnected ? "true" : "false",
        (unsigned long)uptimeSec,
        (unsigned long)freeHeapKb,
        imuStatus,
        FIRMWARE_VERSION,
        otaStatus,
        latestVer,
        otaProg,
        isUpdating ? "true" : "false"
    );

    m_server.send(200, "application/json", json);
}

void LampWebServer::handleControl() {
    String body = m_server.hasArg("plain") ? m_server.arg("plain") : "";

    bool powerVal = false;
    if (getBoolParam(m_server, body, "power", powerVal)) {
        if (m_effectMgr) {
            if (powerVal) m_effectMgr->powerOn();
            else m_effectMgr->powerOff();
        }
    }

    int brightVal = 0;
    if (getIntParam(m_server, body, "brightness", brightVal)) {
        if (brightVal < 10) brightVal = 10;
        if (brightVal > 255) brightVal = 255;
        if (m_effectMgr) m_effectMgr->setBrightness(brightVal);
    }

    int effectVal = 0;
    if (getIntParam(m_server, body, "effect", effectVal)) {
        if (effectVal >= 0 && effectVal < NUM_CORE_EFFECTS && m_effectMgr && m_matrix) {
            m_effectMgr->setEffect(effectVal, *m_matrix);
        }
    }

    int speedVal = 0;
    if (getIntParam(m_server, body, "speed", speedVal)) {
        if (speedVal < 1) speedVal = 1;
        if (speedVal > 255) speedVal = 255;
        if (m_effectMgr) m_effectMgr->setSpeed(speedVal);
    }

    int micSensVal = 0;
    if (getIntParam(m_server, body, "micSensitivity", micSensVal)) {
        if (micSensVal < 1) micSensVal = 1;
        if (micSensVal > 255) micSensVal = 255;
        if (m_effectMgr) m_effectMgr->setMicSensitivity(micSensVal);
        if (m_mic) m_mic->setSensitivity(micSensVal);
    }

    m_server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void LampWebServer::handleOtaCheck() {
    if (m_ota) {
        m_ota->triggerCheck();
        m_server.send(200, "application/json", "{\"status\":\"triggered\"}");
    } else {
        m_server.send(500, "application/json", "{\"status\":\"error\",\"message\":\"OTA module unavailable\"}");
    }
}

void LampWebServer::handleWifi() {
    String body = m_server.hasArg("plain") ? m_server.arg("plain") : "";
    String ssid = "";
    String pass = "";

    getStringParam(m_server, body, "ssid", ssid);
    getStringParam(m_server, body, "password", pass);
    if (pass.isEmpty()) {
        getStringParam(m_server, body, "pass", pass);
    }

    if (ssid.isEmpty()) {
        m_server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"SSID cannot be empty\"}");
        return;
    }

    if (m_wifi) {
        m_wifi->saveCredentials(ssid, pass);
        m_server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Wi-Fi credentials saved. Reconnecting...\"}");
    } else {
        m_server.send(500, "application/json", "{\"status\":\"error\",\"message\":\"WiFiManager unavailable\"}");
    }
}

void LampWebServer::handleNotFound() {
    if (m_wifi && m_wifi->isApMode()) {
        String target = String("http://") + m_wifi->getIpAddress() + "/";
        m_server.sendHeader("Location", target, true);
        m_server.send(302, "text/plain", "");
    } else {
        m_server.send(404, "text/plain", "404 Not Found");
    }
}
