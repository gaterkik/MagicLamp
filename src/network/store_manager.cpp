#include "store_manager.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

StoreManager::StoreManager()
    : m_mounted(false),
      m_nvsFallback(false),
      m_effectMgr(nullptr) {}

bool StoreManager::begin(EffectManager* effectMgr) {
    m_effectMgr = effectMgr;
    m_nvsFallback = false;

    // 1. Try mounting LittleFS with explicit "littlefs" partition label
    m_mounted = LittleFS.begin(true, "/littlefs", 10, "littlefs");
    if (!m_mounted) {
        Serial.println(F("[Store] LittleFS mount with 'littlefs' failed, trying fallback label 'spiffs'..."));
        m_mounted = LittleFS.begin(true, "/littlefs", 10, "spiffs");
    }

    if (m_mounted) {
        Serial.printf("[Store] LittleFS mounted successfully. Total: %u KB, Used: %u KB\n",
                      (unsigned int)(getTotalBytes() / 1024),
                      (unsigned int)(getUsedBytes() / 1024));

        // Ensure /effects directory exists
        if (!LittleFS.exists("/effects")) {
            LittleFS.mkdir("/effects");
            Serial.println(F("[Store] Created /effects directory in LittleFS."));
        }
    } else {
        m_nvsFallback = true;
        Serial.println(F("[Store] LittleFS mount failed on both labels. Activating NVS backup (Preferences 'eff_store') & RAM cache."));
        loadNvsEffects();
    }

    if (m_effectMgr) {
        scanAndLoadEffects(*m_effectMgr);
    }

    return true;
}

void StoreManager::loadNvsEffects() {
    m_nvsCache.clear();
    Preferences pref;
    if (pref.begin("eff_store", true)) {
        String ids = pref.getString("ids", "");
        pref.end();

        if (!ids.isEmpty()) {
            Preferences rPref;
            if (rPref.begin("eff_store", true)) {
                int start = 0;
                while (start < (int)ids.length()) {
                    int comma = ids.indexOf(',', start);
                    String item = (comma >= 0) ? ids.substring(start, comma) : ids.substring(start);
                    item.trim();
                    if (!item.isEmpty()) {
                        String json = rPref.getString(item.c_str(), "");
                        if (!json.isEmpty()) {
                            m_nvsCache.push_back({item, json});
                            Serial.printf("[Store] NVS loaded effect: %s (%u bytes)\n",
                                          item.c_str(), (unsigned int)json.length());
                        }
                    }
                    if (comma < 0) break;
                    start = comma + 1;
                }
                rPref.end();
            }
        }
    }
}

void StoreManager::saveNvsEffect(const String& id, const String& payload) {
    Preferences pref;
    if (pref.begin("eff_store", false)) {
        pref.putString(id.c_str(), payload);

        String ids = pref.getString("ids", "");
        bool found = false;
        int start = 0;
        while (start < (int)ids.length()) {
            int comma = ids.indexOf(',', start);
            String item = (comma >= 0) ? ids.substring(start, comma) : ids.substring(start);
            item.trim();
            if (item.equalsIgnoreCase(id)) {
                found = true;
                break;
            }
            if (comma < 0) break;
            start = comma + 1;
        }

        if (!found) {
            if (!ids.isEmpty()) ids += ",";
            ids += id;
            pref.putString("ids", ids);
        }
        pref.end();
    }

    // Update RAM cache
    bool foundInCache = false;
    for (auto& item : m_nvsCache) {
        if (item.id.equalsIgnoreCase(id)) {
            item.json = payload;
            foundInCache = true;
            break;
        }
    }
    if (!foundInCache) {
        m_nvsCache.push_back({id, payload});
    }
}

bool StoreManager::deleteNvsEffect(const String& id) {
    Preferences pref;
    bool removed = false;
    if (pref.begin("eff_store", false)) {
        if (pref.isKey(id.c_str())) {
            pref.remove(id.c_str());
            removed = true;
        }
        String ids = pref.getString("ids", "");
        String newIds = "";
        int start = 0;
        while (start < (int)ids.length()) {
            int comma = ids.indexOf(',', start);
            String item = (comma >= 0) ? ids.substring(start, comma) : ids.substring(start);
            item.trim();
            if (!item.equalsIgnoreCase(id) && !item.isEmpty()) {
                if (!newIds.isEmpty()) newIds += ",";
                newIds += item;
            }
            if (comma < 0) break;
            start = comma + 1;
        }
        pref.putString("ids", newIds);
        pref.end();
    }

    for (size_t i = 0; i < m_nvsCache.size(); i++) {
        if (m_nvsCache[i].id.equalsIgnoreCase(id)) {
            m_nvsCache.erase(m_nvsCache.begin() + i);
            removed = true;
            break;
        }
    }
    return removed;
}

void StoreManager::scanAndLoadEffects(EffectManager& effectMgr) {
    // Reset custom slots to 10 core effects
    effectMgr.clearCustomEffects();

    uint8_t loadedCount = 0;

    if (m_mounted) {
        File root = LittleFS.open("/effects");
        if (!root || !root.isDirectory()) {
            Serial.println(F("[Store] No /effects directory found during scan."));
            return;
        }

        File file = root.openNextFile();
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
    } else {
        // NVS / RAM cache load
        for (const auto& item : m_nvsCache) {
            EffectVM* vm = new EffectVM();
            if (vm->parseDescriptor(item.json)) {
                if (effectMgr.registerCustomEffect(vm)) {
                    loadedCount++;
                } else {
                    delete vm;
                    Serial.println(F("[Store] Max custom effects capacity reached (24)."));
                    break;
                }
            } else {
                delete vm;
                Serial.printf("[Store] Skipping invalid NVS effect: %s\n", item.id.c_str());
            }
        }
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
        http.setUserAgent("MagicLamp-Store/0.5.2");

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

    if (m_mounted) {
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
    } else {
        saveNvsEffect(effectId, payload);
        Serial.printf("[Store] Successfully saved %d bytes to NVS for '%s'\n", payload.length(), effectId.c_str());
    }

    // Rescan and update EffectManager
    if (effectMgr) {
        scanAndLoadEffects(*effectMgr);
    }

    outMessage = "Effect installed successfully";
    return true;
}

bool StoreManager::deleteEffect(const String& effectId, EffectManager* effectMgr) {
    if (effectId.isEmpty()) return false;

    bool deleted = false;
    if (m_mounted) {
        String filePath = String("/effects/") + effectId + ".eff";
        if (LittleFS.exists(filePath)) {
            deleted = LittleFS.remove(filePath);
            if (deleted) {
                Serial.printf("[Store] Deleted effect file from LittleFS: %s\n", filePath.c_str());
            }
        }
    } else {
        deleted = deleteNvsEffect(effectId);
        if (deleted) {
            Serial.printf("[Store] Deleted effect from NVS: %s\n", effectId.c_str());
        }
    }

    if (deleted && effectMgr) {
        scanAndLoadEffects(*effectMgr);
    }
    return deleted;
}

bool StoreManager::isEffectInstalled(const String& effectId) const {
    if (effectId.isEmpty()) return false;
    if (m_mounted) {
        String filePath = String("/effects/") + effectId + ".eff";
        return LittleFS.exists(filePath);
    } else {
        for (const auto& item : m_nvsCache) {
            if (item.id.equalsIgnoreCase(effectId)) return true;
        }
        return false;
    }
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
                  ",\"storage\":\"" + (m_mounted ? "littlefs" : "nvs") + "\"" +
                  ",\"nvsFallback\":" + (m_mounted ? "false" : "true") +
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
    } else {
        bool first = true;
        for (const auto& item : m_nvsCache) {
            if (!first) json += ",";
            first = false;
            json += "{\"id\":\"" + item.id + "\",\"size\":" + String((unsigned long)item.json.length()) + "}";
        }
    }

    json += "]}";
    return json;
}

size_t StoreManager::getTotalBytes() const {
    if (m_mounted) {
        return LittleFS.totalBytes();
    }
    return 32768; // 32 KB NVS storage allocation
}

size_t StoreManager::getUsedBytes() const {
    if (m_mounted) {
        return LittleFS.usedBytes();
    }
    size_t used = 0;
    for (const auto& item : m_nvsCache) {
        used += item.json.length();
    }
    return used;
}
