/**
 * ======================================================================================
 * Self-Hosted Node.js / Express OTA Server with Web Dashboard
 * ======================================================================================
 * Features:
 * - Web UI for uploading firmware .bin files
 * - Automatically computes MD5 hash and file size
 * - Handles device check-ins at /api/ota/check
 * - Serves firmware files with HTTP Range support
 * - Runs locally, on VPS, or on free cloud hosts (Render, Railway, Fly.io)
 * ======================================================================================
 */

const express = require('express');
const multer = require('multer');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 3000;
const FIRMWARE_DIR = path.join(__dirname, 'firmware');
const MANIFEST_PATH = path.join(__dirname, 'manifest.json');
const AUTH_TOKEN = process.env.OTA_TOKEN || 'esp8266_secure_token_change_me';

if (!fs.existsSync(FIRMWARE_DIR)) {
  fs.mkdirSync(FIRMWARE_DIR, { recursive: true });
}

// Default manifest state
let manifest = {
  version: "1.0.0",
  bin_url: "",
  md5: "",
  size: 0,
  target_model: "ESP8266-GENERIC",
  force: false,
  notes: "Initial release"
};

if (fs.existsSync(MANIFEST_PATH)) {
  try {
    manifest = JSON.parse(fs.readFileSync(MANIFEST_PATH, 'utf8'));
  } catch (e) {
    console.error("Error reading manifest.json:", e);
  }
}

app.use(express.json());
app.use('/firmware', express.static(FIRMWARE_DIR));

// Configure upload storage
const storage = multer.diskStorage({
  destination: (req, file, cb) => cb(null, FIRMWARE_DIR),
  filename: (req, file, cb) => {
    const ext = path.extname(file.originalname);
    const base = path.basename(file.originalname, ext);
    cb(null, `${base}_${Date.now()}${ext}`);
  }
});
const upload = multer({ storage });

// OTA Check Endpoint for ESP8266 (Shortest path: /ota.log)
app.get(['/ota.log', '/api/ota/check', '/manifest.json'], (req, res) => {
  const token = req.headers['x-device-token'] || req.query.token;
  const deviceId = req.headers['x-device-id'] || 'UNKNOWN';
  const currentVersion = req.headers['x-current-version'] || 'UNKNOWN';

  console.log(`[Check-In] Device: ${deviceId} | Version: ${currentVersion} | IP: ${req.ip}`);

  if (AUTH_TOKEN && token !== AUTH_TOKEN) {
    return res.status(401).json({ error: 'Unauthorized: Invalid device token' });
  }

  res.setHeader('Cache-Control', 'no-cache, no-store, must-revalidate');
  res.json(manifest);
});

// Admin Web Dashboard
app.get('/', (req, res) => {
  res.send(`
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>ESP8266 OTA Firmware Dashboard</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #0f172a; color: #f8fafc; padding: 40px; margin: 0; }
    .card { background: #1e293b; max-width: 600px; margin: 0 auto; padding: 30px; border-radius: 12px; border: 1px solid #334155; box-shadow: 0 10px 30px rgba(0,0,0,0.4); }
    h1 { color: #38bdf8; font-size: 22px; margin-top: 0; }
    .status-box { background: #090e1a; padding: 15px; border-radius: 8px; font-family: monospace; font-size: 13px; line-height: 1.6; margin-bottom: 25px; border: 1px solid #1e293b; color: #94a3b8; }
    .status-box b { color: #38bdf8; }
    label { display: block; margin: 12px 0 6px; font-size: 14px; font-weight: 500; }
    input, textarea { width: 100%; box-sizing: border-box; padding: 10px; background: #0f172a; border: 1px solid #334155; border-radius: 6px; color: #fff; font-size: 14px; }
    button { background: #0284c7; color: white; border: none; padding: 12px 24px; border-radius: 8px; font-size: 15px; font-weight: 600; cursor: pointer; margin-top: 20px; width: 100%; transition: background 0.2s; }
    button:hover { background: #0369a1; }
  </style>
</head>
<body>
  <div class="card">
    <h1>🚀 ESP8266 Remote OTA Management</h1>
    
    <div class="status-box">
      <div>Current Deployed Version: <b>${manifest.version}</b></div>
      <div>Binary MD5: <b>${manifest.md5 || 'None'}</b></div>
      <div>File Size: <b>${manifest.size ? (manifest.size / 1024).toFixed(1) + ' KB' : 'N/A'}</b></div>
      <div>Binary URL: <b>${manifest.bin_url || 'None'}</b></div>
      <div>Target Hardware: <b>${manifest.target_model}</b></div>
    </div>

    <form action="/upload" method="post" enctype="multipart/form-data">
      <label for="version">New Version String (e.g., 1.0.1):</label>
      <input type="text" id="version" name="version" required placeholder="1.0.1">

      <label for="notes">Release Notes:</label>
      <textarea id="notes" name="notes" rows="2" placeholder="Bug fixes, new sensor logic..."></textarea>

      <label for="firmware">Compiled Firmware (.bin file):</label>
      <input type="file" id="firmware" name="firmware" accept=".bin" required>

      <button type="submit">Upload & Publish New Firmware</button>
    </form>
  </div>
</body>
</html>
  `);
});

// Upload and Publish Handler
app.post('/upload', upload.single('firmware'), (req, res) => {
  if (!req.file) {
    return res.status(400).send("No firmware file uploaded.");
  }

  const filePath = req.file.path;
  const fileBuffer = fs.readFileSync(filePath);
  
  // Compute MD5
  const md5Hash = crypto.createHash('md5').update(fileBuffer).digest('hex');
  const host = req.get('host');
  const protocol = req.protocol;

  manifest = {
    version: req.body.version || '1.0.1',
    bin_url: `${protocol}://${host}/firmware/${req.file.filename}`,
    md5: md5Hash,
    size: req.file.size,
    target_model: 'ESP8266-GENERIC',
    force: false,
    notes: req.body.notes || 'Uploaded via Web Dashboard'
  };

  fs.writeFileSync(MANIFEST_PATH, JSON.stringify(manifest, null, 2));

  console.log(`[Publish] New firmware published! Version: ${manifest.version} | MD5: ${manifest.md5} | Size: ${manifest.size} bytes`);
  res.redirect('/');
});

app.listen(PORT, () => {
  console.log(`\n========================================================`);
  console.log(` ESP8266 Remote OTA Server running on port ${PORT}`);
  console.log(` Dashboard: http://localhost:${PORT}/`);
  console.log(` OTA Check: http://localhost:${PORT}/api/ota/check`);
  console.log(`========================================================\n`);
});
