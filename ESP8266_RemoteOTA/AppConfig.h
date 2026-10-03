#pragma once
#include <Arduino.h>

// ====================================================================
// ESP8266 REMOTE OTA FIRMWARE SYSTEM - CONFIGURATION
// ====================================================================

// Current Firmware Version (Follows Semantic Versioning: MAJOR.MINOR.PATCH)
#define CURRENT_FIRMWARE_VERSION  "1.0.0"

// Firmware Hardware Target Identifier (prevents flashing incompatible models)
#define FIRMWARE_TARGET_MODEL     "ESP8266-GENERIC"

// Configuration AP Settings (when no Wi-Fi credentials are saved)
#define AP_SSID_PREFIX            "ESP-OTA-SETUP-"
#define AP_PASSWORD               "admin1234"      // Min 8 chars for WPA2, or "" for open AP
#define AP_CAPTIVE_IP             IPAddress(192, 168, 4, 1)

// Hardware Pins
#define FACTORY_RESET_PIN         0                // GPIO0 (FLASH button on NodeMCU/Wemos D1 Mini)
#define FACTORY_RESET_HOLD_MS     5000             // Hold for 5 seconds to wipe credentials
#define STATUS_LED_PIN            2                // Built-in LED on ESP8266 (Active LOW on most boards)

// Default Wi-Fi Credentials (friend's mobile hotspot - connects automatically)
#define DEFAULT_WIFI_SSID         "sakshyam"
#define DEFAULT_WIFI_PASS         "sakshyam"

// Remote OTA Check Settings (Direct ota.log)
#define DEFAULT_OTA_MANIFEST_URL  "http://ota.log"

// Periodic OTA check interval (in milliseconds)
// Default: 60 seconds (60000 ms) for responsive live pushes during testing
#define OTA_CHECK_INTERVAL_MS     60000UL

// Connection & Security
#define DEFAULT_DEVICE_SECRET     "esp8266_secure_token_change_me"
#define HTTP_REQUEST_TIMEOUT_MS   15000

// Wi-Fi Connection Timeouts
#define WIFI_CONNECT_TIMEOUT_SEC  25
#define WIFI_RETRY_INTERVAL_MS    10000UL
