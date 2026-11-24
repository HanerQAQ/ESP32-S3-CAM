"""
ESP32-S3 CAM Python SDK for Linux Single-Board Computers (Raspberry Pi, NVIDIA Jetson, etc.)
"""

from .driver import (
    ESP32_S3_CAM,
    ESP32_S3_CAM_Error,
    ESP32_S3_CAM_I2C_ADDR,
    RawI2C,
    VISION_MODE_COLOR,
    VISION_MODE_LINE,
)

__all__ = [
    "ESP32_S3_CAM",
    "ESP32_S3_CAM_Error",
    "ESP32_S3_CAM_I2C_ADDR",
    "RawI2C",
    "VISION_MODE_COLOR",
    "VISION_MODE_LINE",
]

__version__ = "1.0.0"
