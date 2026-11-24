#!/usr/bin/env bash
# ==============================================================================
# ESP32-S3 CAM Firmware Flasher Launcher for Linux & macOS
# ==============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if command -v python3 &>/dev/null; then
    PYTHON_CMD="python3"
elif command -v python &>/dev/null; then
    PYTHON_CMD="python"
else
    echo "[!] Error: Python 3 was not found in your PATH."
    echo "    Please install Python 3 (e.g. sudo apt install python3 python3-pip)."
    exit 1
fi

echo "========================================================"
echo " ESP32-S3 CAM One-Click Firmware Flashing Launcher"
echo "========================================================"
exec "$PYTHON_CMD" "$SCRIPT_DIR/tools/flash_firmware.py" "$@"
