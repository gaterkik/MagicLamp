#pragma once

#include <Arduino.h>
#include <WebServer.h>

class EffectManager;
class LedMatrix;
class MicI2S;
class ImuMpu6050;
class WiFiManager;
class OtaGithub;
class StoreManager;

/**
 * @file web_server.h
 * @brief Embedded WebServer for MagicLamp (ESP32-C6) on port 80.
 *
 * Implements:
 * - Single Page Application WebUI served from PROGMEM (GET /).
 * - Telemetry & status reporting (GET /api/status).
 * - Real-time lamp control (POST /api/control).
 * - GitHub Silent OTA trigger (POST /api/ota/check).
 * - Direct browser firmware upload via Update.h (POST /api/ota/upload).
 * - GitHub Effect Store endpoints (GET /api/store/installed, GET /api/store/catalog, POST /api/store/install, POST /api/store/delete).
 * - Wi-Fi credentials configuration (POST /api/wifi).
 * - Captive Portal redirection when in SoftAP setup mode.
 */
class LampWebServer {
public:
    LampWebServer();
    ~LampWebServer() = default;

    /**
     * @brief Initialize web server, configure REST routes and start listening.
     */
    void begin(EffectManager* effectMgr,
               LedMatrix* matrix,
               MicI2S* mic,
               ImuMpu6050* imu,
               WiFiManager* wifi,
               OtaGithub* ota,
               StoreManager* store = nullptr);

    /**
     * @brief Process incoming client HTTP requests. Call frequently in loop().
     */
    void update();

private:
    WebServer m_server;

    EffectManager* m_effectMgr;
    LedMatrix* m_matrix;
    MicI2S* m_mic;
    ImuMpu6050* m_imu;
    WiFiManager* m_wifi;
    OtaGithub* m_ota;
    StoreManager* m_store;

    bool m_rebootPending;
    unsigned long m_rebootTime;

    void registerRoutes();

    void handleRoot();
    void handleStatus();
    void handleControl();
    void handleOtaCheck();
    void handleWifi();
    void handleStoreInstalled();
    void handleStoreCatalog();
    void handleStoreInstall();
    void handleStoreDelete();
    void handleNotFound();
};

