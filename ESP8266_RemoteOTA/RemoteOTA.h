#pragma once
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecureBearSSL.h>
#include "AppConfig.h"

enum OTAResult {
    OTA_OK = 0,
    OTA_NO_UPDATE_AVAILABLE,
    OTA_ERR_WIFI_DISCONNECTED,
    OTA_ERR_HTTP_CHECK_FAILED,
    OTA_ERR_INVALID_MANIFEST,
    OTA_ERR_INCOMPATIBLE_HARDWARE,
    OTA_ERR_NOT_ENOUGH_SPACE,
    OTA_ERR_DOWNLOAD_FAILED,
    OTA_ERR_MD5_MISMATCH
};

struct OTAManifest {
    String version;
    String bin_url;
    String md5;
    String target_model;
    String notes;
    bool force;
    size_t size_bytes;
};

// Callback function prototypes
typedef std::function<void(const String& newVersion, const String& notes)> OTAAvailableCallback;
typedef std::function<void(int percent)> OTAProgressCallback;
typedef std::function<void(OTAResult result, const String& message)> OTAErrorCallback;

class RemoteOTA {
public:
    RemoteOTA();

    // Initialize with current version and device secrets
    void begin(const String& manifestUrl, const String& deviceId, const String& deviceSecret);

    // Call continuously in loop() for non-blocking periodic checks
    void handle();

    // Trigger an immediate check and update if available (synchronous)
    OTAResult checkForUpdate();

    // Manually set check interval
    void setCheckInterval(unsigned long intervalMs);

    // Set optional custom SHA1 fingerprint for certificate pinning
    void setFingerprint(const char* fingerprint);

    // Event Callbacks
    void onUpdateAvailable(OTAAvailableCallback cb) { _onAvailable = cb; }
    void onProgress(OTAProgressCallback cb)         { _onProgress = cb; }
    void onError(OTAErrorCallback cb)               { _onError = cb; }

    // Helpers
    static bool isVersionNewer(const String& newVer, const String& currentVer);
    String getLastError() const { return _lastErrorMessage; }

private:
    String _manifestUrl;
    String _deviceId;
    String _deviceSecret;
    unsigned long _checkInterval;
    unsigned long _lastCheckTime;
    const char* _fingerprint;
    String _lastErrorMessage;

    OTAAvailableCallback _onAvailable;
    OTAProgressCallback _onProgress;
    OTAErrorCallback _onError;

    // Internal execution
    bool fetchManifest(OTAManifest& manifest);
    OTAResult executeUpdate(const OTAManifest& manifest);
    bool parseManifestJson(const String& json, OTAManifest& manifest);
    String extractJsonString(const String& json, const String& key);
    bool extractJsonBool(const String& json, const String& key);
    long extractJsonLong(const String& json, const String& key);
};
