#!/usr/bin/env python3
"""
Update SC01 Smart Panel device configuration with corrected MQTT topics.

This script reads the devices_updated.json file and merges it with the
LittleFS spiffs_image, ensuring all devices have the correct cmnd_topic
configured to match Homebridge's setOn topics.

Usage:
    python3 update_devices_littlefs.py
"""

import json
import os
import sys
import shutil
from pathlib import Path

def update_devices_json():
    """Read updated devices config and write to data/devices.json"""
    devices_updated_path = Path(__file__).parent / "devices_updated.json"
    data_dir = Path(__file__).parent / "data"
    devices_json_path = data_dir / "devices.json"
    
    if not devices_updated_path.exists():
        print(f"[ERROR] {devices_updated_path} not found!")
        return False
    
    data_dir.mkdir(parents=True, exist_ok=True)
    
    try:
        # Read the updated devices config. The firmware expects
        # {"devices": [...]} at the filesystem root.
        with open(devices_updated_path, 'r', encoding='utf-8') as f:
            payload = json.load(f)

        devices = payload.get("devices", payload)
        wrapped_payload = {"devices": devices}

        # Write to data/devices.json (will be packed into LittleFS)
        with open(devices_json_path, 'w', encoding='utf-8') as f:
            json.dump(wrapped_payload, f, indent=2, ensure_ascii=False)

        print(f"[OK] Updated devices.json with {len(devices)} devices")
        print(f"[OK] Written to: {devices_json_path}")
        print("\nDevices configured:")
        for i, dev in enumerate(devices, 1):
            state_topic = dev.get('state_topic', '')
            cmnd_topic = dev.get('cmnd_topic', '')
            print(f"  {i}. {dev['name']}")
            print(f"     State: {state_topic}")
            print(f"     Cmnd:  {cmnd_topic if cmnd_topic else '(none)'}")
        
        return True
    except json.JSONDecodeError as e:
        print(f"[ERROR] Invalid JSON in {devices_updated_path}: {e}")
        return False
    except Exception as e:
        print(f"[ERROR] {e}")
        return False

if __name__ == "__main__":
    success = update_devices_json()
    sys.exit(0 if success else 1)
