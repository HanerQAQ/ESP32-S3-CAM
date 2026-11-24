# Multi-Host I2C Examples & Drivers / 多主控 I2C 示例与驱动库

This directory provides ready-to-use I2C master drivers and 6 matching example sketches/scripts for **5 host platforms**, verified against the [Offline Vision I2C Protocol](../docs/i2c_protocol_en.md) (`0x52`, `100 kHz`, JSON request + 32-byte fixed fragment frames + XOR checksum).

本目录提供适配 **五大主控平台** 的离线视觉 I2C 通信驱动与 6 大功能对应示例，均严格遵循 [离线视觉 I2C 功能协议](../docs/i2c_protocol_zh.md)（从机地址 `0x52`、速率 `100 kHz`、JSON 指令 + 32 字节固定分片帧 + XOR 校验）。

---

## Platform Overview / 平台目录一览

| Platform | Directory | Driver Files | Default I2C Pins / Bus | Power / Logic Notes |
| :--- | :--- | :--- | :--- | :--- |
| **ESP32 Arduino** | [`esp32_arduino/`](esp32_arduino) | [`ESP32_S3_CAM`](esp32_arduino/ESP32_S3_CAM) (Global Library) / [`platformio.ini`](esp32_arduino/platformio.ini) | `Wire`: `SDA = GPIO21`, `SCL = GPIO22` | See [`esp32_arduino/README.md`](esp32_arduino/README.md) |
| **ESP32 MicroPython** | [`esp32_micropython/`](esp32_micropython) | [`lib/ESP32_S3_CAM.py`](esp32_micropython/lib/ESP32_S3_CAM.py) (Deploy to `/lib`) | `I2C(0)`: `SDA = GPIO21`, `SCL = GPIO22` | See [`esp32_micropython/README.md`](esp32_micropython/README.md) |
| **STM32 HAL** | [`stm32_hal/`](stm32_hal) | [`driver/`](stm32_hal/driver) + [`CMakeLists.txt`](stm32_hal/CMakeLists.txt) | `hi2c2`: `PB11 = SDA`, `PB10 = SCL` | CubeMX HAL driver + 6 `app.c` demos |
| **Raspberry Pi** | [`raspberry_pi/`](raspberry_pi) | Uses unified [`python_sdk`](python_sdk) (`RawI2C`) | `/dev/i2c-1`: `Pin 3 SDA (GPIO2)`, `Pin 5 SCL (GPIO3)` | See [`raspberry_pi/README.md`](raspberry_pi/README.md) |
| **NVIDIA Jetson Orin** | [`jetson_orin/`](jetson_orin) | Uses unified [`python_sdk`](python_sdk) (auto-probe) | `Pin 3 SDA`, `Pin 5 SCL` (`/dev/i2c-7` or `1`) | See [`jetson_orin/README.md`](jetson_orin/README.md) |
| **Linux Python SDK** | [`python_sdk/`](python_sdk) | `esp32-s3-cam` installable package | Auto-probe `/dev/i2c-1, 7, 8, 0` | See [`python_sdk/README.md`](python_sdk/README.md) |

---

## 6 Example Cases per Platform / 每平台对应 6 个示例

| Folder / Demo | Required S3-CAM Firmware (`0x0`) | Function Demonstrated |
| :--- | :--- | :--- |
| `01_cat_dog_detect` | `cat_detect.bin` | Cat/Dog bounding box & class readout |
| `02_color_detect` | `color_detect.bin` | Target color registration, block coordinates & line-tracking offset/angle |
| `03_face_detect` | `human_face_detect.bin` | Face count & bounding box detection |
| `04_face_recognition` | `human_face_recognition.bin` | Face enrollment (`face_register`), identification (`face_id`), deletion (`face_delete`) |
| `05_image_transmission` | `Image_transmission_function_AP.bin` | Dynamic camera `hmirror` / `vflip` control while streaming on `192.168.4.1` |
| `06_qrcode` | `qrcode_recognition.bin` | Multi-fragment QR code UTF-8 string decoding & bounding box |

---

## Quick Usage Notes / 各平台运行提示

<a id="esp32-arduino"></a>
### ESP32 Arduino
Install [`examples/esp32_arduino/ESP32_S3_CAM`](esp32_arduino/ESP32_S3_CAM) into your Arduino `libraries/` directory, or use [`platformio.ini`](esp32_arduino/platformio.ini) (`pio run -e cat_dog_detect`). See [`esp32_arduino/README.md`](esp32_arduino/README.md).

<a id="esp32-micropython"></a>
### ESP32 MicroPython
Upload [`lib/ESP32_S3_CAM.py`](esp32_micropython/lib/ESP32_S3_CAM.py) to `/lib/` on your board once. Run any `main.py` directly. See [`esp32_micropython/README.md`](esp32_micropython/README.md).

<a id="stm32-hal"></a>
### STM32 HAL
Use [`driver/`](stm32_hal/driver) or the root [`CMakeLists.txt`](stm32_hal/CMakeLists.txt) in a STM32CubeMX or CLion project with `hi2c2` enabled at `100 kHz`. See [`stm32_hal/README.md`](stm32_hal/README.md).

<a id="raspberry-pi"></a>
### Raspberry Pi
See [`raspberry_pi/README.md`](raspberry_pi/README.md) for 40-pin GPIO pinout, `raspi-config` setup, and shared `python_sdk` integration.

### NVIDIA Jetson Orin
See [`examples/jetson_orin/README.md`](jetson_orin/README.md) for `/dev/i2c-*` group permissions, multi-bus auto-detection (`7 -> 1 -> 8 -> 0`), and CLI options (`--bus 7`).
