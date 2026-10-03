/**
 * ======================================================================================
 * Cloudflare Worker - ESP8266 Remote OTA Management Engine
 * ======================================================================================
 * - 100% Free on Cloudflare's free tier (100,000 requests/day).
 * - Internet-accessible, global low-latency CDN, HTTPS by default.
 * - Handles device authentication tokens, device tracking, and version management.
 * - Serves JSON manifest and routes download requests to your firmware storage.
 * ======================================================================================
 */

// Global Firmware Release Configuration
const LATEST_RELEASE = {
  version: "1.0.1",
  target_model: "ESP8266-GENERIC",
  // URL where your compiled .bin is hosted (GitHub Releases, Cloudflare R2, AWS S3, etc.)
  bin_url: "https://github.com/your-username/esp8266-firmware/releases/download/v1.0.1/firmware.bin",
  // MD5 checksum of the compiled .bin file (lowercase hex, 32 characters)
  md5: "4a7d1ed414474e4033ac29ccb8653d9b",
  size: 384512,
  force: false,
  notes: "Production patch: optimized sensor loops and BearSSL buffer sizing."
};

// Security Settings
const ALLOWED_SECRET_TOKEN = "esp8266_secure_token_change_me";

export default {
  async fetch(request, env, ctx) {
    const url = new URL(request.url);

    // Endpoint 1: Health check
    if (url.pathname === "/") {
      return new Response(JSON.stringify({ 
        service: "ESP8266 Remote OTA Worker", 
        status: "operational",
        latest_version: LATEST_RELEASE.version 
      }), {
        headers: { "Content-Type": "application/json" }
      });
    }

    // Endpoint 2: OTA Manifest Check (/ota.log, /api/ota/check, or /manifest.json)
    if (url.pathname === "/ota.log" || url.pathname === "/api/ota/check" || url.pathname === "/manifest.json") {
      const deviceId = request.headers.get("X-Device-Id") || url.searchParams.get("device_id") || "UNKNOWN";
      const deviceToken = request.headers.get("X-Device-Token") || url.searchParams.get("token") || "";
      const currentVersion = request.headers.get("X-Current-Version") || url.searchParams.get("current_version") || "0.0.0";
      const hardwareModel = request.headers.get("X-Hardware-Model") || "UNKNOWN";

      // Verify Authentication Token if configured
      if (ALLOWED_SECRET_TOKEN && deviceToken !== ALLOWED_SECRET_TOKEN) {
        return new Response(JSON.stringify({ error: "Unauthorized: Invalid device token" }), {
          status: 401,
          headers: { "Content-Type": "application/json" }
        });
      }

      console.log(`[OTA Check] Device: ${deviceId} | Ver: ${currentVersion} | Model: ${hardwareModel} | IP: ${request.headers.get("CF-Connecting-IP")}`);

      // Return the current release manifest
      return new Response(JSON.stringify(LATEST_RELEASE), {
        status: 200,
        headers: {
          "Content-Type": "application/json",
          "Cache-Control": "no-cache, no-store, must-revalidate"
        }
      });
    }

    // Endpoint 3: Status / Telemetry Report from ESP8266
    if (url.pathname === "/api/ota/report" && request.method === "POST") {
      const report = await request.json().catch(() => ({}));
      console.log("[Device Report]", JSON.stringify(report));
      return new Response(JSON.stringify({ success: true }), {
        headers: { "Content-Type": "application/json" }
      });
    }

    return new Response("Not Found", { status: 404 });
  }
};
