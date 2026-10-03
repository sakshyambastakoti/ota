#!/usr/bin/env python3
"""
CLI Utility to calculate MD5 and build manifest.json for ESP8266 Remote OTA
Usage:
    python publish_firmware.py <path_to_bin> <version> <public_download_url> [release_notes]

Example:
    python publish_firmware.py firmware.bin 1.0.1 https://github.com/user/repo/releases/download/v1.0.1/firmware.bin "Fix sensor read bug"
"""

import sys
import hashlib
import json
import os

def compute_md5(filepath):
    hasher = hashlib.md5()
    with open(filepath, 'rb') as f:
        while chunk := f.read(8192):
            hasher.update(chunk)
    return hasher.hexdigest()

def main():
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(1)

    bin_path = sys.argv[1]
    version = sys.argv[2]
    bin_url = sys.argv[3]
    notes = sys.argv[4] if len(sys.argv) > 4 else f"Release version {version}"

    if not os.path.exists(bin_path):
        print(f"Error: Binary file '{bin_path}' not found!")
        sys.exit(1)

    file_size = os.path.getsize(bin_path)
    md5_hash = compute_md5(bin_path)

    manifest = {
        "version": version,
        "bin_url": bin_url,
        "md5": md5_hash,
        "size": file_size,
        "target_model": "ESP8266-GENERIC",
        "force": False,
        "notes": notes
    }

    output_path = os.path.join(os.path.dirname(__file__), "manifest.json")
    with open(output_path, "w") as f:
        json.dump(manifest, f, indent=2)

    print("\n========================================================")
    print("  ESP8266 OTA Manifest Generated Successfully!")
    print("========================================================")
    print(f" Version:     {version}")
    print(f" Size:        {file_size} bytes ({file_size / 1024:.1f} KB)")
    print(f" MD5 Hash:    {md5_hash}")
    print(f" Download URL:{bin_url}")
    print(f" Manifest:    {output_path}")
    print("========================================================\n")

if __name__ == "__main__":
    main()
