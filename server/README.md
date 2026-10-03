# Server-Side Deployment Guides for ESP8266 Remote OTA

Choose one of three hosting methods depending on your preferences.

---

## ⚡ Option 1: Cloudflare Worker (Recommended - 100% Free & Zero Maintenance)

Cloudflare Workers gives you **100,000 free HTTPS requests daily**, world-class low latency, and built-in TLS with zero infrastructure to manage.

### Step-by-Step Setup:
1. Log in or create a free account at [https://dash.cloudflare.com](https://dash.cloudflare.com).
2. On the left navigation bar, click **Workers & Pages** -> **Create application** -> **Create Worker**.
3. Name your worker (e.g., `esp-ota-updater`) and click **Deploy**.
4. Click **Quick Edit** or **Edit code**.
5. Replace the entire code editor with the contents of [`server/cloudflare-worker.js`](file:///e:/Downloads/ota/server/cloudflare-worker.js).
6. Edit the top configuration object:
   ```javascript
   const LATEST_RELEASE = {
     version: "1.0.1",
     target_model: "ESP8266-GENERIC",
     bin_url: "https://your-public-storage.com/firmware_v1.0.1.bin",
     md5: "your_computed_md5_hash",
     size: 384512,
     force: false,
     notes: "First remote update!"
   };
   ```
7. Click **Save and Deploy**.
8. Your remote OTA endpoint is now live at:
   `https://esp-ota-updater.<your-subdomain>.workers.dev/api/ota/check`

Whenever you want to trigger an update for all boards:
- Update `version`, `bin_url`, and `md5` in the worker and click Save and Deploy.
- Within your configured interval (e.g. 30 mins), every board worldwide will automatically pull and install the update!

---

## 📦 Option 2: GitHub Releases + Raw Manifest (Free & Serverless)

If you don't even want a serverless worker, you can use GitHub directly:

1. Create a public repository (e.g., `https://github.com/yourname/esp8266-ota`).
2. Go to **Releases** -> **Draft a new release**.
3. Set the release tag to `v1.0.1`.
4. Attach your compiled `firmware.bin` file and click **Publish release**.
5. Copy the direct download link for the binary.
6. Run the local python helper:
   ```bash
   python server/publish_firmware.py firmware.bin 1.0.1 "https://github.com/yourname/esp8266-ota/releases/download/v1.0.1/firmware.bin" "Bug fix"
   ```
7. Commit the generated [`server/manifest.json`](file:///e:/Downloads/ota/server/manifest.json) to your repository.
8. Set the ESP8266 manifest URL to the GitHub Raw URL:
   `https://raw.githubusercontent.com/yourname/esp8266-ota/main/server/manifest.json`

---

## 🖥️ Option 3: Self-Hosted Node.js / Express Server

For local testing on your LAN or running on your own VPS / Cloud VM:

### 1. Installation:
```bash
cd server
npm install
```

### 2. Start the Server:
```bash
npm start
```
By default, the server runs on port 3000.

### 3. Web Dashboard:
- Open `http://localhost:3000/` in your browser.
- Enter the version number (e.g., `1.0.1`).
- Select your compiled `.bin` file.
- Click **Upload & Publish New Firmware**.
- The server computes the MD5 hash on upload and serves the updated manifest at `/api/ota/check`.
