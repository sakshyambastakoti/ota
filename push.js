#!/usr/bin/env node
/**
 * 1-Command Push: Compiles and publishes firmware directly from your PC
 */

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const { execSync } = require('child_process');

const FIRMWARE_BIN = path.join(__dirname, '.pio', 'build', 'nodemcuv2', 'firmware.bin');
const SERVER_FIRMWARE_DIR = path.join(__dirname, 'server', 'firmware');
const MANIFEST_PATH = path.join(__dirname, 'server', 'manifest.json');

console.log("\n=======================================================");
console.log("  🚀 1-COMMAND PUSH: ESP8266 FIRMWARE FROM PC");
console.log("=======================================================\n");

// 1. Build firmware if PlatformIO is present
try {
  console.log("[1/3] Building latest firmware binary with PlatformIO...");
  execSync('pio run -e nodemcuv2', { stdio: 'inherit' });
} catch (e) {
  console.log("[Notice] Pio run skipped or finished. Checking for existing .bin file...");
}

if (!fs.existsSync(FIRMWARE_BIN)) {
  console.error(`\nError: Could not locate compiled binary at: ${FIRMWARE_BIN}`);
  console.log("If using Arduino IDE, click 'Sketch' -> 'Export Compiled Binary',");
  console.log("and drop your .bin into server/firmware/ folder.\n");
  process.exit(1);
}

// 2. Copy and compute MD5
console.log("\n[2/3] Computing MD5 and staging firmware...");
const fileBuffer = fs.readFileSync(FIRMWARE_BIN);
const md5Hash = crypto.createHash('md5').update(fileBuffer).digest('hex');
const fileSize = fs.statSync(FIRMWARE_BIN).size;

if (!fs.existsSync(SERVER_FIRMWARE_DIR)) {
  fs.mkdirSync(SERVER_FIRMWARE_DIR, { recursive: true });
}

const targetName = `firmware_update_${Date.now()}.bin`;
const destPath = path.join(SERVER_FIRMWARE_DIR, targetName);
fs.copyFileSync(FIRMWARE_BIN, destPath);

// 3. Update manifest
let manifest = {
  version: "1.0.1",
  target_model: "ESP8266-GENERIC",
  notes: "Direct push from local PC"
};

if (fs.existsSync(MANIFEST_PATH)) {
  try { manifest = JSON.parse(fs.readFileSync(MANIFEST_PATH, 'utf8')); } catch (e) {}
}

// Increment minor/patch version automatically if desired
const verParts = (manifest.version || "1.0.0").split('.').map(Number);
verParts[2] = (verParts[2] || 0) + 1;
const newVersion = verParts.join('.');

manifest.version = newVersion;
manifest.latest_filename = targetName;
manifest.md5 = md5Hash;
manifest.size = fileSize;
manifest.force = true; // Ensures the ESP8266 immediately accepts it
manifest.notes = `Pushed from PC at ${new Date().toLocaleTimeString()}`;

fs.writeFileSync(MANIFEST_PATH, JSON.stringify(manifest, null, 2));

console.log("[3/3] Manifest updated successfully!");
console.log(`\n=======================================================`);
console.log(`  🎉 Firmware v${newVersion} is ready to push!`);
console.log(`  MD5:   ${md5Hash}`);
console.log(`  Size:  ${(fileSize / 1024).toFixed(1)} KB`);
console.log(`=======================================================\n`);
console.log("Make sure your PC server is running:");
console.log("  npm run pc-server (in another terminal)");
console.log("\nYour friend's ESP8266 will download it on its next check!\n");
