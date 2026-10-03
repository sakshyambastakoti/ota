/*
 * ======================================================================================
 * ESP8266 Remote OTA Firmware System
 * ======================================================================================
 * Production-ready, modular firmware updater over HTTPS.
 * - Works behind NAT, home routers, and mobile hotspots (no port forwarding).
 * - SoftAP Captive Portal Wi-Fi Provisioning with LittleFS storage.
 * - Hardware Flash button (GPIO0) hold for 5s triggers Factory Reset.
 * - Semantic Versioning (X.Y.Z) & MD5 integrity verification.
 * - Optimized BearSSL TLS memory usage (safe for ESP8266 80KB RAM).
 * 
 * Target Board: NodeMCU / Wemos D1 Mini / ESP-12E/F / ESP-01S (4MB or 1MB Flash)
 * ======================================================================================
 */

#include <Arduino.h>
#include "AppConfig.h"
#include "WiFiProvisioner.h"
#include "RemoteOTA.h"

// Core System Instances
WiFiProvisioner provisioner;
RemoteOTA ota;

// Heartbeat & Telemetry timers
unsigned long lastHeartbeat = 0;
unsigned long lastTelemetry = 0;
const unsigned long HEARTBEAT_INTERVAL = 1000;
const unsigned long TELEMETRY_INTERVAL = 30000;

// Application status
int loopCounter = 0;

void printSystemStatus() {
    Serial.println("\n----------------- [DEVICE DIAGNOSTICS] -----------------");
    Serial.printf("Device ID:         %s\n", provisioner.getDeviceId().c_str());
    Serial.printf("MAC Address:       %s\n", provisioner.getMacAddress().c_str());
    Serial.printf("Firmware Version:  %s\n", CURRENT_FIRMWARE_VERSION);
    Serial.printf("Target Hardware:   %s\n", FIRMWARE_TARGET_MODEL);
    Serial.printf("Free RAM Heap:     %u bytes\n", ESP.getFreeHeap());
    Serial.printf("Max Heap Block:    %u bytes\n", ESP.getMaxFreeBlockSize());
    Serial.printf("Heap Fragmentation:%u%%\n", ESP.getHeapFragmentation());
    Serial.printf("Flash Chip Size:   %u bytes\n", ESP.getFlashChipRealSize());
    Serial.printf("Free Sketch Space: %u bytes\n", ESP.getFreeSketchSpace());
    Serial.printf("CPU Frequency:     %u MHz\n", ESP.getCpuFreqMHz());
    if (provisioner.isConnected()) {
        Serial.printf("Wi-Fi SSID:        %s\n", WiFi.SSID().c_str());
        Serial.printf("IP Address:        %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Gateway:           %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("Signal (RSSI):     %d dBm\n", WiFi.RSSI());
    } else if (provisioner.isProvisioningMode()) {
        Serial.println("Wi-Fi Status:      [PROVISIONING CAPTIVE PORTAL ACTIVE]");
    } else {
        Serial.println("Wi-Fi Status:      [CONNECTING...]");
    }
    Serial.println("--------------------------------------------------------\n");
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n\n========================================================");
    Serial.printf("  ESP8266 REMOTE OTA FIRMWARE SYSTEM v%s\n", CURRENT_FIRMWARE_VERSION);
    Serial.println("========================================================");

    // Initialize Wi-Fi Provisioner (loads credentials from LittleFS or opens SoftAP)
    bool connected = provisioner.begin();

    // Register OTA Event Handlers
    ota.onUpdateAvailable([](const String& newVer, const String& notes) {
        Serial.println("\n[APP ALERT] >>> New firmware update found! <<<");
        Serial.printf("Version: %s\nNotes: %s\n", newVer.c_str(), notes.c_str());
    });

    ota.onProgress([](int percent) {
        // Toggle LED rapidly during flashing
        digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
    });

    ota.onError([](OTAResult errCode, const String& errMsg) {
        Serial.printf("[APP ALERT] OTA Error [Code %d]: %s\n", errCode, errMsg.c_str());
    });

    if (connected) {
        // Initialize Remote OTA with manifest URL and device identification
        const DeviceConfig& cfg = provisioner.getConfig();
        ota.begin(cfg.ota_url, provisioner.getDeviceId(), cfg.device_secret);
        
        printSystemStatus();

        // Perform an initial check on boot after Wi-Fi is ready
        Serial.println("[APP] Performing initial boot OTA check in 5 seconds...");
    } else {
        Serial.println("[APP] In Provisioning Mode. Connect to the setup Wi-Fi network.");
    }
}

void loop() {
    // 1. Mandatory: Handle Provisioning, Captive Portal, and Auto-reconnection
    provisioner.handle();

    // 2. If connected to Wi-Fi, run OTA handler & application code
    if (provisioner.isConnected()) {
        // Static flag to initialize OTA on first connection
        static bool otaStarted = false;
        if (!otaStarted) {
            const DeviceConfig& cfg = provisioner.getConfig();
            ota.begin(cfg.ota_url, provisioner.getDeviceId(), cfg.device_secret);
            otaStarted = true;
        }

        // Run non-blocking periodic OTA update checks
        ota.handle();

        // 3. User Application Code - Non-blocking Heartbeat LED
        if (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL) {
            lastHeartbeat = millis();
            // Invert LED for a subtle tick
            digitalWrite(STATUS_LED_PIN, LOW);
            delay(30);
            digitalWrite(STATUS_LED_PIN, HIGH);
        }

        // Periodic Telemetry Diagnostics (every 30s)
        if (millis() - lastTelemetry >= TELEMETRY_INTERVAL) {
            lastTelemetry = millis();
            Serial.printf("[Telemetry] Uptime: %lu s | Free Heap: %u bytes | RSSI: %d dBm | Loop #%d\n",
                          millis() / 1000, ESP.getFreeHeap(), WiFi.RSSI(), ++loopCounter);
        }
    }

    // 4. Interactive Serial Monitor Commands (useful during testing)
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        cmd.toLowerCase();

        if (cmd == "check") {
            Serial.println("\n[CMD] Manual OTA check initiated via Serial...");
            ota.checkForUpdate();
        } else if (cmd == "info") {
            printSystemStatus();
        } else if (cmd == "reset") {
            Serial.println("\n[CMD] Factory reset requested via Serial...");
            provisioner.factoryReset();
        } else if (cmd == "restart") {
            Serial.println("\n[CMD] Restarting ESP8266...");
            ESP.restart();
        } else if (cmd.length() > 0) {
            Serial.printf("[CMD] Unknown command '%s'. Supported: 'check', 'info', 'reset', 'restart'\n", cmd.c_str());
        }
    }

    yield(); // Keep ESP8266 background Wi-Fi tasks happy
}
