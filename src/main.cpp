#include <Arduino.h>
#include "config.h"
#include "version.h"
#include "hal/button.h"
#include "hal/led_matrix.h"
#include "hal/mic_i2s.h"
#include "effects/effect_manager.h"
#include "network/wifi_manager.h"
#include "network/ota_github.h"
#include "network/web_server.h"
#include "network/zigbee_manager.h"

static ButtonDriver button(PIN_BUTTON, BUTTON_ACTIVE_LEVEL);
static LedMatrix ledMatrix;
static MicI2S mic(PIN_MIC_SCK, PIN_MIC_WS, PIN_MIC_SD);
static EffectManager effectManager;
static WiFiManager wifiManager;
static OtaGithub otaGithub;
static ZigbeeManager zigbeeManager;
static LampWebServer webServer;
static unsigned long lastTempPrintTime = 0;

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    
    // Allow brief time for USB CDC connection if active
    unsigned long startWait = millis();
    while (!Serial && (millis() - startWait < 2000)) {
        // Non-blocking wait for Serial port to open
    }

    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("   MagicLamp Firmware (ESP32-C6)        "));
    Serial.printf( "   Version: v%s                         \n", FIRMWARE_VERSION);
    Serial.println(F("   Phase 5: 15 Built-in Effects & UX    "));
    Serial.println(F("========================================"));
    Serial.printf("Repo: %s (%s/%s)\n", PROJECT_REPO_URL, PROJECT_REPO_OWNER, PROJECT_REPO_NAME);
    Serial.printf("Timezone: %s (Offset: %d sec)\n", TIMEZONE_NAME, TIMEZONE_OFFSET_SEC);
    Serial.printf("LED Matrix Pin: GPIO %d (%dx%d, %d LEDs, Limit %d mA)\n",
                  PIN_LED_DIN, MATRIX_WIDTH, MATRIX_HEIGHT, NUM_LEDS, CURRENT_LIMIT_MA);
    Serial.printf("Button Pin: GPIO %d (Active Level: %s)\n",
                  PIN_BUTTON, BUTTON_ACTIVE_LEVEL == HIGH ? "HIGH" : "LOW");
    Serial.printf("Mic I2S Pins: SCK=%d, WS=%d, SD=%d\n",
                  PIN_MIC_SCK, PIN_MIC_WS, PIN_MIC_SD);

    // 1. Initialize button driver
    button.begin();
    Serial.println(F("[Button] Driver initialized on GPIO 7 (TTP223, non-blocking FSM)."));

    // 2. Initialize LED matrix driver
    ledMatrix.begin();
    Serial.println(F("[LED] Matrix driver initialized on GPIO 2 (WS2812B 16x16, RMT, 2500mA limit)."));

    // 3. Initialize I2S microphone driver
    if (mic.begin(MIC_SAMPLE_RATE_HZ)) {
        Serial.println(F("[Mic] Driver initialized on I2S (INMP441, DMA, non-blocking)."));
    } else {
        Serial.println(F("[Mic] ERROR: Failed to initialize I2S microphone driver!"));
    }

    // 4. Initialize Effect Manager & NVS preferences
    effectManager.begin(ledMatrix);
    Serial.println(F("[EffectManager] Engine ready with 15 built-in effects and NVS debounce."));

    // 5. Initialize Wi-Fi Manager (STA or SoftAP + Captive Portal + mDNS + ArduinoOTA)
    wifiManager.begin();

    // 6. Initialize GitHub OTA Updater
    otaGithub.init(&ledMatrix, &effectManager);

    // 7. Initialize Zigbee / Radio mode manager
    zigbeeManager.begin(&ledMatrix);

    // 8. Initialize WebServer & WebUI
    webServer.begin(&effectManager, &ledMatrix, &mic, &wifiManager, &otaGithub, &zigbeeManager);

    Serial.println(F("Initialization complete. Entering main loop..."));
}

void loop() {
    unsigned long currentMillis = millis();

    // 1. Poll button driver for events (non-blocking FSM)
    ButtonEvent btnEvent = button.update();
    if (btnEvent != BUTTON_EVENT_NONE) {
        Serial.printf("[Button] Event: %s (id=%d)\n",
                      button.eventToString(btnEvent),
                      static_cast<int>(btnEvent));

        switch (btnEvent) {
            case BUTTON_EVENT_SINGLE_CLICK:
                // Single click: Toggle power (smooth fade in / fade out)
                effectManager.togglePower();
                break;

            case BUTTON_EVENT_DOUBLE_CLICK:
                // Double click: Switch to next effect in circle (0..14)
                effectManager.nextEffect(ledMatrix);
                break;

            case BUTTON_EVENT_TRIPLE_CLICK:
                // Triple click: Trigger GitHub Silent OTA release check
                Serial.println(F("[Control] Triple click: Checking for GitHub OTA updates..."));
                otaGithub.triggerCheck();
                break;

            case BUTTON_EVENT_FOUR_CLICKS:
                // 4 clicks: Toggle radio mode Wi-Fi <-> Zigbee
                Serial.println(F("[Control] 4 clicks: Toggling Radio Mode (Wi-Fi <-> Zigbee)..."));
                if (zigbeeManager.isZigbee() || zigbeeManager.getActiveRadio() == RADIO_MODE_ZIGBEE) {
                    // Switch from Zigbee to Wi-Fi
                    zigbeeManager.stopPairing(&effectManager);
                    zigbeeManager.setMode(RADIO_MODE_WIFI, false, &effectManager, &wifiManager);
                    effectManager.triggerRadioModeAnimation(false);
                    wifiManager.begin();
                    Serial.println(F("[Control] Switched to Wi-Fi mode."));
                } else {
                    // Switch from Wi-Fi to Zigbee with 180s pairing
                    zigbeeManager.setMode(RADIO_MODE_ZIGBEE, false, &effectManager, &wifiManager);
                    effectManager.triggerRadioModeAnimation(true);
                    zigbeeManager.startPairing(&effectManager);
                    Serial.println(F("[Control] Switched to Zigbee mode. Pairing started (180s)..."));
                }
                break;

            case BUTTON_EVENT_SEVEN_CLICKS:
                // 7 clicks: Wi-Fi reset & restore network defaults
                Serial.println(F("[Control] 7 clicks: Wi-Fi credentials reset requested."));
                wifiManager.resetSettings();
                break;

            case BUTTON_EVENT_LONG_PRESS_HOLD:
                // Long press hold: tick every 40 ms after 700 ms for real-time smooth dimming
                if (button.isFirstHoldTick()) {
                    effectManager.toggleDimmerDirection();
                    Serial.println(F("[Control] Long press hold start: dimmer direction toggled."));
                }
                effectManager.stepDimmer(ledMatrix);
                break;

            default:
                break;
        }
    }

    // 2. Poll microphone audio processing (non-blocking DMA read, ~10-30 us)
    mic.update();

    // 3. Update Wi-Fi manager state machine and network services
    wifiManager.update();

    // 4. Update WebServer client requests
    webServer.update();

    // 5. Update Zigbee / Radio mode manager
    zigbeeManager.update(&wifiManager, &effectManager, &ledMatrix, &otaGithub);

    // 6. Update active effect animation or OTA progress (~60 FPS)
    effectManager.update(ledMatrix, mic);
    otaGithub.update(&ledMatrix);

    // 7. Telemetry: Print CPU temperature, network, and Effect metrics every TEMP_PRINT_INTERVAL_MS
    if (currentMillis - lastTempPrintTime >= TEMP_PRINT_INTERVAL_MS) {
        lastTempPrintTime = currentMillis;
        float cpuTemp = temperatureRead();
        Serial.printf("[Telemetry] Uptime: %lu s | CPU: %.1f °C | Radio: %s | Wi-Fi: %s (%s, RSSI: %d) | OTA: %s | Power: %s | Effect: [%d] %s | Bright: %d | Speed: %d | MicSens: %d | Mic: %.2f | NVS Dirty: %s\n",
                      currentMillis / 1000UL,
                      cpuTemp,
                      zigbeeManager.getModeString(),
                      wifiManager.isApMode() ? "AP" : (wifiManager.isConnected() ? "STA" : "DISC"),
                      wifiManager.getIpAddress().c_str(),
                      wifiManager.getRssi(),
                      otaGithub.getLastStatus(),
                      effectManager.isPoweredOn() ? "ON" : "OFF",
                      effectManager.getCurrentEffectIndex(),
                      effectManager.getCurrentEffectName(),
                      effectManager.getBrightness(),
                      effectManager.getSpeed(),
                      effectManager.getMicSensitivity(),
                      mic.getNormalizedLevel(),
                      effectManager.isNvsDirty() ? "YES" : "NO");
    }
}


