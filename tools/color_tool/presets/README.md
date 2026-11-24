# ColorTool HSV Calibration Presets / 颜色阈值预设库

This directory contains pre-calibrated HSV color threshold profiles for the **ESP32-S3-CAM** color recognition and line-tracking firmware.

本目录包含适用于 **ESP32-S3-CAM** 颜色检测与巡线固件的官方标定 HSV 颜色阈值配置文件。

---

## Included Profiles / 预置配置文件

| File / 文件 | Environment / 适用环境 | Description / 特性说明 |
| :--- | :--- | :--- |
| **[`default_industrial_colors.json`](default_industrial_colors.json)** | Standard Indoor / 室内与竞赛场 | Balanced HSV thresholds for red, blue, green, yellow, and purple under typical fluorescent/LED lighting (300~800 lux). |
| **[`outdoor_high_ambient.json`](outdoor_high_ambient.json)** | High Ambient / 强光与户外 | Elevated saturation ($S_{min} \ge 90$) and value ($V_{min} \ge 80$) filters to reject direct sunlight reflections and shadows. |

---

## How to Use in ColorTool / 在 ColorTool 中使用

1. Launch **ColorTool** via `run.bat` (Windows) or `run.sh` (Linux/macOS).
2. Connect to the camera's USB-UART port (Baud: `921600`).
3. Click the **「导入 (Import)」** button in the top-right toolbar of the *Color Parameter Configuration* panel.
4. Select `default_industrial_colors.json` or `outdoor_high_ambient.json`.
5. Click **「一键下载 (Download)」** to write the parameters directly into the ESP32-S3's non-volatile NVS flash!
