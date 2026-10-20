#include "store_manager.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

StoreManager::StoreManager()
    : m_mounted(false),
      m_effectMgr(nullptr) {}

bool StoreManager::begin(EffectManager* effectMgr) {
    m_effectMgr = effectMgr;

    // Mount LittleFS (formatOnFail = true)
    m_mounted = LittleFS.begin(true);
    if (!m_mounted) {
        Serial.println(F("[Store] ERROR: LittleFS mount failed!"));
        return false;
    }

    Serial.printf("[Store] LittleFS mounted successfully. Total: %u KB, Used: %u KB\n",
                  (unsigned int)(getTotalBytes() / 1024),
                  (unsigned int)(getUsedBytes() / 1024));

    // Ensure /effects directory exists
    if (!LittleFS.exists("/effects")) {
        LittleFS.mkdir("/effects");
        Serial.println(F("[Store] Created /effects directory in LittleFS."));
    }

    if (m_effectMgr) {
        scanAndLoadEffects(*m_effectMgr);
    }

    return true;
}

void StoreManager::scanAndLoadEffects(EffectManager& effectMgr) {
    if (!m_mounted) return;

    // Reset custom slots to 10 core effects
    effectMgr.clearCustomEffects();

    File root = LittleFS.open("/effects");
    if (!root || !root.isDirectory()) {
        Serial.println(F("[Store] No /effects directory found during scan."));
        return;
    }

    File file = root.openNextFile();
    uint8_t loadedCount = 0;
    while (file) {
        if (!file.isDirectory()) {
            String name = String(file.name());
            if (name.endsWith(".eff")) {
                String fullPath = name.startsWith("/effects/") ? name : (String("/effects/") + name);
                EffectVM* vm = new EffectVM();
                if (vm->loadFromFile(fullPath.c_str())) {
                    if (effectMgr.registerCustomEffect(vm)) {
                        loadedCount++;
                    } else {
                        delete vm;
                        Serial.println(F("[Store] Max custom effects capacity reached (24)."));
                        break;
                    }
                } else {
                    delete vm;
                    Serial.printf("[Store] Skipping invalid effect file: %s\n", fullPath.c_str());
                }
            }
        }
        file = root.openNextFile();
    }

    Serial.printf("[Store] Scan complete. %d custom effects loaded (Total active: %d)\n",
                  loadedCount, effectMgr.getTotalEffects());
}

String StoreManager::getDefaultEffectUrl(const String& effectId) const {
    return String("https://raw.githubusercontent.com/gaterkik/MagicLamp/main/store/effects/") + effectId + ".eff";
}

bool StoreManager::downloadFile(const String& url, String& outContent) {
    int code = 0;
    for (uint8_t attempt = 1; attempt <= 3; attempt++) {
        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(15000);
        client.setHandshakeTimeout(15);

        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.setTimeout(15000);
        http.setUserAgent("MagicLamp-Store/0.5.1");

        Serial.printf("[Store] Downloading (attempt %u/3): %s\n", attempt, url.c_str());
        if (!http.begin(client, url)) {
            Serial.printf("[Store] http.begin failed on attempt %u/3\n", attempt);
            if (attempt < 3) {
                delay(1500);
                continue;
            }
            return false;
        }

        code = http.GET();
        if (code < 0) {
            Serial.printf("[Store] Connection error %d on attempt %u/3: %s\n",
                          code, attempt, http.errorToString(code).c_str());
            http.end();
            client.stop();
            if (attempt < 3) {
                delay(1500);
                continue;
            }
        } else {
            if (code == HTTP_CODE_OK) {
                outContent = http.getString();
                http.end();
                client.stop();
                return true;
            }
            Serial.printf("[Store] HTTP response status code %d for %s\n", code, url.c_str());
            http.end();
            client.stop();
            break;
        }
    }
    return false;
}

bool StoreManager::installEffect(const String& effectId, const String& url, EffectManager* effectMgr, String& outMessage) {
    if (!m_mounted) {
        outMessage = "LittleFS is not mounted";
        return false;
    }

    if (effectId.isEmpty()) {
        outMessage = "Effect ID cannot be empty";
        return false;
    }

    String downloadUrl = url.isEmpty() ? getDefaultEffectUrl(effectId) : url;
    Serial.printf("[Store] Downloading effect '%s' from: %s\n", effectId.c_str(), downloadUrl.c_str());

    String payload;
    if (!downloadFile(downloadUrl, payload)) {
        outMessage = "Download failed (network error, timeout or HTTP error)";
        Serial.printf("[Store] ERROR: Failed to download %s\n", downloadUrl.c_str());
        return false;
    }

    if (payload.length() < 10) {
        outMessage = "Downloaded file payload is too small or empty";
        return false;
    }

    // Validate effect descriptor with a temporary EffectVM
    EffectVM tempVM;
    if (!tempVM.parseDescriptor(payload)) {
        outMessage = "Invalid .eff descriptor format or unknown engine";
        Serial.println(F("[Store] ERROR: Effect descriptor validation failed"));
        return false;
    }

    String filePath = String("/effects/") + effectId + ".eff";
    File file = LittleFS.open(filePath, "w");
    if (!file) {
        outMessage = "Failed to write file to LittleFS";
        Serial.printf("[Store] ERROR: Cannot open %s for writing\n", filePath.c_str());
        return false;
    }

    file.print(payload);
    file.close();
    Serial.printf("[Store] Successfully wrote %d bytes to %s\n", payload.length(), filePath.c_str());

    // Rescan and update EffectManager
    if (effectMgr) {
        scanAndLoadEffects(*effectMgr);
    }

    outMessage = "Effect installed successfully";
    return true;
}

bool StoreManager::deleteEffect(const String& effectId, EffectManager* effectMgr) {
    if (!m_mounted || effectId.isEmpty()) return false;

    String filePath = String("/effects/") + effectId + ".eff";
    if (!LittleFS.exists(filePath)) {
        Serial.printf("[Store] Delete skipped: %s does not exist\n", filePath.c_str());
        return false;
    }

    bool ok = LittleFS.remove(filePath);
    if (ok) {
        Serial.printf("[Store] Deleted effect file: %s\n", filePath.c_str());
        if (effectMgr) {
            scanAndLoadEffects(*effectMgr);
        }
    }
    return ok;
}

bool StoreManager::isEffectInstalled(const String& effectId) const {
    if (!m_mounted || effectId.isEmpty()) return false;
    String filePath = String("/effects/") + effectId + ".eff";
    return LittleFS.exists(filePath);
}

bool StoreManager::fetchCatalogJson(String& outJson) {
    String catalogUrl = "https://raw.githubusercontent.com/gaterkik/MagicLamp/main/store/manifest.json";
    Serial.printf("[Store] Fetching catalog from %s\n", catalogUrl.c_str());
    return downloadFile(catalogUrl, outJson);
}

String StoreManager::getInstalledJson() const {
    size_t total = getTotalBytes();
    size_t used = getUsedBytes();

    String json = "{\"totalBytes\":" + String((unsigned long)total) +
                  ",\"usedBytes\":" + String((unsigned long)used) +
                  ",\"installed\":[";

    if (m_mounted) {
        File root = LittleFS.open("/effects");
        if (root && root.isDirectory()) {
            File f = root.openNextFile();
            bool first = true;
            while (f) {
                if (!f.isDirectory()) {
                    String fname = String(f.name());
                    if (fname.endsWith(".eff")) {
                        String id = fname;
                        if (id.startsWith("/effects/")) id = id.substring(9);
                        if (id.endsWith(".eff")) id = id.substring(0, id.length() - 4);

                        if (!first) json += ",";
                        first = false;

                        json += "{\"id\":\"" + id + "\",\"size\":" + String((unsigned long)f.size()) + "}";
                    }
                }
                f = root.openNextFile();
            }
        }
    }

    json += "]}";
    return json;
}

size_t StoreManager::getTotalBytes() const {
    return m_mounted ? LittleFS.totalBytes() : 0;
}

size_t StoreManager::getUsedBytes() const {
    return m_mounted ? LittleFS.usedBytes() : 0;
}
