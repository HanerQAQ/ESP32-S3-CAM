# ESP-IDF v6.1 Firmware Source / ESP-IDF v6.1 固件源码

This directory contains all 7 ESP-IDF v6.1 firmware projects and shared ESP32-S3 components for the ESP32-S3-CAM module.

本目录包含 ESP32-S3-CAM 视觉模块的全部 7 套 **ESP-IDF v6.1** 工程源码及公共组件。

---

## 1. Projects / 工程列表

| Project Directory | Vision Mode / 功能 | I2C `get_info` / Command | Notes |
| :--- | :--- | :--- | :--- |
| `cat_detect/` | Cat & Dog Detection / 猫狗检测 | `"cat_dog"` | ESP-DL dual-class detector |
| `color_detect/` | Color Tracking & Line Following / 颜色识别与巡线 | `"color_config"`, `"color_result"` | UART0 **`921600`** baud for `tools/color_tool` |
| `human_face_detect/` | Human Face Detection / 人脸检测 | `"human"` | Dual-stage CNN face detector |
| `human_face_recognition/` | Face Recognition / 人脸识别 | `"face_recognition"`, `"face_register"`, `"face_delete"`, `"face_count"` | Saves embeddings to dedicated `fr` Flash partition |
| `Image_transmission_function_AP/` | AP Wi-Fi Video Streaming / AP 图传 | `vision_setting` (`hmirror`/`vflip`) | Pure SoftAP `ESP32-S3-CAM-WIFI` (`192.168.4.1`) |
| `Image_transmission_function_STA/` | STA Provisioning Streaming / STA 配网图传 | `vision_setting` (`hmirror`/`vflip`) | Provisioning AP `ESP32-S3-CAM-XXXXXX` (`192.168.5.1/wifi`), `BOOT` 1s hold |
| `qrcode_recognition/` | QR Code Decoding / 二维码识别 | `"qrcode"` | Includes test sample any standard printed QR code |

> **Security Note / 安全提示**：Hardcoded lab Wi-Fi router credentials in `*/main/src/app_mywifi.c` (`wifi_SSID` / `wifi_PASSWD`) have been replaced with `"YOUR_WIFI_SSID"` / `"YOUR_WIFI_PASSWORD"` placeholders. All AP-mode projects default to SoftAP `ESP32-S3-CAM-WIFI` (`wifi_Mode = 0`) unless STA mode is explicitly configured.

---

## 2. Shared Components (`components/`) / 公共组件说明

- `esp32-camera/` — OV2640 / GC2145 camera driver adapted for ESP-IDF v6.1 GDMA / LEDC / GPIO APIs.
- `esp-dl/` — Espressif deep-learning library pruned to **ESP32-S3 (`lib/esp32s3/`)** and headers (`include/`) for fast Git cloning.
- `esp-code-scanner/` — QR code and barcode recognition engine.
- `fb_gfx/` — Framebuffer graphics drawing primitives.
- `mdns/` — mDNS local service discovery component.
- `dns_server/` — SoftAP captive-portal DNS server (used by `Image_transmission_function_STA`).
- `modules/` — Shared AI utilities and common Web UI assets.

---

## 3. Building & Exporting / 编译与导出指南

Activate an **ESP-IDF v6.1** shell environment before running any build command.

请在已激活 **ESP-IDF v6.1** 环境变量的终端中执行以下命令：

### Batch Build All 7 Projects / 一键批量编译全部工程

Outputs merged single-file binaries (`0x0`) directly to [`../prebuilt_firmware/`](../prebuilt_firmware/README.md):

```bash
# Clean build all 7 projects + export merged binaries
python build_and_verify_all.py --clean

# Incremental build for selected project(s)
python build_and_verify_all.py cat_detect color_detect
```

### Build & Flash a Single Project / 单工程编译与烧录

```bash
cd cat_detect
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

### Clean Build Caches / 清理构建缓存

```bash
# Remove build/, sdkconfig.old, and __pycache__/ (keeps sdkconfig and managed_components/)
python clean_for_sharing.py

# Also remove managed_components/, sdkconfig, and dependencies.lock
python clean_for_sharing.py --with-managed
```
