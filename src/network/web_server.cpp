#include "web_server.h"
#include "webui_html.h"
#include "version.h"
#include "effects/effect_manager.h"
#include "hal/led_matrix.h"
#include "hal/mic_i2s.h"
#include "wifi_manager.h"
#include "ota_github.h"
#include "zigbee_manager.h"
#include <Update.h>

LampWebServer::LampWebServer()
    : m_server(80),
      m_effectMgr(nullptr),
      m_matrix(nullptr),
      m_mic(nullptr),
      m_wifi(nullptr),
      m_ota(nullptr),
      m_zigbee(nullptr),
      m_rebootPending(false),
      m_rebootTime(0),
      m_lastHttpActivityMs(0) {}

void LampWebServer::noteHttpActivity() {
    m_lastHttpActivityMs = millis();
    if (m_zigbee) {
        m_zigbee->noteHttpActivity();
    }
}

void LampWebServer::begin(EffectManager* effectMgr,
                          LedMatrix* matrix,
                          MicI2S* mic,
                          WiFiManager* wifi,
                          OtaGithub* ota,
                          ZigbeeManager* zigbee) {
    m_effectMgr = effectMgr;
    m_matrix = matrix;
    m_mic = mic;
    m_wifi = wifi;
    m_ota = ota;
    m_zigbee = zigbee;
    m_rebootPending = false;
    m_rebootTime = 0;
    m_lastHttpActivityMs = millis();

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

    // Favicon
    m_server.on("/favicon.ico", HTTP_GET, [this]() {
        static const char FAVICON_SVG[] PROGMEM = 
            "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'>"
            "<defs><filter id='g' x='-20%' y='-20%' width='140%' height='140%'>"
            "<feGaussianBlur stdDeviation='1.5' result='b'/>"
            "<feMerge><feMergeNode in='b'/><feMergeNode in='SourceGraphic'/></feMerge>"
            "</filter></defs>"
            "<rect width='32' height='32' rx='6' fill='#090d16'/>"
            "<path d='M16 4 C11 4 8 8 8 13 C8 17 11 20 12 22 L20 22 C21 20 24 17 24 13 C24 8 21 4 16 4 Z' fill='none' stroke='#38bdf8' stroke-width='2' filter='url(#g)'/>"
            "<path d='M13 13 Q16 7 19 13 Q16 19 13 13' fill='#818cf8' opacity='0.8' filter='url(#g)'/>"
            "<rect x='13' y='23' width='6' height='2' rx='1' fill='#38bdf8'/>"
            "<rect x='14' y='26' width='4' height='2' rx='1' fill='#94a3b8'/>"
            "</svg>";
        m_server.send(200, "image/svg+xml", FAVICON_SVG);
    });

    // REST API
    m_server.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    m_server.on("/api/control", HTTP_POST, [this]() { handleControl(); });
    m_server.on("/api/schedule", HTTP_GET, [this]() { handleSchedule(); });
    m_server.on("/api/schedule", HTTP_POST, [this]() { handleSchedule(); });
    m_server.on("/api/schedule/sleep_now", HTTP_POST, [this]() { handleSleepNow(); });
    m_server.on("/api/radio/switch", HTTP_POST, [this]() { handleRadioSwitch(); });
    m_server.on("/api/ota/check", HTTP_POST, [this]() { handleOtaCheck(); });
    m_server.on("/api/wifi", HTTP_POST, [this]() { handleWifi(); });

    // Browser OTA Upload Handler via Update.h
    m_server.on("/api/ota/upload", HTTP_POST, [this]() {
        noteHttpActivity();
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
            if (m_effectMgr) {
                m_effectMgr->setOtaState(EffectManager::OTA_STATE_DOWNLOADING, 0);
            }
            Update.onProgress([this](size_t progress, size_t total) {
                if (m_effectMgr && total > 0) {
                    uint8_t pct = (progress * 100) / total;
                    m_effectMgr->setOtaState(EffectManager::OTA_STATE_DOWNLOADING, pct);
                }
                yield();
            });
            if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
                Update.printError(Serial);
                if (m_effectMgr) m_effectMgr->setOtaState(EffectManager::OTA_STATE_ERROR);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            yield();
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
                if (m_effectMgr) m_effectMgr->setOtaState(EffectManager::OTA_STATE_ERROR);
            }
            yield();
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) {
                Serial.printf("[WebOTA] Success: %u bytes written. Reboot pending.\n", upload.totalSize);
                if (m_effectMgr) {
                    m_effectMgr->setOtaState(EffectManager::OTA_STATE_SUCCESS, 100);
                }
            } else {
                Update.printError(Serial);
                if (m_effectMgr) m_effectMgr->setOtaState(EffectManager::OTA_STATE_ERROR);
            }
        } else if (upload.status == UPLOAD_FILE_ABORTED) {
            Update.end();
            if (m_effectMgr) m_effectMgr->setOtaState(EffectManager::OTA_STATE_ERROR);
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
    noteHttpActivity();
    m_server.send_P(200, "text/html; charset=utf-8", WEBUI_HTML);
}

void LampWebServer::handleStatus() {
    noteHttpActivity();
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

    const char* otaStatus = m_ota ? m_ota->getLastStatus() : "Idle";
    const char* latestVer = m_ota ? m_ota->getLatestVersion() : "";
    int otaProg = m_ota ? m_ota->getProgress() : 0;
    bool isUpdating = m_ota ? m_ota->isUpdating() : false;

    uint8_t totalEff = m_effectMgr ? m_effectMgr->getTotalEffects() : NUM_CORE_EFFECTS;
    String effectsArray = "[";
    if (m_effectMgr) {
        for (uint8_t i = 0; i < totalEff; i++) {
            if (i > 0) effectsArray += ",";
            EffectProfile prof = m_effectMgr->getProfile(i);
            effectsArray += "{\"index\":" + String(i) +
                            ",\"name\":\"" + String(m_effectMgr->getEffectName(i)) + "\"" +
                            ",\"brightness\":" + String(prof.brightness) +
                            ",\"speed\":" + String(prof.speed) +
                            ",\"sensitivity\":" + String(prof.sensitivity) +
                            ",\"param1\":" + String(prof.param1) +
                            ",\"inCycle\":" + String(prof.inCycle ? "true" : "false") + "}";
        }
    }
    effectsArray += "]";

    time_t rawtime;
    time(&rawtime);
    bool ntpSynced = (rawtime > 100000);
    String currentTime = "--:--:--";
    if (ntpSynced) {
        time_t local = rawtime + TIMEZONE_OFFSET_SEC;
        struct tm* ti = gmtime(&local);
        if (ti) {
            char tbuf[16];
            snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%02d", ti->tm_hour, ti->tm_min, ti->tm_sec);
            currentTime = String(tbuf);
        }
    }
    String formattedTime = ntpSynced ? currentTime.substring(0, 5) : "--:--";
    ScheduleConfig sch = m_effectMgr ? m_effectMgr->getSchedule() : ScheduleConfig();
    String schedStatus = m_effectMgr ? m_effectMgr->getScheduleStatus() : "Расписание выключено";
    bool sleepNowActive = m_effectMgr ? m_effectMgr->isSleepNowActive() : false;

    char dawnBuf[8], duskBuf[8];
    snprintf(dawnBuf, sizeof(dawnBuf), "%02d:%02d", sch.dawnHour, sch.dawnMinute);
    snprintf(duskBuf, sizeof(duskBuf), "%02d:%02d", sch.duskHour, sch.duskMinute);

    const char* radioMode = m_zigbee ? m_zigbee->getModeString() : "wifi";
    uint16_t cycleMask = m_effectMgr ? m_effectMgr->getCycleMask() : 0xFFFF;
    uint8_t activeParam1 = m_effectMgr ? m_effectMgr->getProfile(effect).param1 : 90;
    uint8_t nightlightTemp = m_effectMgr ? m_effectMgr->getProfile(0).param1 : 90;

    String jsonStr = "{\"power\":" + String(power ? "true" : "false") +
                     ",\"brightness\":" + String(brightness) +
                     ",\"effect\":" + String(effect) +
                     ",\"effectName\":\"" + String(effectName) + "\"" +
                     ",\"speed\":" + String(speed) +
                     ",\"micSensitivity\":" + String(micSens) +
                     ",\"param1\":" + String(activeParam1) +
                     ",\"colorTemp\":" + String(nightlightTemp) +
                     ",\"radio_mode\":\"" + String(radioMode) + "\"" +
                     ",\"cycle_mask\":" + String(cycleMask) +
                     ",\"time\":\"" + formattedTime + "\"" +
                     ",\"currentTime\":\"" + currentTime + "\"" +
                     ",\"ntpSynced\":" + String(ntpSynced ? "true" : "false") +
                     ",\"scheduleEnabled\":" + String(sch.isEnabled() ? "true" : "false") +
                     ",\"dawnEnabled\":" + String(sch.dawnEnabled ? "true" : "false") +
                     ",\"duskEnabled\":" + String(sch.duskEnabled ? "true" : "false") +
                     ",\"dawnTime\":\"" + String(dawnBuf) + "\"" +
                     ",\"duskTime\":\"" + String(duskBuf) + "\"" +
                     ",\"dawnHour\":" + String(sch.dawnHour) +
                     ",\"dawnMinute\":" + String(sch.dawnMinute) +
                     ",\"dawnDurationMin\":" + String(sch.dawnDurationMin) +
                     ",\"duskHour\":" + String(sch.duskHour) +
                     ",\"duskMinute\":" + String(sch.duskMinute) +
                     ",\"duskDurationMin\":" + String(sch.duskDurationMin) +
                     ",\"scheduleStatus\":\"" + schedStatus + "\"" +
                     ",\"sleepNowActive\":" + String(sleepNowActive ? "true" : "false") +
                     ",\"cpuTemp\":" + String(cpuTemp, 1) +
                     ",\"rssi\":" + String(rssi) +
                     ",\"ip\":\"" + ip + "\"" +
                     ",\"ssid\":\"" + ssid + "\"" +
                     ",\"isAp\":" + String(isAp ? "true" : "false") +
                     ",\"isConnected\":" + String(isConnected ? "true" : "false") +
                     ",\"uptime\":" + String((unsigned long)uptimeSec) +
                     ",\"freeHeap\":" + String((unsigned long)freeHeapKb) +
                     ",\"version\":\"" + String(FIRMWARE_VERSION) + "\"" +
                     ",\"otaStatus\":\"" + String(otaStatus) + "\"" +
                     ",\"latestVersion\":\"" + String(latestVer) + "\"" +
                     ",\"otaProgress\":" + String(otaProg) +
                     ",\"isUpdating\":" + String(isUpdating ? "true" : "false") +
                     ",\"totalEffects\":" + String(totalEff) +
                     ",\"effects\":" + effectsArray + "}";

    m_server.send(200, "application/json", jsonStr);
}

void LampWebServer::handleControl() {
    noteHttpActivity();
    String body = m_server.hasArg("plain") ? m_server.arg("plain") : "";

    // 1. Power control
    bool powerVal = false;
    if (getBoolParam(m_server, body, "power", powerVal)) {
        if (m_effectMgr) {
            if (powerVal) m_effectMgr->powerOn();
            else m_effectMgr->powerOff();
        }
    }

    // 2. Cycle mask control
    int cycleMaskVal = 0;
    if (getIntParam(m_server, body, "cycle_mask", cycleMaskVal) || getIntParam(m_server, body, "cycleMask", cycleMaskVal)) {
        if (m_effectMgr) {
            m_effectMgr->setCycleMask(static_cast<uint16_t>(cycleMaskVal));
        }
    }

    // 3. Effect parameters / Profile update
    int effectVal = -1;
    bool hasEffect = getIntParam(m_server, body, "effect", effectVal) ||
                     getIntParam(m_server, body, "effectIndex", effectVal) ||
                     getIntParam(m_server, body, "effect_index", effectVal);

    int brightVal = 0;
    bool hasBright = getIntParam(m_server, body, "brightness", brightVal);

    int speedVal = 0;
    bool hasSpeed = getIntParam(m_server, body, "speed", speedVal);

    int micSensVal = 0;
    bool hasMicSens = getIntParam(m_server, body, "micSensitivity", micSensVal) ||
                      getIntParam(m_server, body, "sensitivity", micSensVal);

    int param1Val = 0;
    bool hasParam1 = getIntParam(m_server, body, "param1", param1Val) ||
                     getIntParam(m_server, body, "colorTemp", param1Val) ||
                     getIntParam(m_server, body, "color_temp", param1Val);

    bool inCycleVal = false;
    bool hasInCycle = getBoolParam(m_server, body, "inCycle", inCycleVal);

    if (m_effectMgr) {
        uint8_t totalEff = m_effectMgr->getTotalEffects();

        if (hasEffect && effectVal >= 0 && effectVal < totalEff) {
            // Update individual effect profile if parameters provided
            if (hasInCycle || hasBright || hasSpeed || hasMicSens || hasParam1) {
                EffectProfile prof = m_effectMgr->getProfile(effectVal);
                if (hasInCycle) prof.inCycle = inCycleVal;
                if (hasBright) prof.brightness = constrain(brightVal, 10, 255);
                if (hasSpeed) prof.speed = constrain(speedVal, 1, 255);
                if (hasMicSens) prof.sensitivity = constrain(micSensVal, 1, 255);
                if (hasParam1) prof.param1 = constrain(param1Val, 0, 255);
                m_effectMgr->setProfile(effectVal, prof);
            }

            // If request had "effect": index without profile fields or explicitly switching effect:
            if (!hasInCycle && !hasBright && !hasSpeed && !hasMicSens && !hasParam1) {
                if (m_matrix) {
                    m_effectMgr->setEffect(effectVal, *m_matrix);
                }
            }
        } else {
            // No specific effect target -> apply to active effect
            uint8_t curEff = m_effectMgr->getCurrentEffectIndex();
            EffectProfile prof = m_effectMgr->getProfile(curEff);
            bool profChanged = false;

            if (hasBright) {
                prof.brightness = constrain(brightVal, 10, 255);
                m_effectMgr->setBrightness(prof.brightness);
                profChanged = true;
            }
            if (hasSpeed) {
                prof.speed = constrain(speedVal, 1, 255);
                m_effectMgr->setSpeed(prof.speed);
                profChanged = true;
            }
            if (hasMicSens) {
                prof.sensitivity = constrain(micSensVal, 1, 255);
                m_effectMgr->setMicSensitivity(prof.sensitivity);
                if (m_mic) m_mic->setSensitivity(prof.sensitivity);
                profChanged = true;
            }
            if (hasParam1) {
                prof.param1 = constrain(param1Val, 0, 255);
                profChanged = true;
            }
            if (hasInCycle) {
                prof.inCycle = inCycleVal;
                profChanged = true;
            }
            if (profChanged) {
                m_effectMgr->setProfile(curEff, prof);
            }
        }
    }

    // Schedule parameters
    if (m_effectMgr) {
        bool sleepNowVal = false;
        if (getBoolParam(m_server, body, "sleepNow", sleepNowVal) || getBoolParam(m_server, body, "sleep_now", sleepNowVal)) {
            if (sleepNowVal && m_matrix) {
                m_effectMgr->startSleepNow(*m_matrix);
            } else {
                m_effectMgr->stopSleepNow();
            }
        }

        ScheduleConfig sch = m_effectMgr->getSchedule();
        bool scheduleChanged = false;

        bool dawnEn = false, duskEn = false, schEn = false;
        if (getBoolParam(m_server, body, "dawnEnabled", dawnEn)) {
            sch.dawnEnabled = dawnEn;
            scheduleChanged = true;
        }
        if (getBoolParam(m_server, body, "duskEnabled", duskEn)) {
            sch.duskEnabled = duskEn;
            scheduleChanged = true;
        }
        if (getBoolParam(m_server, body, "scheduleEnabled", schEn)) {
            sch.dawnEnabled = schEn;
            sch.duskEnabled = schEn;
            sch.enabled = schEn;
            scheduleChanged = true;
        }

        int dh = 0, dm = 0, sh = 0, sm = 0;
        if (getIntParam(m_server, body, "dawnHour", dh)) {
            sch.dawnHour = constrain(dh, 0, 23);
            scheduleChanged = true;
        }
        if (getIntParam(m_server, body, "dawnMinute", dm)) {
            sch.dawnMinute = constrain(dm, 0, 59);
            scheduleChanged = true;
        }
        if (getIntParam(m_server, body, "duskHour", sh)) {
            sch.duskHour = constrain(sh, 0, 23);
            scheduleChanged = true;
        }
        if (getIntParam(m_server, body, "duskMinute", sm)) {
            sch.duskMinute = constrain(sm, 0, 59);
            scheduleChanged = true;
        }

        int dawnDur = 0, duskDur = 0;
        if (getIntParam(m_server, body, "dawnDurationMin", dawnDur) || getIntParam(m_server, body, "dawn_dur", dawnDur) || getIntParam(m_server, body, "dawnDuration", dawnDur)) {
            sch.dawnDurationMin = constrain(dawnDur, 1, 120);
            scheduleChanged = true;
        }
        if (getIntParam(m_server, body, "duskDurationMin", duskDur) || getIntParam(m_server, body, "dusk_dur", duskDur) || getIntParam(m_server, body, "duskDuration", duskDur)) {
            sch.duskDurationMin = constrain(duskDur, 1, 180);
            scheduleChanged = true;
        }

        String dawnStr = "", duskStr = "";
        if (getStringParam(m_server, body, "dawnTime", dawnStr)) {
            int h = 0, m = 0;
            if (sscanf(dawnStr.c_str(), "%d:%d", &h, &m) >= 2) {
                sch.dawnHour = constrain(h, 0, 23);
                sch.dawnMinute = constrain(m, 0, 59);
                scheduleChanged = true;
            }
        }
        if (getStringParam(m_server, body, "duskTime", duskStr)) {
            int h = 0, m = 0;
            if (sscanf(duskStr.c_str(), "%d:%d", &h, &m) >= 2) {
                sch.duskHour = constrain(h, 0, 23);
                sch.duskMinute = constrain(m, 0, 59);
                scheduleChanged = true;
            }
        }

        if (scheduleChanged) {
            m_effectMgr->setSchedule(sch);
        }
    }

    m_server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void LampWebServer::handleSchedule() {
    noteHttpActivity();
    if (m_server.method() == HTTP_GET) {
        time_t rawtime;
        time(&rawtime);
        bool ntpSynced = (rawtime > 100000);
        String currentTime = "--:--:--";
        if (ntpSynced) {
            time_t local = rawtime + TIMEZONE_OFFSET_SEC;
            struct tm* ti = gmtime(&local);
            if (ti) {
                char tbuf[16];
                snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%02d", ti->tm_hour, ti->tm_min, ti->tm_sec);
                currentTime = String(tbuf);
            }
        }

        ScheduleConfig sch = m_effectMgr ? m_effectMgr->getSchedule() : ScheduleConfig();
        String schedStatus = m_effectMgr ? m_effectMgr->getScheduleStatus() : "Расписание выключено";
        bool sleepNowActive = m_effectMgr ? m_effectMgr->isSleepNowActive() : false;

        char dawnBuf[8], duskBuf[8];
        snprintf(dawnBuf, sizeof(dawnBuf), "%02d:%02d", sch.dawnHour, sch.dawnMinute);
        snprintf(duskBuf, sizeof(duskBuf), "%02d:%02d", sch.duskHour, sch.duskMinute);

        String json = "{\"status\":\"ok\""
                      ",\"currentTime\":\"" + currentTime + "\""
                      ",\"ntpSynced\":" + String(ntpSynced ? "true" : "false") +
                      ",\"scheduleEnabled\":" + String(sch.isEnabled() ? "true" : "false") +
                      ",\"dawnEnabled\":" + String(sch.dawnEnabled ? "true" : "false") +
                      ",\"duskEnabled\":" + String(sch.duskEnabled ? "true" : "false") +
                      ",\"dawnTime\":\"" + String(dawnBuf) + "\""
                      ",\"duskTime\":\"" + String(duskBuf) + "\""
                      ",\"dawnHour\":" + String(sch.dawnHour) +
                      ",\"dawnMinute\":" + String(sch.dawnMinute) +
                      ",\"dawnDurationMin\":" + String(sch.dawnDurationMin) +
                      ",\"duskHour\":" + String(sch.duskHour) +
                      ",\"duskMinute\":" + String(sch.duskMinute) +
                      ",\"duskDurationMin\":" + String(sch.duskDurationMin) +
                      ",\"scheduleStatus\":\"" + schedStatus + "\""
                      ",\"sleepNowActive\":" + String(sleepNowActive ? "true" : "false") + "}";

        m_server.send(200, "application/json", json);
        return;
    }

    // POST /api/schedule
    String body = m_server.hasArg("plain") ? m_server.arg("plain") : "";
    if (m_effectMgr) {
        bool sleepNowVal = false;
        if (getBoolParam(m_server, body, "sleepNow", sleepNowVal) || getBoolParam(m_server, body, "sleep_now", sleepNowVal)) {
            if (sleepNowVal && m_matrix) {
                m_effectMgr->startSleepNow(*m_matrix);
            } else {
                m_effectMgr->stopSleepNow();
            }
        }

        ScheduleConfig sch = m_effectMgr->getSchedule();
        bool changed = false;

        bool dawnEn = false, duskEn = false;
        if (getBoolParam(m_server, body, "dawnEnabled", dawnEn)) {
            sch.dawnEnabled = dawnEn;
            changed = true;
        }
        if (getBoolParam(m_server, body, "duskEnabled", duskEn)) {
            sch.duskEnabled = duskEn;
            changed = true;
        }

        int dh = 0, dm = 0, sh = 0, sm = 0;
        if (getIntParam(m_server, body, "dawnHour", dh)) {
            sch.dawnHour = constrain(dh, 0, 23);
            changed = true;
        }
        if (getIntParam(m_server, body, "dawnMinute", dm)) {
            sch.dawnMinute = constrain(dm, 0, 59);
            changed = true;
        }
        if (getIntParam(m_server, body, "duskHour", sh)) {
            sch.duskHour = constrain(sh, 0, 23);
            changed = true;
        }
        if (getIntParam(m_server, body, "duskMinute", sm)) {
            sch.duskMinute = constrain(sm, 0, 59);
            changed = true;
        }

        int dawnDur = 0, duskDur = 0;
        if (getIntParam(m_server, body, "dawnDurationMin", dawnDur) || getIntParam(m_server, body, "dawn_dur", dawnDur) || getIntParam(m_server, body, "dawnDuration", dawnDur)) {
            sch.dawnDurationMin = constrain(dawnDur, 1, 120);
            changed = true;
        }
        if (getIntParam(m_server, body, "duskDurationMin", duskDur) || getIntParam(m_server, body, "dusk_dur", duskDur) || getIntParam(m_server, body, "duskDuration", duskDur)) {
            sch.duskDurationMin = constrain(duskDur, 1, 180);
            changed = true;
        }

        String dawnStr = "", duskStr = "";
        if (getStringParam(m_server, body, "dawnTime", dawnStr)) {
            int h = 0, m = 0;
            if (sscanf(dawnStr.c_str(), "%d:%d", &h, &m) >= 2) {
                sch.dawnHour = constrain(h, 0, 23);
                sch.dawnMinute = constrain(m, 0, 59);
                changed = true;
            }
        }
        if (getStringParam(m_server, body, "duskTime", duskStr)) {
            int h = 0, m = 0;
            if (sscanf(duskStr.c_str(), "%d:%d", &h, &m) >= 2) {
                sch.duskHour = constrain(h, 0, 23);
                sch.duskMinute = constrain(m, 0, 59);
                changed = true;
            }
        }

        if (changed) {
            m_effectMgr->setSchedule(sch);
        }
    }

    m_server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void LampWebServer::handleSleepNow() {
    noteHttpActivity();
    if (m_effectMgr && m_matrix) {
        String body = m_server.hasArg("plain") ? m_server.arg("plain") : "";
        bool enable = true;
        if (getBoolParam(m_server, body, "enable", enable)) {
            if (!enable) {
                m_effectMgr->stopSleepNow();
                m_server.send(200, "application/json", "{\"status\":\"ok\",\"sleepNowActive\":false}");
                return;
            }
        }
        m_effectMgr->startSleepNow(*m_matrix);
        m_server.send(200, "application/json", "{\"status\":\"ok\",\"sleepNowActive\":true}");
    } else {
        m_server.send(500, "application/json", "{\"status\":\"error\",\"message\":\"EffectManager unavailable\"}");
    }
}

void LampWebServer::handleRadioSwitch() {
    noteHttpActivity();
    if (!m_zigbee) {
        m_server.send(500, "application/json", "{\"status\":\"error\",\"message\":\"ZigbeeManager unavailable\"}");
        return;
    }

    String body = m_server.hasArg("plain") ? m_server.arg("plain") : "";
    String modeStr = "";
    uint8_t targetMode = RADIO_MODE_WIFI;
    if (getStringParam(m_server, body, "mode", modeStr)) {
        if (modeStr == "wifi") {
            targetMode = RADIO_MODE_WIFI;
        } else if (modeStr == "zigbee") {
            targetMode = RADIO_MODE_ZIGBEE;
        }
    } else {
        targetMode = (m_zigbee->getMode() == RADIO_MODE_WIFI) ? RADIO_MODE_ZIGBEE : RADIO_MODE_WIFI;
    }

    m_zigbee->setMode(targetMode, false, m_effectMgr, m_wifi);

    if (m_effectMgr) {
        m_effectMgr->triggerRadioModeAnimation(targetMode == RADIO_MODE_ZIGBEE);
    }

    String res = "{\"status\":\"ok\",\"radio_mode\":\"" + String(m_zigbee->getModeString()) + "\"}";
    m_server.send(200, "application/json", res);
}

void LampWebServer::handleOtaCheck() {
    noteHttpActivity();
    if (m_ota) {
        m_ota->triggerCheck();
        m_server.send(200, "application/json", "{\"status\":\"triggered\"}");
    } else {
        m_server.send(500, "application/json", "{\"status\":\"error\",\"message\":\"OTA module unavailable\"}");
    }
}

void LampWebServer::handleWifi() {
    noteHttpActivity();
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
    noteHttpActivity();
    if (m_wifi && m_wifi->isApMode()) {
        String target = String("http://") + m_wifi->getIpAddress() + "/";
        m_server.sendHeader("Location", target, true);
        m_server.send(302, "text/plain", "");
    } else {
        m_server.send(404, "text/plain", "404 Not Found");
    }
}

