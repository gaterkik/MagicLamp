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

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(12000);
    http.setUserAgent("MagicLamp-Store/0.5.0");

    if (!http.begin(client, downloadUrl)) {
        outMessage = "Failed to initiate HTTPS connection";
        Serial.println(F("[Store] ERROR: http.begin failed"));
        return false;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        outMessage = String("HTTP error: ") + String(httpCode);
        Serial.printf("[Store] Download failed with HTTP code: %d\n", httpCode);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

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

        // Find the newly installed effect and immediately activate it
        for (uint8_t i = 10; i < effectMgr->getTotalEffects(); i++) {
            Effect* eff = effectMgr->getEffect(i);
            if (eff && strcmp(eff->getName(), tempVM.getName()) == 0) {
                extern LedMatrix* g_activeMatrixForStore; // fallback or pass matrix
                // Will be activated in web handler with matrix reference
                break;
            }
        }
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

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(10000);
    http.setUserAgent("MagicLamp-Store/0.5.0");

    if (!http.begin(client, catalogUrl)) {
        return false;
    }

    int code = http.GET();
    if (code == HTTP_CODE_OK) {
        outJson = http.getString();
        http.end();
        return true;
    }

    http.end();
    return false;
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
