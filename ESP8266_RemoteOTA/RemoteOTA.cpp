#include "RemoteOTA.h"

RemoteOTA::RemoteOTA()
    : _checkInterval(OTA_CHECK_INTERVAL_MS),
      _lastCheckTime(0),
      _fingerprint(nullptr),
      _lastErrorMessage(""),
      _onAvailable(nullptr),
      _onProgress(nullptr),
      _onError(nullptr) {}

void RemoteOTA::begin(const String& manifestUrl, const String& deviceId, const String& deviceSecret) {
    _manifestUrl = manifestUrl;
    _deviceId = deviceId;
    _deviceSecret = deviceSecret;
    // Stagger initial check slightly to allow Wi-Fi to stabilize
    _lastCheckTime = millis() - (_checkInterval - 10000); 
    Serial.printf("[OTA] Initialized. Current Version: %s | Target: %s\n", CURRENT_FIRMWARE_VERSION, FIRMWARE_TARGET_MODEL);
    Serial.printf("[OTA] Manifest Endpoint: %s\n", _manifestUrl.c_str());
}

void RemoteOTA::setCheckInterval(unsigned long intervalMs) {
    _checkInterval = intervalMs;
}

void RemoteOTA::setFingerprint(const char* fingerprint) {
    _fingerprint = fingerprint;
}

void RemoteOTA::handle() {
    if (WiFi.status() != WL_CONNECTED) return;

    if (millis() - _lastCheckTime >= _checkInterval) {
        _lastCheckTime = millis();
        Serial.println("\n[OTA] Scheduled background check triggered...");
        checkForUpdate();
    }
}

bool RemoteOTA::isVersionNewer(const String& newVer, const String& currentVer) {
    int newMajor = 0, newMinor = 0, newPatch = 0;
    int curMajor = 0, curMinor = 0, curPatch = 0;

    sscanf(newVer.c_str(), "%d.%d.%d", &newMajor, &newMinor, &newPatch);
    sscanf(currentVer.c_str(), "%d.%d.%d", &curMajor, &curMinor, &curPatch);

    if (newMajor > curMajor) return true;
    if (newMajor < curMajor) return false;

    if (newMinor > curMinor) return true;
    if (newMinor < curMinor) return false;

    return (newPatch > curPatch);
}

String RemoteOTA::extractJsonString(const String& json, const String& key) {
    String search = "\"" + key + "\"";
    int keyPos = json.indexOf(search);
    if (keyPos == -1) return "";

    int colonPos = json.indexOf(':', keyPos + search.length());
    if (colonPos == -1) return "";

    int startQuote = json.indexOf('"', colonPos);
    if (startQuote == -1) return "";

    int endQuote = json.indexOf('"', startQuote + 1);
    if (endQuote == -1) return "";

    return json.substring(startQuote + 1, endQuote);
}

bool RemoteOTA::extractJsonBool(const String& json, const String& key) {
    String search = "\"" + key + "\"";
    int keyPos = json.indexOf(search);
    if (keyPos == -1) return false;

    int colonPos = json.indexOf(':', keyPos + search.length());
    if (colonPos == -1) return false;

    String sub = json.substring(colonPos + 1);
    sub.trim();
    return sub.startsWith("true");
}

long RemoteOTA::extractJsonLong(const String& json, const String& key) {
    String search = "\"" + key + "\"";
    int keyPos = json.indexOf(search);
    if (keyPos == -1) return 0;

    int colonPos = json.indexOf(':', keyPos + search.length());
    if (colonPos == -1) return 0;

    String sub = json.substring(colonPos + 1);
    sub.trim();
    return sub.toInt();
}

bool RemoteOTA::parseManifestJson(const String& json, OTAManifest& manifest) {
    manifest.version = extractJsonString(json, "version");
    manifest.bin_url = extractJsonString(json, "bin_url");
    manifest.md5 = extractJsonString(json, "md5");
    manifest.target_model = extractJsonString(json, "target_model");
    manifest.notes = extractJsonString(json, "notes");
    manifest.force = extractJsonBool(json, "force");
    manifest.size_bytes = extractJsonLong(json, "size");

    if (manifest.version.length() == 0 || manifest.bin_url.length() == 0) {
        return false;
    }
    return true;
}

bool RemoteOTA::fetchManifest(OTAManifest& manifest) {
    if (WiFi.status() != WL_CONNECTED) {
        _lastErrorMessage = "Wi-Fi not connected";
        return false;
    }

    String requestUrl = _manifestUrl;
    if (!requestUrl.startsWith("http://") && !requestUrl.startsWith("https://")) {
        requestUrl = "http://" + requestUrl;
    }

    Serial.printf("[OTA] Querying manifest from: %s\n", requestUrl.c_str());
    Serial.printf("[OTA] Free Heap: %u bytes\n", ESP.getFreeHeap());

    HTTPClient http;
    http.setTimeout(HTTP_REQUEST_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    std::unique_ptr<BearSSL::WiFiClientSecure> secureClient;
    WiFiClient standardClient;
    bool beginOk = false;

    if (requestUrl.startsWith("https://")) {
        secureClient.reset(new BearSSL::WiFiClientSecure());
        if (!secureClient) {
            _lastErrorMessage = "Failed to allocate TLS client";
            return false;
        }
        secureClient->setBufferSizes(1024, 1024);
        if (_fingerprint != nullptr) {
            secureClient->setFingerprint(_fingerprint);
        } else {
            secureClient->setInsecure();
        }
        beginOk = http.begin(*secureClient, requestUrl);
    } else {
        beginOk = http.begin(standardClient, requestUrl);
    }

    if (!beginOk) {
        _lastErrorMessage = "HTTP client begin failed for: " + requestUrl;
        return false;
    }

    // Send security & device headers
    http.addHeader("User-Agent", "ESP8266-RemoteOTA/" CURRENT_FIRMWARE_VERSION);
    http.addHeader("X-Device-Id", _deviceId);
    http.addHeader("X-Device-Token", _deviceSecret);
    http.addHeader("X-Current-Version", CURRENT_FIRMWARE_VERSION);
    http.addHeader("X-Hardware-Model", FIRMWARE_TARGET_MODEL);

    int httpCode = http.GET();
    Serial.printf("[OTA] Manifest HTTP response code: %d\n", httpCode);

    if (httpCode != HTTP_CODE_OK) {
        _lastErrorMessage = "HTTP check returned status " + String(httpCode);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    Serial.printf("[OTA] Payload received: %s\n", payload.c_str());

    if (!parseManifestJson(payload, manifest)) {
        _lastErrorMessage = "Malformed or missing fields in manifest JSON";
        return false;
    }

    return true;
}

OTAResult RemoteOTA::checkForUpdate() {
    if (WiFi.status() != WL_CONNECTED) {
        _lastErrorMessage = "Wi-Fi is disconnected";
        if (_onError) _onError(OTA_ERR_WIFI_DISCONNECTED, _lastErrorMessage);
        return OTA_ERR_WIFI_DISCONNECTED;
    }

    OTAManifest manifest;
    if (!fetchManifest(manifest)) {
        if (_onError) _onError(OTA_ERR_HTTP_CHECK_FAILED, _lastErrorMessage);
        return OTA_ERR_HTTP_CHECK_FAILED;
    }

    // Hardware Compatibility Check
    if (manifest.target_model.length() > 0 && manifest.target_model != FIRMWARE_TARGET_MODEL) {
        _lastErrorMessage = "Incompatible target model: " + manifest.target_model + " (Device: " FIRMWARE_TARGET_MODEL ")";
        Serial.printf("[OTA] ERROR: %s\n", _lastErrorMessage.c_str());
        if (_onError) _onError(OTA_ERR_INCOMPATIBLE_HARDWARE, _lastErrorMessage);
        return OTA_ERR_INCOMPATIBLE_HARDWARE;
    }

    // Version Comparison
    bool updateNeeded = manifest.force || isVersionNewer(manifest.version, CURRENT_FIRMWARE_VERSION);
    if (!updateNeeded) {
        Serial.printf("[OTA] Firmware is up to date (Current: %s | Server: %s)\n", CURRENT_FIRMWARE_VERSION, manifest.version.c_str());
        return OTA_NO_UPDATE_AVAILABLE;
    }

    Serial.printf("[OTA] New firmware detected! Version: %s (Current: %s)\n", manifest.version.c_str(), CURRENT_FIRMWARE_VERSION);
    Serial.printf("[OTA] Release Notes: %s\n", manifest.notes.c_str());

    if (_onAvailable) {
        _onAvailable(manifest.version, manifest.notes);
    }

    // Check available flash memory space
    uint32_t freeSpace = ESP.getFreeSketchSpace();
    Serial.printf("[OTA] Available Flash OTA Space: %u bytes\n", freeSpace);
    if (manifest.size_bytes > 0 && manifest.size_bytes > freeSpace) {
        _lastErrorMessage = "Insufficient flash space. Required: " + String(manifest.size_bytes) + ", Available: " + String(freeSpace);
        Serial.printf("[OTA] ERROR: %s\n", _lastErrorMessage.c_str());
        if (_onError) _onError(OTA_ERR_NOT_ENOUGH_SPACE, _lastErrorMessage);
        return OTA_ERR_NOT_ENOUGH_SPACE;
    }

    return executeUpdate(manifest);
}

OTAResult RemoteOTA::executeUpdate(const OTAManifest& manifest) {
    Serial.println("\n[OTA] ===============================================");
    Serial.printf("[OTA] Initiating Firmware Download & Flash...\n");
    Serial.printf("[OTA] Source URL: %s\n", manifest.bin_url.c_str());
    if (manifest.md5.length() > 0) {
        Serial.printf("[OTA] Expected MD5: %s\n", manifest.md5.c_str());
    }
    Serial.println("[OTA] ===============================================");

    // Setup ESPhttpUpdate callbacks
    ESPhttpUpdate.rebootOnUpdate(false); // Manual controlled reboot
    ESPhttpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    if (manifest.md5.length() > 0) {
        ESPhttpUpdate.setMD5sum(manifest.md5.c_str());
    }

    // Progress handler
    ESPhttpUpdate.onProgress([this](int current, int total) {
        int percent = (current * 100) / total;
        static int lastReportedPercent = -1;
        if (percent != lastReportedPercent && percent % 10 == 0) {
            lastReportedPercent = percent;
            Serial.printf("[OTA Progress] %d%% (%d / %d bytes)\n", percent, current, total);
            if (_onProgress) _onProgress(percent);
            ESP.wdtFeed();
        }
    });

    ESPhttpUpdate.onStart([]() {
        Serial.println("[OTA] Download started...");
    });

    ESPhttpUpdate.onEnd([]() {
        Serial.println("[OTA] Download completed. Verifying integrity...");
    });

    ESPhttpUpdate.onError([](int err) {
        Serial.printf("[OTA] Flash Error Code: %d\n", err);
    });

    t_httpUpdate_return ret;

    if (manifest.bin_url.startsWith("https://")) {
        std::unique_ptr<BearSSL::WiFiClientSecure> updateClient(new BearSSL::WiFiClientSecure());
        if (!updateClient) {
            _lastErrorMessage = "Failed to allocate TLS client for download";
            if (_onError) _onError(OTA_ERR_DOWNLOAD_FAILED, _lastErrorMessage);
            return OTA_ERR_DOWNLOAD_FAILED;
        }
        updateClient->setBufferSizes(1024, 1024);
        if (_fingerprint != nullptr) {
            updateClient->setFingerprint(_fingerprint);
        } else {
            updateClient->setInsecure();
        }
        ret = ESPhttpUpdate.update(*updateClient, manifest.bin_url);
    } else {
        WiFiClient standardClient;
        ret = ESPhttpUpdate.update(standardClient, manifest.bin_url);
    }

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            _lastErrorMessage = "HTTP_UPDATE_FAILED Error (" + String(ESPhttpUpdate.getLastError()) + "): " + ESPhttpUpdate.getLastErrorString();
            Serial.printf("[OTA] FAILED: %s\n", _lastErrorMessage.c_str());
            if (_onError) _onError(OTA_ERR_DOWNLOAD_FAILED, _lastErrorMessage);
            return OTA_ERR_DOWNLOAD_FAILED;

        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[OTA] HTTP_UPDATE_NO_UPDATES");
            return OTA_NO_UPDATE_AVAILABLE;

        case HTTP_UPDATE_OK:
            Serial.println("\n***************************************************");
            Serial.println(" [OTA SUCCESS] Firmware updated and verified!");
            Serial.println(" Restarting ESP8266 in 2 seconds...");
            Serial.println("***************************************************\n");
            Serial.flush();
            delay(2000);
            ESP.restart();
            return OTA_OK;
    }

    return OTA_OK;
}
