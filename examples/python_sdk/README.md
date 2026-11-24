# ESP32-S3 CAM Linux Python SDK / Linux 单板机 Python 驱动库

Unified Python driver for communicating with the **ESP32-S3-CAM** module over I2C on Linux single-board computers, including **Raspberry Pi (all models)**, **NVIDIA Jetson (Orin, Xavier, Nano)**, **Orange Pi**, and **Rock Pi**.

适用于 Linux 单板计算机（树莓派全系列、NVIDIA Jetson Orin/Xavier/Nano、香橙派等）的统一 Python I2C 通信驱动库，基于 Linux 内核 `I2C_RDWR` ioctl 实现纯二进制直连通信，零第三方库依赖。

---

## Installation / 安装

From the repository root:

```bash
cd examples/python_sdk
pip install .

# Or install in editable mode:
pip install -e .
```

---

## Quick Example / 快速调用示例

```python
from esp32_s3_cam import ESP32_S3_CAM, RawI2C

# Automatically probes active I2C bus (/dev/i2c-1 on RPi, /dev/i2c-7 on Jetson Orin)
# Or specify explicitly: RawI2C(bus=1)
with RawI2C() as i2c:
    cam = ESP32_S3_CAM(i2c, addr=0x52)
    
    # 1. Query cat & dog detection results
    result = cam.get_cat_dog_detect_result()
    print("Detected targets:", result["items"])

    # 2. Query face detection results
    faces = cam.get_face_detect_result()
    print("Faces found:", faces["items"])

    # 3. Adjust camera orientation dynamically
    cam.set_hmirror(True)
    cam.set_vflip(True)
```

---

## Hardware Pinout Reference / 硬件接线

| ESP32-S3 CAM | Raspberry Pi (40-Pin Header) | Jetson Orin (40-Pin Header) | Description |
| :---: | :---: | :---: | :--- |
| `5V` | Pin 2 or 4 (`5V`) | Pin 2 or 4 (`5V`) | Module power input (`>= 2A` recommended) |
| `GND` | Pin 6, 9, 14, 20, 25, 30, 34, 39 (`GND`) | Pin 6, 9, 14, 20, 25, 30, 34, 39 (`GND`) | Common ground (required) |
| `SDA` (`GPIO47`) | Pin 3 (`GPIO2 / I2C1_SDA`) | Pin 3 (`I2C_SDA`) | 3.3V I2C Data |
| `SCL` (`GPIO48`) | Pin 5 (`GPIO3 / I2C1_SCL`) | Pin 5 (`I2C_SCL`) | 3.3V I2C Clock |

---

## Troubleshooting Permissions / 权限设置

If you encounter `Permission denied` when accessing `/dev/i2c-*`:

```bash
sudo usermod -aG i2c $USER
# Re-login or reboot for group changes to take effect
```
