#pragma once
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include "AppConfig.h"

struct DeviceConfig {
    String wifi_ssid;
    String wifi_password;
    String ota_url;
    String device_secret;
};

class WiFiProvisioner {
public:
    WiFiProvisioner();
    
    // Initialize LittleFS, read credentials, attempt connection or launch AP
    bool begin();
    
    // Must be called in loop() to handle DNS, WebServer, and Auto-reconnect
    void handle();
    
    // Check if currently connected to local Wi-Fi station
    bool isConnected() const;
    
    // Check if device is in Access Point / Provisioning Mode
    bool isProvisioningMode() const;
    
    // Wipe credentials and reboot into setup mode
    void factoryReset();
    
    // Getters for configuration
    const DeviceConfig& getConfig() const { return _config; }
    String getDeviceId() const;
    String getMacAddress() const;
    
    // Update OTA Manifest URL in flash storage
    void setOtaUrl(const String& newUrl);

private:
    DeviceConfig _config;
    bool _isProvisioning;
    bool _isConnected;
    unsigned long _lastWifiCheck;
    unsigned long _resetPressStart;
    
    std::unique_ptr<DNSServer> _dnsServer;
    std::unique_ptr<ESP8266WebServer> _webServer;
    
    bool loadConfig();
    bool saveConfig();
    void startProvisioningAP();
    bool connectToWiFi(bool timeoutFast = false);
    void checkResetButton();
    void checkWiFiConnection();
    
    // Web Server Handlers
    void handleRoot();
    void handleScan();
    void handleSave();
    void handleNotFound();
    
    String buildWebPage();
};
