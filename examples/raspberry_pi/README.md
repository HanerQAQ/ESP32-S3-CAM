# ESP32-S3 CAM Raspberry Pi Guide & Examples / 树莓派例程指南

This directory provides I2C master example scripts for connecting the **ESP32-S3-CAM** module to a **Raspberry Pi** (Pi 5, Pi 4B, Pi 3B+, CM4, Zero 2W).

本目录提供适用于 **树莓派（Raspberry Pi 5 / 4B / 3B+ / CM4 / Zero 2W）** 的 ESP32-S3 CAM 视觉模块 I2C 通信例程。所有例程共用标准 `python_sdk`（或原生回退），实现统一驱动管理。

---

## 1. Hardware Wiring / 硬件接线 (40-Pin GPIO)

The ESP32-S3 CAM communicates as an I2C slave at 7-bit address **`0x52`** (100 kHz standard mode).

| ESP32-S3 CAM Pin | Raspberry Pi 40-Pin Header | Pin Function | Description / 说明 |
| :---: | :---: | :---: | :--- |
| **5V / VCC** | **Pin 2 or Pin 4** | 5V Power | Recommended DC 5V ≥ 2A supply / 推荐独立或稳压供电 |
| **GND** | **Pin 6 / 9 / 14 / 20 / 25 / 30 / 34 / 39** | Ground | Common ground / 必须共地 |
| **SDA** (`GPIO47`) | **Pin 3** | `GPIO2 (I2C1_SDA)` | I2C Data line / 数据线 |
| **SCL** (`GPIO48`) | **Pin 5** | `GPIO3 (I2C1_SCL)` | I2C Clock line / 时钟线 |

> [!IMPORTANT]
> Ensure the Raspberry Pi and the ESP32-S3 CAM share a solid common ground (GND). If the camera video has horizontal interference ripples, provide an independent `DC 5V >= 2A` power supply to the camera module.

---

## 2. Environment Setup / 环境配置与权限

### Step 1: Enable I2C Interface / 开启 I2C 接口
```bash
sudo raspi-config
# Navigate to: Interface Options -> I2C -> Enable -> Finish
```
Or directly edit `/boot/config.txt` (or `/boot/firmware/config.txt` on Bookworm) and ensure `dtparam=i2c_arm=on` is present, then reboot.

### Step 2: User Permission / 添加用户权限
```bash
sudo usermod -aG i2c $USER
# Log out and log back in (or reboot) for permissions to take effect:
sudo reboot
```

### Step 3: Verify Bus Connection / 扫描检测模块
```bash
# Install i2c-tools if not already present
sudo apt-get install -y i2c-tools

# Scan I2C bus 1
i2cdetect -y 1
```
You should see **`52`** at row `0x50`, column `2`.

---

## 3. Running Examples / 运行功能例程

Each directory corresponds to an ESP-IDF firmware mode:

| Directory / 目录 | Required Firmware / 烧录固件 | Description / 功能说明 | Run Command / 执行命令 |
| :--- | :--- | :--- | :--- |
| **`01_cat_dog_detect`** | `cat_detect.bin` | Cat & Dog Detection / 猫狗识别检测 | `python3 main.py` |
| **`02_color_detect`** | `color_detect.bin` | Multi-color block tracking / 多色块坐标与巡线 | `python3 main.py` |
| **`03_face_detect`** | `human_face_detect.bin` | Human face detection / 人脸检测与数量统计 | `python3 main.py` |
| **`04_face_recognition`** | `human_face_recognition.bin` | Face recognition / 人脸识别（已知 ID 与陌生人） | `python3 main.py` |
| **`05_image_transmission`** | `Image_transmission_function_AP.bin` | Stream control (mirror & flip) / 图传与镜像配置 | `python3 main.py` |
| **`06_qrcode`** | `qrcode_recognition.bin` | QR code decoding / 二维码检测与字符串解码 | `python3 main.py` |

```bash
# Example: Run cat and dog detection demo
cd 01_cat_dog_detect
python3 main.py
```

---

## 4. Optional: Install Unified Python SDK / 安装标准 Python SDK

For system-wide library access across your custom robotics scripts:

```bash
cd ../python_sdk
pip install -e .

# Now importable in any Python script:
# from esp32_s3_cam import ESP32_S3_CAM, RawI2C
```
