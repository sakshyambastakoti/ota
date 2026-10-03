/**
 * ======================================================================================
 * PC-Hosted Remote OTA Server with Auto-Tunneling
 * ======================================================================================
 * Exposes your local PC to the entire internet using a secure HTTPS tunnel.
 * Zero router port forwarding. Zero configuration needed on your friend's router.
 *
 * Usage:
 *   node start-pc-server.js [custom-subdomain]
 *
 * Example:
 *   node start-pc-server.js my-esp-ota
 *   => Public HTTPS: https://my-esp-ota.loca.lt
 * ======================================================================================
 */

const express = require('express');
const multer = require('multer');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const localtunnel = require('localtunnel');

const app = express();
const PORT = process.env.PORT || 3000;
const FIRMWARE_DIR = path.join(__dirname, 'firmware');
const MANIFEST_PATH = path.join(__dirname, 'manifest.json');
const AUTH_TOKEN = process.env.OTA_TOKEN || 'esp8266_secure_token_change_me';
const DESIRED_SUBDOMAIN = process.argv[2] || process.env.SUBDOMAIN || 'esp-remote-ota-' + Math.floor(1000 + Math.random() * 9000);

if (!fs.existsSync(FIRMWARE_DIR)) {
  fs.mkdirSync(FIRMWARE_DIR, { recursive: true });
}

let publicTunnelUrl = `http://localhost:${PORT}`;

// Load manifest
let manifest = {
  version: "1.0.0",
  bin_url: "",
  md5: "",
  size: 0,
  target_model: "ESP8266-GENERIC",
  force: false,
  notes: "Initial build"
};

if (fs.existsSync(MANIFEST_PATH)) {
  try {
    manifest = JSON.parse(fs.readFileSync(MANIFEST_PATH, 'utf8'));
  } catch (e) {
    console.error("Error reading manifest:", e);
  }
}

app.use(express.json());
app.use('/firmware', express.static(FIRMWARE_DIR));

const storage = multer.diskStorage({
  destination: (req, file, cb) => cb(null, FIRMWARE_DIR),
  filename: (req, file, cb) => {
    const ext = path.extname(file.originalname);
    const base = path.basename(file.originalname, ext);
    cb(null, `${base}_${Date.now()}${ext}`);
  }
});
const upload = multer({ storage });

// Handler function to return manifest
const sendManifest = (req, res) => {
  const token = req.headers['x-device-token'] || req.query.token;
  const deviceId = req.headers['x-device-id'] || 'UNKNOWN';
  const currentVersion = req.headers['x-current-version'] || 'UNKNOWN';

  console.log(`\n[ESP8266 Check-In] Device: ${deviceId} | Board Ver: ${currentVersion} | IP: ${req.ip}`);

  if (AUTH_TOKEN && token !== AUTH_TOKEN) {
    console.log(`[Auth Reject] Invalid token from ${deviceId}`);
    return res.status(401).json({ error: 'Unauthorized: Invalid device token' });
  }

  if (manifest.latest_filename) {
    manifest.bin_url = `${publicTunnelUrl}/firmware/${manifest.latest_filename}`;
  }

  res.setHeader('Cache-Control', 'no-cache, no-store, must-revalidate');
  res.json(manifest);
};

// OTA check endpoints (Shortest path: /ota.log or direct domain)
app.get(['/ota.log', '/api/ota/check', '/manifest.json'], sendManifest);

// Root path: If ESP8266 device calls it, send manifest; otherwise show Web GUI
app.get('/', (req, res) => {
  if (req.headers['x-device-id'] || (req.headers['user-agent'] && req.headers['user-agent'].includes('ESP8266'))) {
    return sendManifest(req, res);
  }
  res.send(`
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>ESP8266 PC Remote OTA Console</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #0b1329; color: #f8fafc; padding: 30px; margin: 0; }
    .card { background: #16203c; max-width: 650px; margin: 0 auto; padding: 30px; border-radius: 16px; border: 1px solid #23315a; box-shadow: 0 10px 30px rgba(0,0,0,0.6); }
    h1 { color: #38bdf8; font-size: 22px; margin-top: 0; }
    .tunnel-badge { background: #064e3b; border: 1px solid #059669; color: #34d399; padding: 10px 14px; border-radius: 8px; font-family: monospace; font-size: 13px; margin-bottom: 20px; display: flex; justify-content: space-between; align-items: center; }
    .status-box { background: #0c142b; padding: 16px; border-radius: 10px; font-family: monospace; font-size: 13px; line-height: 1.8; margin-bottom: 25px; border: 1px solid #1f2b4d; color: #94a3b8; }
    .status-box b { color: #38bdf8; }
    label { display: block; margin: 14px 0 6px; font-size: 14px; font-weight: 500; color: #cbd5e1; }
    input, textarea { width: 100%; box-sizing: border-box; padding: 10px 12px; background: #0b1329; border: 1px solid #2b3a67; border-radius: 8px; color: #fff; font-size: 14px; outline: none; }
    input:focus { border-color: #38bdf8; }
    button { background: linear-gradient(135deg, #0284c7, #2563eb); color: white; border: none; padding: 14px 24px; border-radius: 10px; font-size: 15px; font-weight: 600; cursor: pointer; margin-top: 22px; width: 100%; box-shadow: 0 4px 15px rgba(2,132,199,0.4); }
    button:hover { opacity: 0.95; }
    .note { font-size: 12px; color: #64748b; margin-top: 15px; text-align: center; }
  </style>
</head>
<body>
  <div class="card">
    <h1>💻 PC-Hosted Remote OTA Engine</h1>
    
    <div class="tunnel-badge">
      <div>🌐 Live Public Internet Tunnel</div>
      <div><b>${publicTunnelUrl}</b></div>
    </div>

    <div class="status-box">
      <div>Deployed Version: <b>${manifest.version}</b></div>
      <div>MD5 Checksum:   <b>${manifest.md5 || 'None'}</b></div>
      <div>Binary Size:    <b>${manifest.size ? (manifest.size / 1024).toFixed(1) + ' KB' : 'N/A'}</b></div>
      <div>OTA Endpoint:   <b>${publicTunnelUrl}/ota.log</b></div>
    </div>

    <form action="/upload" method="post" enctype="multipart/form-data">
      <label for="version">New Version String (e.g. 1.0.1):</label>
      <input type="text" id="version" name="version" required placeholder="1.0.1" value="1.0.1">

      <label for="notes">Release Notes / Changelog:</label>
      <input type="text" id="notes" name="notes" placeholder="New sensor algorithm and bug fixes">

      <label for="firmware">Select Compiled Firmware (.bin):</label>
      <input type="file" id="firmware" name="firmware" accept=".bin" required>

      <button type="submit">🚀 Push Firmware Directly to Remote Board</button>
    </form>
    
    <div class="note">
      When your friend's ESP8266 checks in, it will stream the firmware directly from your PC!
    </div>
  </div>
</body>
</html>
  `);
});

// Upload and publish handler
app.post('/upload', upload.single('firmware'), (req, res) => {
  if (!req.file) {
    return res.status(400).send("No firmware file uploaded.");
  }

  const filePath = req.file.path;
  const fileBuffer = fs.readFileSync(filePath);
  const md5Hash = crypto.createHash('md5').update(fileBuffer).digest('hex');

  manifest = {
    version: req.body.version || '1.0.1',
    latest_filename: req.file.filename,
    bin_url: `${publicTunnelUrl}/firmware/${req.file.filename}`,
    md5: md5Hash,
    size: req.file.size,
    target_model: 'ESP8266-GENERIC',
    force: true, // Force update flag ensures immediate trigger
    notes: req.body.notes || 'Pushed from PC'
  };

  fs.writeFileSync(MANIFEST_PATH, JSON.stringify(manifest, null, 2));

  console.log(`\n======================================================`);
  console.log(`🚀 [NEW FIRMWARE PUBLISHED FROM YOUR PC]`);
  console.log(` Version:  ${manifest.version}`);
  console.log(` MD5:      ${manifest.md5}`);
  console.log(` Size:     ${(manifest.size / 1024).toFixed(1)} KB`);
  console.log(` Public:   ${manifest.bin_url}`);
  console.log(` Waiting for remote ESP8266 to download...`);
  console.log(`======================================================\n`);

  res.redirect('/');
});

// Start local Express server
const server = app.listen(PORT, async () => {
  console.log(`\n[Local Server] Running on http://localhost:${PORT}`);
  console.log(`[Tunnel] Initializing secure HTTPS tunnel to the global internet...`);

  try {
    const tunnel = await localtunnel({ 
      port: PORT, 
      subdomain: DESIRED_SUBDOMAIN 
    });

    publicTunnelUrl = tunnel.url;

    console.log(`\n=============================================================`);
    console.log(`  🚀 ESP8266 REMOTE OTA SERVER IS LIVE FROM YOUR PC!`);
    console.log(`=============================================================`);
    console.log(` Local Dashboard:   http://localhost:${PORT}/`);
    console.log(` Public Internet:   ${publicTunnelUrl}`);
    console.log(` OTA Check URL:     ${publicTunnelUrl}/ota.log`);
    console.log(`=============================================================`);
    console.log(` Set your ESP8266 OTA URL to:`);
    console.log(` ${publicTunnelUrl}/ota.log`);
    console.log(`\n Your friend's board can now connect from anywhere in the world!`);
    console.log(` Press Ctrl+C to stop.\n`);

    tunnel.on('close', () => {
      console.log('\n[Tunnel] Closed. Reconnecting...');
    });

  } catch (err) {
    console.error(`[Tunnel Error] Could not establish tunnel:`, err.message);
    console.log(`You can still test locally on http://localhost:${PORT}`);
  }
});
