# ESP8266 Remote OTA Firmware System

A production-ready, modular, and secure Remote Over-The-Air (OTA) firmware update system for ESP8266 microcontrollers. 

Designed for real-world deployments where you flash a device once via USB, hand it to a friend or client, configure their local Wi-Fi via a captive portal, and seamlessly push remote firmware updates from anywhere across the globe without port forwarding, static IPs, or physical access.

---

## 🌟 Key Features

- **Global NAT & Firewall Traversal**: Uses outbound HTTPS polling/pull. Operates behind home routers, office firewalls, CGNAT, and mobile hotspots with **zero port forwarding**.
- **SoftAP Captive Portal Provisioning**: If unconfigured or Wi-Fi is lost, broadcasts a setup Wi-Fi network (`ESP-OTA-SETUP-XXXX`) and automatically opens a sleek dark-mode configuration portal at `192.168.4.1`.
- **LittleFS Storage**: Wi-Fi credentials, manifest URLs, and auth secrets are stored reliably in LittleFS flash memory.
- **Physical Factory Reset**: Holding the on-board **FLASH button (GPIO0)** for 5 seconds wipes credentials and restarts provisioning mode.
- **Automatic Reconnection**: Non-blocking Wi-Fi state machine with background auto-reconnect.
- **Semantic Version Management**: Automatically parses `MAJOR.MINOR.PATCH` and only downloads when a newer version (or `force: true`) is available.
- **MD5 Cryptographic Integrity Verification**: Validates the binary's MD5 checksum stream during download. Corrupted or incomplete downloads are discarded before the boot flag is changed.
- **Hardware Model Guard**: Protects against accidental cross-flashing across different hardware revisions.
- **BearSSL Memory Optimization**: Restricts TLS buffer sizes (`client->setBufferSizes(1024, 1024)`) to avoid exhausting the ESP8266's limited ~80KB RAM.
- **100% Free Cloud Hosting Options**: Works out of the box with Cloudflare Workers (100k req/day free) or GitHub Releases + GitHub Raw.
- **Dual IDE Support**: 100% compatible with both **Arduino IDE** and **PlatformIO**.

---

## 📐 Architecture Overview

```
                                      OUTBOUND HTTPS ONLY (No Port Forwarding Required)
  +-----------------------+                                            +---------------------------+
  |                       |  1. Periodic Check (GET /api/ota/check)    |   Cloud Host / CDN        |
  |   ESP8266 Board       | -----------------------------------------> |  - Cloudflare Worker      |
  |  (NodeMCU / D1 Mini)  |      X-Device-Id, X-Device-Token           |  - GitHub Releases / Raw  |
  |                       |                                            |  - Self-Hosted Server     |
  |  - Free Heap: ~35KB   |  2. Returns Manifest JSON                  |                           |
  |  - Active OTA Slot A  | <----------------------------------------- |  {                        |
  |  - Inactive Slot B    |      { "version": "1.0.1", "md5": "...",   |    "version": "1.0.1",    |
  |                       |        "bin_url": "https://..." }          |    "bin_url": "...",      |
  |                       |                                            |    "md5": "..."           |
  |                       |  3. Stream Download & Flash to Slot B      |  }                        |
  |                       | -----------------------------------------> |                           |
  |                       |                                            +---------------------------+
  |  4. Verify MD5 Hash   |
  |  5. Swap Boot Slot    |
  |  6. Reboot into v1.0.1|
  +-----------------------+
```

### Why Outbound HTTPS?
Traditional `ArduinoOTA` uses mDNS and an inbound TCP port. This only works on the same local LAN and fails behind NAT or different subnets. 

This Remote OTA system initiates **outbound HTTPS requests** from the ESP8266 to the cloud server, meaning any router or mobile hotspot allows the traffic through automatically.

---

## 📁 Repository Structure

```
ota/
├── ESP8266_RemoteOTA/          # Arduino IDE Sketch Directory
│   ├── ESP8266_RemoteOTA.ino   # Main application sketch
│   ├── AppConfig.h             # Configuration, pins, versioning
│   ├── RemoteOTA.h             # Modular OTA Client class
│   ├── RemoteOTA.cpp           # BearSSL TLS streaming & MD5 verification
│   ├── WiFiProvisioner.h      # Captive Portal & LittleFS Storage class
│   └── WiFiProvisioner.cpp    # Web UI & DNS captive portal
├── server/                     # Server-side components
│   ├── cloudflare-worker.js    # Free serverless OTA backend (Cloudflare)
│   ├── node-server.js          # Self-hosted Node.js / Express server with UI
│   ├── manifest.json           # Template OTA manifest
│   ├── publish_firmware.py     # CLI helper to generate MD5 & manifest
│   └── package.json            # Node.js dependencies
├── platformio.ini              # Ready-to-build PlatformIO configuration
└── README.md                   # Full documentation
```

---

## 🚀 Quick Start Guide

### Step 1: Initial Flash via USB (Arduino IDE)

1. Open **Arduino IDE**.
2. Install the ESP8266 board core if you haven't already:
   - Go to **File** -> **Preferences** -> **Additional Boards Manager URLs**:
     `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
   - Open **Tools** -> **Board** -> **Boards Manager**, search for `esp8266` and install.
3. Open `ESP8266_RemoteOTA/ESP8266_RemoteOTA.ino`.
4. Configure your board settings under **Tools**:
   - **Board**: `NodeMCU 1.0 (ESP-12E Module)` or `LOLIN(WEMOS) D1 R2 & mini`
   - **CPU Frequency**: `160 MHz` (Recommended: speeds up TLS crypto significantly)
   - **Flash Size**: `4MB (FS:1MB OTA:~1019KB)` or `4MB (FS:2MB OTA:~1019KB)`
   - **Upload Speed**: `921600` (or `115200`)
5. Connect your board via USB and click **Upload**.

### Step 1 (Alternative): Initial Flash via USB (PlatformIO)

If you use VS Code with PlatformIO:
```bash
# Build and flash NodeMCU v2
pio run -e nodemcuv2 -t upload

# Open Serial Monitor
pio device monitor -b 115200
```

---

## 📶 Step 2: Wi-Fi Provisioning (Captive Portal)

When your friend powers on the ESP8266 at their home:
1. Because no Wi-Fi credentials are saved yet, the board starts in **Provisioning Mode**.
2. On a phone or laptop, open Wi-Fi settings and look for the network:
   `ESP-OTA-SETUP-XXXX` (where `XXXX` is unique to that board).
   - Default Password: `admin1234` (configurable in `AppConfig.h`).
3. Connect to the Wi-Fi. The phone will display a **"Sign in to network"** notification.
4. If it doesn't open automatically, open your browser and navigate to `http://192.168.4.1`.
5. In the captive portal UI:
   - Click **Scan** and select the home Wi-Fi SSID.
   - Enter the Wi-Fi password.
   - (Optional) Customize the OTA Manifest URL or Device Secret.
   - Click **Save & Connect Device**.
6. The ESP8266 saves credentials to LittleFS and reboots into normal Station Mode.

> **Factory Reset**: If they ever change routers or move, they simply hold down the **FLASH button (GPIO0)** for 5 seconds. The LED will blink rapidly, erase saved credentials, and reopen the setup network.

---

## 🌐 Step 3: Deploying Remote Updates Anywhere

### Option A: Using Free Cloudflare Workers (Recommended)

Cloudflare Workers provides 100,000 free HTTPS requests every day, requires no server maintenance, and provides worldwide low-latency responses.

1. Sign up for a free account at [cloudflare.com](https://dash.cloudflare.com).
2. Go to **Workers & Pages** -> **Create Application** -> **Create Worker**.
3. Name it `esp-ota-worker` and click **Deploy**.
4. Click **Edit code** and paste the contents of `server/cloudflare-worker.js`.
5. Your worker is now live at:
   `https://esp-ota-worker.<your-subdomain>.workers.dev/api/ota/check`
6. Put this URL in `AppConfig.h` or enter it via the Web Provisioner UI.

---

### Option B: Using GitHub Releases (100% Serverless & Free)

You can host both your manifest and `.bin` binaries directly in a GitHub repository:

1. Create a public GitHub repository (e.g. `esp8266-firmware`).
2. Export your compiled binary (see compilation workflow below).
3. Create a GitHub Release (e.g. tag `v1.0.1`) and upload `firmware.bin`.
4. Copy the direct download link:
   `https://github.com/<user>/<repo>/releases/download/v1.0.1/firmware.bin`
5. Generate the manifest using our CLI helper:
   ```bash
   python server/publish_firmware.py firmware.bin 1.0.1 "https://github.com/<user>/<repo>/releases/download/v1.0.1/firmware.bin" "Patch notes here"
   ```
6. Commit `manifest.json` to the `main` branch of your repository.
7. Use the raw URL in your ESP8266:
   `https://raw.githubusercontent.com/<user>/<repo>/main/server/manifest.json`

---

### Option C: Self-Hosted Node.js Server with Web Dashboard

If you want an on-premise or VPS server with a browser dashboard:
```bash
cd server
npm install
npm start
```
- Open `http://localhost:3000/` in your browser.
- Fill in the new version number, select the compiled `.bin` file, and click **Upload & Publish**.
- The server computes the MD5 hash, copies the binary, and updates the manifest automatically!

---

## 🛠️ Compilation & Export Workflow

### In Arduino IDE:
1. Make your code changes in `ESP8266_RemoteOTA.ino` or your custom project files.
2. Increment `CURRENT_FIRMWARE_VERSION` in `AppConfig.h` (e.g. from `"1.0.0"` to `"1.0.1"`).
3. In the Arduino IDE menu, click:
   **Sketch** -> **Export Compiled Binary** (`Ctrl + Alt + S`).
4. The `.bin` file will be created inside the `build/` subdirectory or sketch folder:
   e.g. `ESP8266_RemoteOTA.ino.nodemcuv2.bin`.
5. Publish this `.bin` using Option A, B, or C above.

### In PlatformIO:
1. Increment the version number in `AppConfig.h`.
2. Build the binary:
   ```bash
   pio run -e nodemcuv2
   ```
3. Your compiled binary is at:
   `.pio/build/nodemcuv2/firmware.bin`
4. Publish using the helper script:
   ```bash
   python server/publish_firmware.py .pio/build/nodemcuv2/firmware.bin 1.0.1 "https://your-url/firmware.bin"
   ```

---

## 🧠 Hardware & Memory Limitations (ESP8266 Deep Dive)

When deploying remote OTA updates on ESP8266, you must understand its hardware constraints:

### 1. Dual-Slot Flash Memory Constraint
- The ESP8266 flash is partitioned into **Slot A (active firmware)**, **Slot B (OTA staging)**, and **File System (LittleFS)**.
- For OTA to work, your compiled `.bin` file size **MUST NOT EXCEED** the available OTA slot size:
  - **4MB Flash Boards (NodeMCU, Wemos D1 Mini, ESP-12E/F)**: 
    OTA slot size is approximately **~1019 KB (~1.0 MB)**. A typical project compiles to ~350KB-500KB, leaving plenty of room.
  - **1MB Flash Boards (ESP-01, ESP-01S)**:
    OTA slot size is limited to **~450 KB**. You must minimize libraries and use `eagle.flash.1m64.ld` (64KB FS).
- Our `RemoteOTA` class automatically queries `ESP.getFreeSketchSpace()` and checks it against the manifest file size before initiating download. If flash space is insufficient, it safely aborts and alerts you without corrupting flash.

### 2. RAM & BearSSL TLS Buffer Sizing
- The ESP8266 has only **80 KB of RAM**, and after Wi-Fi and TCP stacks start, you typically have **25 KB to 45 KB of free heap**.
- Standard TLS (HTTPS) uses 16 KB input and 4 KB output buffers (over 20 KB contiguous RAM), which causes catastrophic `[SSL:out of memory]` crashes.
- **Our Solution**:
  ```cpp
  client->setBufferSizes(1024, 1024);
  ```
  This reduces BearSSL's memory footprint by ~18 KB. It uses Maximum Fragment Length Negotiation (MFLN) supported by Cloudflare, GitHub, AWS, and modern web servers.

### 3. CPU Clock Frequency (160 MHz vs 80 MHz)
- Calculating TLS cryptographic operations (RSA / ECDHE / AES-GCM) takes significant CPU cycles.
- Running at **160 MHz** (`board_build.f_cpu = 160000000L` or Tools -> CPU Frequency: 160MHz) cuts TLS handshake time from ~3.5 seconds down to ~1.2 seconds.

### 4. Power Supply Stability During OTA Writes
- During flash erase and write operations combined with active Wi-Fi transmission, the ESP8266 draws sharp current pulses up to **350 mA - 400 mA**.
- Poor power supplies or long thin USB cables will experience voltage drops, causing brownout reboots during flash writes.
- **Recommendation**: Ensure your 3.3V regulator can supply at least 500mA, and place a **100 µF to 470 µF electrolytic capacitor** across 3.3V and GND on custom PCB designs.

---

## 🔒 Security Architecture

1. **Authentication Token (`X-Device-Token`)**:
   Sent in the HTTP request headers during every check-in. The server rejects unauthorized queries with HTTP 401.
2. **Device Identification (`X-Device-Id`)**:
   Computed uniquely from the hardware MAC address (`ESP8266-A4E57C08D12F`). Allows per-device rollout and device ban lists on the server.
3. **MD5 Checksum Verification**:
   The manifest contains the pre-calculated MD5 hash. `ESPhttpUpdate.setMD5()` calculates the hash block-by-block during flash write. If even 1 bit is flipped, the bootloader aborts and restarts into the old, working firmware.
4. **Hardware Model Lock (`X-Hardware-Model`)**:
   Prevents accidentally uploading a binary built for a NodeMCU onto an ESP-01 or custom board with different pinouts.

---

## 💻 Integrating RemoteOTA Into Your Own Future Projects

The OTA framework is completely decoupled. To add it to any future project:

```cpp
#include "WiFiProvisioner.h"
#include "RemoteOTA.h"

WiFiProvisioner provisioner;
RemoteOTA ota;

void setup() {
    Serial.begin(115200);
    
    // Starts LittleFS + Auto-connect or Captive Portal
    provisioner.begin();

    // Setup OTA
    ota.begin(provisioner.getConfig().ota_url, 
              provisioner.getDeviceId(), 
              provisioner.getConfig().device_secret);
}

void loop() {
    provisioner.handle(); // Non-blocking Wi-Fi & button monitor
    
    if (provisioner.isConnected()) {
        ota.handle();     // Periodic background OTA checker
    }

    // Your application code here...
}
```
