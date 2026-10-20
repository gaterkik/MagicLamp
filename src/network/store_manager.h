#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <vector>
#include <Preferences.h>
#include "effects/effect_manager.h"
#include "effects/effect_vm.h"

/**
 * @file store_manager.h
 * @brief Store and LittleFS manager with NVS-backup for MagicLamp.
 *
 * Handles:
 * - LittleFS mount (with partition label "littlefs" and fallback "spiffs").
 * - Automatic NVS fallback (Preferences "eff_store") and RAM cache when LittleFS is unmounted.
 * - Scanning and dynamic loading of .eff files into EffectManager.
 * - Secure HTTPS downloading of effects from GitHub Raw.
 * - Fetching store manifest/catalog from GitHub Raw.
 * - Installing and deleting custom effects on-the-fly.
 * - Storage telemetry (used/total bytes).
 */

class StoreManager {
public:
    StoreManager();
    ~StoreManager() = default;

    /**
     * @brief Mount LittleFS, ensure directories exist, and load all installed effects.
     * If LittleFS is unavailable, activates NVS-backup ("eff_store") with RAM cache.
     * @param effectMgr Pointer to EffectManager instance.
     * @return true if store is ready.
     */
    bool begin(EffectManager* effectMgr);

    /**
     * @brief Scan effects (from LittleFS or NVS) and register them in EffectManager.
     */
    void scanAndLoadEffects(EffectManager& effectMgr);

    /**
     * @brief Download and install an effect by ID from GitHub Raw or custom URL.
     * @param effectId Unique ID (e.g. "aurora").
     * @param url Optional explicit URL. If empty, default GitHub Raw URL is used.
     * @param effectMgr Pointer to EffectManager to reload and activate.
     * @param outMessage Buffer or string returning result message.
     * @return true if downloaded, validated, and registered.
     */
    bool installEffect(const String& effectId, const String& url, EffectManager* effectMgr, String& outMessage);

    /**
     * @brief Delete an effect from LittleFS or NVS and reload EffectManager.
     * @param effectId Unique ID (e.g. "aurora").
     * @param effectMgr Pointer to EffectManager.
     * @return true if deleted.
     */
    bool deleteEffect(const String& effectId, EffectManager* effectMgr);

    /**
     * @brief Check whether an effect is currently installed.
     */
    bool isEffectInstalled(const String& effectId) const;

    /**
     * @brief Robust HTTPS file downloader with 15s timeout and up to 3 retries on connection error (<0).
     * @param url Remote HTTPS URL.
     * @param outContent Output buffer for downloaded content.
     * @return true if successfully downloaded.
     */
    bool downloadFile(const String& url, String& outContent);

    /**
     * @brief Fetch remote store/manifest.json from GitHub Raw.
     * @param outJson String buffer to receive the JSON manifest.
     * @return true if successfully fetched.
     */
    bool fetchCatalogJson(String& outJson);

    /**
     * @brief Generate JSON listing all currently installed effects.
     * @return JSON string with installed effects list and storage stats.
     */
    String getInstalledJson() const;

    size_t getTotalBytes() const;
    size_t getUsedBytes() const;
    bool isMounted() const { return m_mounted; }
    bool isNvsFallback() const { return m_nvsFallback; }

private:
    bool m_mounted;
    bool m_nvsFallback;
    EffectManager* m_effectMgr;

    struct NvsEffectEntry {
        String id;
        String json;
    };
    std::vector<NvsEffectEntry> m_nvsCache;

    void loadNvsEffects();
    void saveNvsEffect(const String& id, const String& payload);
    bool deleteNvsEffect(const String& id);

    String getDefaultEffectUrl(const String& effectId) const;
};
