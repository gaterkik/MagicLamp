#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include "effects/effect_manager.h"
#include "effects/effect_vm.h"

/**
 * @file store_manager.h
 * @brief Store and LittleFS manager for MagicLamp.
 *
 * Handles:
 * - LittleFS mount and directory maintenance (/effects).
 * - Scanning and dynamic loading of .eff files into EffectManager.
 * - Secure HTTPS downloading of effects from GitHub Raw.
 * - Fetching store manifest/catalog from GitHub Raw.
 * - Installing and deleting custom effects on-the-fly.
 * - LittleFS storage telemetry (used/total bytes).
 */

class StoreManager {
public:
    StoreManager();
    ~StoreManager() = default;

    /**
     * @brief Mount LittleFS, ensure directories exist, and load all installed effects.
     * @param effectMgr Pointer to EffectManager instance.
     * @return true if LittleFS mounted successfully.
     */
    bool begin(EffectManager* effectMgr);

    /**
     * @brief Scan /effects/*.eff in LittleFS and register them in EffectManager.
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
     * @brief Delete an effect from LittleFS and reload EffectManager.
     * @param effectId Unique ID (e.g. "aurora").
     * @param effectMgr Pointer to EffectManager.
     * @return true if deleted.
     */
    bool deleteEffect(const String& effectId, EffectManager* effectMgr);

    /**
     * @brief Check whether an effect is currently installed in LittleFS.
     */
    bool isEffectInstalled(const String& effectId) const;

    /**
     * @brief Fetch remote store/manifest.json from GitHub Raw.
     * @param outJson String buffer to receive the JSON manifest.
     * @return true if successfully fetched.
     */
    bool fetchCatalogJson(String& outJson);

    /**
     * @brief Generate JSON listing all currently installed effects in LittleFS.
     * @return JSON string with installed effects list and storage stats.
     */
    String getInstalledJson() const;

    size_t getTotalBytes() const;
    size_t getUsedBytes() const;
    bool isMounted() const { return m_mounted; }

private:
    bool m_mounted;
    EffectManager* m_effectMgr;

    String getDefaultEffectUrl(const String& effectId) const;
};
