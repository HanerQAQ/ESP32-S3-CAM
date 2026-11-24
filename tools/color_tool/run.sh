#!/usr/bin/env bash
# ColorTool v1.0 Launcher for Linux and macOS
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if command -v python3 &>/dev/null; then
    PYTHON_CMD="python3"
elif command -v python &>/dev/null; then
    PYTHON_CMD="python"
else
    echo "[ERROR] Python 3 was not found in your PATH."
    echo "Please install Python 3.10+ (e.g. sudo apt install python3-pip python3-pyside6)."
    exit 1
fi

echo "Starting ColorTool Studio..."
exec "$PYTHON_CMD" main.py "$@"
