@echo off
title ESP32-S3 CAM Firmware Flasher
echo ========================================================
echo  ESP32-S3 CAM One-Click Firmware Flashing Launcher
echo ========================================================
python "%~dp0tools\flash_firmware.py" %*
if errorlevel 1 pause
