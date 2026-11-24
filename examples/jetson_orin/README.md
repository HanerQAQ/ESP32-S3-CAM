# ESP32-S3 CAM NVIDIA Jetson Orin Guide & Examples / Jetson Orin 例程指南

This directory contains I2C communication examples for the **ESP32-S3 CAM** vision module tailored for the **NVIDIA Jetson Orin family (Jetson Orin Nano / Jetson Orin NX / AGX Orin)**.

本目录包含适配 **NVIDIA Jetson Orin 系列主控（Jetson Orin Nano / Jetson Orin NX / AGX Orin）** 的 ESP32-S3 CAM 离线视觉与语音协处理器 I2C 通信例程。所有例程共用统一驱动架构，并针对 Jetson Orin 40-Pin 扩展引脚与 Linux `I2C_RDWR` 进行深度优化。

---

## 1. Hardware Pin Wiring / 硬件引脚连接 (40-Pin Header J12)

The ESP32-S3 CAM module acts as an I2C slave with a fixed 7-bit address of **`0x52`** (HAL address `0xA4`). Connect it to the 40-Pin expansion header (J12) on the Jetson Orin carrier board:

ESP32-S3 CAM 视觉模块采用标准 I2C 接口（从机地址固定为 `0x52`），连接至 Jetson Orin 开发板的 40-Pin 扩展引脚（J12）：

| ESP32-S3 CAM Pin | Jetson Orin 40-Pin (J12) | Pin Function / 功能定义 | Description / 说明 |
| :---: | :---: | :---: | :--- |
| **VCC / 5V** | **Pin 2 or Pin 4** | 5V Power Supply | Recommended DC 5V ≥ 2A supply / 建议 5V 稳压输入 |
| **GND** | **Pin 6 / 9 / 14 / 20 / 25 / 30 / 34 / 39** | Ground (GND) | Common ground / 必须共地连接 |
| **SDA** (`GPIO47`) | **Pin 3** | `I2C2_SDA` (`/dev/i2c-7` or `/dev/i2c-1`) | I2C Data line / 数据线 |
| **SCL** (`GPIO48`) | **Pin 5** | `I2C2_SCL` (`/dev/i2c-7` or `/dev/i2c-1`) | I2C Clock line / 时钟线 |

> [!NOTE]
> - On official **Jetson Orin Nano / Orin NX Developer Kits (P3768 carrier board)**, Pin 3 and Pin 5 map to **`/dev/i2c-7`**.
> - On some third-party carrier boards or AGX Orin, Pin 3 and Pin 5 map to **`/dev/i2c-1`**.
> - The driver includes **smart multi-bus auto-probing** (probing `7 -> 1 -> 8 -> 0`), eliminating the need to manually modify bus numbers!

---

## 2. Environment Setup & Permissions / 运行环境与权限配置

### 2.1 User Permissions (Avoid `Permission Denied`) / 配置用户权限
In Ubuntu / Linux, accessing `/dev/i2c-*` requires membership in the `i2c` group. Run in terminal:
在 Ubuntu 系统中，访问 `/dev/i2c-*` 需要属于 `i2c` 用户组。请在终端执行：
```bash
sudo usermod -aG i2c $USER
```
Log out and log back in (or reboot) for group changes to take effect:
执行后**注销并重新登录**（或重启），即可免 `sudo` 运行 Python 例程。

### 2.2 Verify I2C Bus Detection / 扫描检测模块通信
Install `i2c-tools` and scan for the ESP32-S3 CAM module (address `0x52`):
使用 `i2c-tools` 工具检测 ESP32-S3 CAM 模块（地址 `0x52`）：
```bash
# Install i2c-tools if not present / 安装 i2c-tools
sudo apt-get install -y i2c-tools

# Scan bus 7 (default on Jetson Orin Nano / NX) / 扫描 7 号总线
i2cdetect -y -r 7

# Or scan bus 1 / 或扫描 1 号总线
i2cdetect -y -r 1
```
If you see **`52`** at row `0x50`, column `2`, the physical connection is working properly!
若在矩阵 `0x50` 行的 `2` 列显示 `52`，说明硬件连接完全正常！

---

## 3. Example Matrix & Running / 功能例程与执行方法

| Directory / 目录 | Matching S3 Firmware / 对应固件 | Description / 功能说明 | Run Command / 执行命令 |
| :--- | :--- | :--- | :--- |
| **`01_cat_dog_detect`** | `cat_detect.bin` | Real-time pet detection / 猫狗目标实时检测识别 | `python3 main.py` |
| **`02_color_detect`** | `color_detect.bin` | 4-color block tracking & line following / 4种目标颜色识别与坐标上报 | `python3 main.py` |
| **`03_face_detect`** | `human_face_detect.bin` | Human face detection & count / 实时人脸检测与人脸数量统计 | `python3 main.py` |
| **`04_face_recognition`** | `human_face_recognition.bin` | Face recognition & ID matching / 人脸识别比对（已知 ID 与陌生人区分） | `python3 main.py` |
| **`05_image_transmission`** | `Image_transmission_function_AP.bin` | Camera config (mirror/flip) / 摄像头参数配置与图传流模式 | `python3 main.py` |
| **`06_qrcode`** | `qrcode_recognition.bin` | QR code decoding / 二维码检测与字符串解码 | `python3 main.py` |

### 3.1 Bus Selection Options / 运行参数说明
All examples support specifying the I2C bus number via command-line arguments or environment variables:
所有例程均支持通过命令行参数或环境变量灵活指定 I2C 总线号：
```bash
# Method 1: Automatic bus probing (recommended) / 默认自动探测运行
python3 main.py

# Method 2: Explicitly specify bus 7 / 显式指定总线号为 7
python3 main.py --bus 7
# or
python3 main.py 7

# Method 3: Global environment variable / 通过环境变量指定全局总线号
export ESP32_S3_CAM_I2C_BUS=7
python3 main.py
```

---

## 4. Key Driver Features / 驱动优化关键特性

1. **Unified Driver Architecture / 同源驱动架构**：
   Shares the same binary protocol implementation as `examples/raspberry_pi` and `examples/python_sdk`.
2. **Multi-Fragment Assembly / 多分包流式拼接**：
   Assembles multi-frame payloads (e.g. 4 fragments, 82 bytes for color detection) with streaming sequence validation and zero packet loss.
3. **Dynamic XOR Checksum / 动态异或校验和寻址**：
   Calculates checksums dynamically at offset `8 + Data_Length`, preventing hardcoded index misalignment.
4. **Resync & Error Recovery / 掉同步自动搜寻自恢复**：
   Includes automatic `0x55` header recovery when I2C FIFO shifts, safe file descriptor cleanup, and detailed diagnostics.
