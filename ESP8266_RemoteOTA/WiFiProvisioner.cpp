#include "WiFiProvisioner.h"

static const char* CONFIG_FILE = "/sys_config.dat";
static const byte DNS_PORT = 53;

WiFiProvisioner::WiFiProvisioner() 
    : _isProvisioning(false), _isConnected(false), _lastWifiCheck(0), _resetPressStart(0) {
    _config.wifi_ssid = DEFAULT_WIFI_SSID;
    _config.wifi_password = DEFAULT_WIFI_PASS;
    _config.ota_url = DEFAULT_OTA_MANIFEST_URL;
    _config.device_secret = DEFAULT_DEVICE_SECRET;
}

String WiFiProvisioner::getMacAddress() const {
    return WiFi.macAddress();
}

String WiFiProvisioner::getDeviceId() const {
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    return "ESP8266-" + mac;
}

bool WiFiProvisioner::begin() {
    pinMode(FACTORY_RESET_PIN, INPUT_PULLUP);
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, HIGH); // Off initially (Active LOW)

    Serial.println("\n[Provisioner] Initializing LittleFS...");
    if (!LittleFS.begin()) {
        Serial.println("[Provisioner] LittleFS mount failed, formatting...");
        LittleFS.format();
        if (!LittleFS.begin()) {
            Serial.println("[Provisioner] FATAL: Failed to initialize LittleFS!");
        }
    }

    // Try loading saved Wi-Fi credentials
    bool configLoaded = loadConfig();

    // Fall back to default friend hotspot credentials if unconfigured
    if (!configLoaded || _config.wifi_ssid.length() == 0) {
        Serial.printf("[Provisioner] Using default hotspot SSID: '%s'\n", DEFAULT_WIFI_SSID);
        _config.wifi_ssid = DEFAULT_WIFI_SSID;
        _config.wifi_password = DEFAULT_WIFI_PASS;
        _config.ota_url = DEFAULT_OTA_MANIFEST_URL;
    }

    if (_config.wifi_ssid.length() > 0) {
        Serial.printf("[Provisioner] Connecting to SSID: '%s'\n", _config.wifi_ssid.c_str());
        if (connectToWiFi(false)) {
            Serial.println("[Provisioner] Successfully connected to Wi-Fi station!");
            _isConnected = true;
            _isProvisioning = false;
            return true;
        }
    }

    // If no config or connection failed, switch to Provisioning AP
    Serial.println("[Provisioner] Launching Captive Portal Provisioning Mode...");
    startProvisioningAP();
    return false;
}

bool WiFiProvisioner::loadConfig() {
    if (!LittleFS.exists(CONFIG_FILE)) {
        Serial.println("[Provisioner] No saved configuration file found.");
        return false;
    }

    File f = LittleFS.open(CONFIG_FILE, "r");
    if (!f) return false;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        int eqIndex = line.indexOf('=');
        if (eqIndex > 0) {
            String key = line.substring(0, eqIndex);
            String val = line.substring(eqIndex + 1);
            if (key == "ssid") _config.wifi_ssid = val;
            else if (key == "pass") _config.wifi_password = val;
            else if (key == "ota_url") _config.ota_url = val;
            else if (key == "secret") _config.device_secret = val;
        }
    }
    f.close();
    return true;
}

bool WiFiProvisioner::saveConfig() {
    File f = LittleFS.open(CONFIG_FILE, "w");
    if (!f) {
        Serial.println("[Provisioner] Failed to open config file for writing!");
        return false;
    }

    f.printf("ssid=%s\n", _config.wifi_ssid.c_str());
    f.printf("pass=%s\n", _config.wifi_password.c_str());
    f.printf("ota_url=%s\n", _config.ota_url.c_str());
    f.printf("secret=%s\n", _config.device_secret.c_str());
    f.close();
    Serial.println("[Provisioner] Configuration saved successfully to LittleFS.");
    return true;
}

void WiFiProvisioner::setOtaUrl(const String& newUrl) {
    if (newUrl.length() > 0 && newUrl != _config.ota_url) {
        _config.ota_url = newUrl;
        saveConfig();
    }
}

bool WiFiProvisioner::connectToWiFi(bool timeoutFast) {
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(_config.wifi_ssid.c_str(), _config.wifi_password.c_str());

    Serial.printf("[Wi-Fi] Connecting to '%s'", _config.wifi_ssid.c_str());
    int maxWaitSec = timeoutFast ? 10 : WIFI_CONNECT_TIMEOUT_SEC;
    unsigned long start = millis();

    while (WiFi.status() != WL_CONNECTED && (millis() - start) < (unsigned long)(maxWaitSec * 1000)) {
        delay(500);
        Serial.print(".");
        // Blink LED while connecting
        digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
    }

    if (WiFi.status() == WL_CONNECTED) {
        digitalWrite(STATUS_LED_PIN, LOW); // Solid ON when connected (Active LOW)
        Serial.printf("\n[Wi-Fi] Connected! IP: %s | Gateway: %s | RSSI: %d dBm\n", 
            WiFi.localIP().toString().c_str(), 
            WiFi.gatewayIP().toString().c_str(), 
            WiFi.RSSI());
        _isConnected = true;
        return true;
    } else {
        digitalWrite(STATUS_LED_PIN, HIGH); // OFF
        Serial.printf("\n[Wi-Fi] Connection failed! Status code: %d\n", WiFi.status());
        _isConnected = false;
        return false;
    }
}

void WiFiProvisioner::startProvisioningAP() {
    _isProvisioning = true;
    _isConnected = false;
    WiFi.disconnect(true);
    delay(100);

    // Compute unique AP SSID using last 4 characters of MAC
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    String suffix = mac.substring(mac.length() >= 4 ? mac.length() - 4 : 0);
    String apSsid = String(AP_SSID_PREFIX) + suffix;

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_CAPTIVE_IP, AP_CAPTIVE_IP, IPAddress(255, 255, 255, 0));
    
    if (strlen(AP_PASSWORD) >= 8) {
        WiFi.softAP(apSsid.c_str(), AP_PASSWORD);
    } else {
        WiFi.softAP(apSsid.c_str());
    }

    Serial.printf("\n======================================================\n");
    Serial.printf(" [PROVISIONING AP ACTIVE]\n");
    Serial.printf(" SSID: %s\n", apSsid.c_str());
    if (strlen(AP_PASSWORD) >= 8) {
        Serial.printf(" Password: %s\n", AP_PASSWORD);
    } else {
        Serial.printf(" Password: [None - Open AP]\n");
    }
    Serial.printf(" Captive Portal IP: %s\n", AP_CAPTIVE_IP.toString().c_str());
    Serial.printf("======================================================\n\n");

    // Initialize Captive DNS server redirecting everything to 192.168.4.1
    _dnsServer.reset(new DNSServer());
    _dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    _dnsServer->start(DNS_PORT, "*", AP_CAPTIVE_IP);

    // Setup Web Server
    _webServer.reset(new ESP8266WebServer(80));
    _webServer->on("/", HTTP_GET, std::bind(&WiFiProvisioner::handleRoot, this));
    _webServer->on("/scan", HTTP_GET, std::bind(&WiFiProvisioner::handleScan, this));
    _webServer->on("/save", HTTP_POST, std::bind(&WiFiProvisioner::handleSave, this));

    // Common Captive Portal detection probes
    _webServer->on("/generate_204", HTTP_GET, std::bind(&WiFiProvisioner::handleRoot, this));
    _webServer->on("/hotspot-detect.html", HTTP_GET, std::bind(&WiFiProvisioner::handleRoot, this));
    _webServer->on("/canonical.html", HTTP_GET, std::bind(&WiFiProvisioner::handleRoot, this));
    _webServer->on("/ncsi.txt", HTTP_GET, std::bind(&WiFiProvisioner::handleRoot, this));
    _webServer->on("/connecttest.txt", HTTP_GET, std::bind(&WiFiProvisioner::handleRoot, this));

    _webServer->onNotFound(std::bind(&WiFiProvisioner::handleNotFound, this));
    _webServer->begin();
}

void WiFiProvisioner::handleRoot() {
    _webServer->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    _webServer->send(200, "text/html", buildWebPage());
}

void WiFiProvisioner::handleScan() {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; ++i) {
        if (i > 0) json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + ",\"enc\":" + String(WiFi.encryptionType(i) != ENC_TYPE_NONE ? "true" : "false") + "}";
    }
    json += "]";
    _webServer->sendHeader("Content-Type", "application/json");
    _webServer->send(200, "application/json", json);
}

void WiFiProvisioner::handleSave() {
    if (!_webServer->hasArg("ssid")) {
        _webServer->send(400, "text/plain", "Missing SSID parameter");
        return;
    }

    _config.wifi_ssid = _webServer->arg("ssid");
    _config.wifi_password = _webServer->arg("password");
    
    if (_webServer->hasArg("ota_url") && _webServer->arg("ota_url").length() > 0) {
        _config.ota_url = _webServer->arg("ota_url");
    }
    if (_webServer->hasArg("secret") && _webServer->arg("secret").length() > 0) {
        _config.device_secret = _webServer->arg("secret");
    }

    saveConfig();

    String responseHtml = 
        "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<style>body{background:#0a0f1d;color:#e2e8f0;font-family:-apple-system,BlinkMacSystemFont,sans-serif;padding:30px;text-align:center;}"
        ".card{background:#1e293b;padding:30px;border-radius:16px;box-shadow:0 10px 25px rgba(0,0,0,0.5);max-width:400px;margin:auto;border:1px solid #334155;}"
        "h2{color:#38bdf8;margin-bottom:10px;}p{color:#94a3b8;font-size:15px;line-height:1.5;}"
        ".spinner{width:40px;height:40px;border:4px solid #334155;border-top-color:#38bdf8;border-radius:50%;margin:20px auto;animation:spin 1s linear infinite;}"
        "@keyframes spin{to{transform:rotate(360deg);}}</style></head>"
        "<body><div class='card'><h2>Credentials Saved!</h2>"
        "<div class='spinner'></div>"
        "<p>Connecting to <b>" + _config.wifi_ssid + "</b>.<br>Device is rebooting into Station Mode...</p>"
        "</div></body></html>";

    _webServer->send(200, "text/html", responseHtml);
    delay(1000);
    ESP.restart();
}

void WiFiProvisioner::handleNotFound() {
    // Captive portal fallback: redirect any unknown request to /
    _webServer->sendHeader("Location", String("http://") + AP_CAPTIVE_IP.toString() + "/", true);
    _webServer->send(302, "text/plain", "");
}

String WiFiProvisioner::buildWebPage() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP8266 Setup Portal</title>
<style>
  :root {
    --bg: #0a0f1d;
    --card: #141c2f;
    --border: #23314f;
    --primary: #0284c7;
    --primary-hover: #0369a1;
    --accent: #38bdf8;
    --text: #f1f5f9;
    --muted: #94a3b8;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: var(--bg);
    color: var(--text);
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    display: flex;
    justify-content: center;
    align-items: center;
    min-height: 100vh;
    padding: 20px;
  }
  .container {
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 20px;
    width: 100%;
    max-width: 440px;
    padding: 28px;
    box-shadow: 0 20px 40px rgba(0,0,0,0.6);
  }
  .header { text-align: center; margin-bottom: 24px; }
  .badge {
    display: inline-block;
    background: rgba(56, 189, 248, 0.12);
    color: var(--accent);
    padding: 4px 12px;
    border-radius: 100px;
    font-size: 12px;
    font-weight: 600;
    margin-bottom: 8px;
    border: 1px solid rgba(56, 189, 248, 0.25);
  }
  h1 { font-size: 22px; font-weight: 700; color: var(--text); }
  p.sub { font-size: 13px; color: var(--muted); margin-top: 4px; }
  .info-bar {
    background: #0d1527;
    border: 1px solid #1c2742;
    border-radius: 10px;
    padding: 10px 14px;
    font-size: 12px;
    color: var(--muted);
    margin-bottom: 20px;
    display: flex;
    justify-content: space-between;
  }
  .info-bar span { color: var(--accent); font-weight: 600; }
  .form-group { margin-bottom: 18px; }
  label { display: block; font-size: 13px; font-weight: 500; color: #cbd5e1; margin-bottom: 6px; }
  input, select {
    width: 100%;
    padding: 12px 14px;
    background: #090e1a;
    border: 1px solid var(--border);
    border-radius: 10px;
    color: var(--text);
    font-size: 14px;
    outline: none;
    transition: border-color 0.2s;
  }
  input:focus, select:focus { border-color: var(--accent); }
  .network-select-wrap { display: flex; gap: 8px; }
  .btn-scan {
    padding: 0 14px;
    background: #1e293b;
    border: 1px solid var(--border);
    border-radius: 10px;
    color: var(--accent);
    cursor: pointer;
    font-size: 13px;
    font-weight: 600;
    transition: background 0.2s;
  }
  .btn-scan:hover { background: #334155; }
  .btn-primary {
    width: 100%;
    background: linear-gradient(135deg, #0284c7, #2563eb);
    color: #fff;
    border: none;
    padding: 14px;
    font-size: 15px;
    font-weight: 600;
    border-radius: 12px;
    cursor: pointer;
    box-shadow: 0 6px 20px rgba(2, 132, 199, 0.4);
    transition: opacity 0.2s, transform 0.1s;
  }
  .btn-primary:active { transform: scale(0.98); }
  .details-box {
    margin-top: 14px;
    border-top: 1px dashed var(--border);
    padding-top: 14px;
  }
  details summary { font-size: 12px; color: var(--muted); cursor: pointer; user-select: none; }
  details[open] summary { margin-bottom: 12px; color: var(--accent); }
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <div class="badge">ESP8266 PROVISIONING</div>
    <h1>Connect Device</h1>
    <p class="sub">Configure your local Wi-Fi and Remote OTA</p>
  </div>

  <div class="info-bar">
    <div>ID: <span id="chipId">)rawliteral" + getDeviceId() + R"rawliteral(</span></div>
    <div>Ver: <span>)rawliteral" + String(CURRENT_FIRMWARE_VERSION) + R"rawliteral(</span></div>
  </div>

  <form method="POST" action="/save">
    <div class="form-group">
      <label for="ssid">Wi-Fi Network (SSID)</label>
      <div class="network-select-wrap">
        <select id="networkSelect" onchange="onSelectSSID(this.value)">
          <option value="">-- Scanning nearby Wi-Fi... --</option>
        </select>
        <button type="button" class="btn-scan" onclick="scanNetworks()">Scan</button>
      </div>
      <input type="text" id="ssid" name="ssid" placeholder="Or enter SSID manually" style="margin-top:8px;" required value=")rawliteral" + _config.wifi_ssid + R"rawliteral(">
    </div>

    <div class="form-group">
      <label for="password">Wi-Fi Password</label>
      <input type="password" id="password" name="password" placeholder="Enter network password" value=")rawliteral" + _config.wifi_password + R"rawliteral(">
    </div>

    <details class="details-box">
      <summary>Advanced Remote OTA Settings</summary>
      <div class="form-group" style="margin-top: 12px;">
        <label for="ota_url">Remote OTA Manifest URL</label>
        <input type="text" id="ota_url" name="ota_url" value=")rawliteral" + _config.ota_url + R"rawliteral(" placeholder="https://...">
      </div>
      <div class="form-group">
        <label for="secret">Device Auth Secret Token</label>
        <input type="text" id="secret" name="secret" value=")rawliteral" + _config.device_secret + R"rawliteral(" placeholder="Optional security key">
      </div>
    </details>

    <div style="margin-top: 22px;">
      <button type="submit" class="btn-primary">Save & Connect Device</button>
    </div>
  </form>
</div>

<script>
function scanNetworks() {
  const sel = document.getElementById('networkSelect');
  sel.innerHTML = '<option>Scanning...</option>';
  fetch('/scan')
    .then(r => r.json())
    .then(data => {
      sel.innerHTML = '<option value="">-- Select detected network --</option>';
      data.sort((a,b) => b.rssi - a.rssi);
      data.forEach(net => {
        if (!net.ssid) return;
        const opt = document.createElement('option');
        opt.value = net.ssid;
        opt.textContent = `${net.ssid} (${net.rssi} dBm)${net.enc ? ' 🔒' : ''}`;
        sel.appendChild(opt);
      });
    })
    .catch(() => {
      sel.innerHTML = '<option value="">Scan failed, type SSID below</option>';
    });
}
function onSelectSSID(val) {
  if (val) document.getElementById('ssid').value = val;
}
window.onload = scanNetworks;
</script>
</body>
</html>
)rawliteral";

    return html;
}

void WiFiProvisioner::checkResetButton() {
    // Check if FACTORY_RESET_PIN (GPIO0) is held low for FACTORY_RESET_HOLD_MS
    if (digitalRead(FACTORY_RESET_PIN) == LOW) {
        if (_resetPressStart == 0) {
            _resetPressStart = millis();
        } else if (millis() - _resetPressStart > FACTORY_RESET_HOLD_MS) {
            Serial.println("\n[Provisioner] RESET BUTTON HELD! Triggering Factory Reset...");
            factoryReset();
        }
    } else {
        _resetPressStart = 0;
    }
}

void WiFiProvisioner::factoryReset() {
    Serial.println("[Provisioner] Formatting LittleFS and wiping credentials...");
    LittleFS.remove(CONFIG_FILE);
    for (int i = 0; i < 10; i++) {
        digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
        delay(100);
    }
    Serial.println("[Provisioner] System rebooting...");
    ESP.restart();
}

void WiFiProvisioner::checkWiFiConnection() {
    if (_isProvisioning) return;

    if (millis() - _lastWifiCheck > WIFI_RETRY_INTERVAL_MS) {
        _lastWifiCheck = millis();

        if (WiFi.status() != WL_CONNECTED) {
            _isConnected = false;
            digitalWrite(STATUS_LED_PIN, HIGH); // LED OFF
            Serial.println("[Wi-Fi] Disconnected! Attempting auto-reconnect...");
            WiFi.reconnect();
        } else {
            _isConnected = true;
            digitalWrite(STATUS_LED_PIN, LOW); // LED Solid ON
        }
    }
}

void WiFiProvisioner::handle() {
    checkResetButton();

    if (_isProvisioning) {
        if (_dnsServer) _dnsServer->processNextRequest();
        if (_webServer) _webServer->handleClient();
        
        // Fast blink LED to indicate Setup Mode
        static unsigned long lastBlink = 0;
        if (millis() - lastBlink > 200) {
            lastBlink = millis();
            digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
        }
    } else {
        checkWiFiConnection();
    }
}

bool WiFiProvisioner::isConnected() const {
    return _isConnected && (WiFi.status() == WL_CONNECTED);
}

bool WiFiProvisioner::isProvisioningMode() const {
    return _isProvisioning;
}
