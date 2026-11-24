<div align="center">

# ESP32-S3-CAM

<img src="docs/assets/module_usb.png" width="520" alt="ESP32-S3-CAM AI Vision Module"/>

**Compact Edge AI Vision & 2.4 GHz Wi-Fi Streaming Module**

[![CI](https://github.com/HanerQAQ/ESP32-S3-CAM/actions/workflows/ci.yml/badge.svg)](https://github.com/HanerQAQ/ESP32-S3-CAM/actions/workflows/ci.yml)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v6.1-E7352C?logo=espressif&logoColor=white)](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Python](https://img.shields.io/badge/Python-3.10%2B-3776AB?logo=python&logoColor=white)](examples/python_sdk/)

**English** | [简体中文](README_zh.md)

</div>

---

## 1. Overview & Product Line Comparison

**ESP32-S3-CAM** is an ultra-compact (`42 × 27 × 19 mm` stacked Core + Carrier assembly) offline AI vision and wireless video streaming module built around the **ESP32-S3** dual-core Xtensa LX7 MCU (`240 MHz`, AI vector instructions, `8 MB` Octal PSRAM, `8 MB` Flash) and a **GC2145 2MP** camera sensor with an onboard CH340 USB-to-UART bridge.

As the compact **twin sibling** of [**ESP32-P4C5-CAM**](https://github.com/HanerQAQ/ESP32-P4C5-CAM), both repositories share a unified directory structure, a consistent `0xAA 0x55` 32-byte fragmented I²C vision protocol, cross-platform host SDKs (`Arduino`, `MicroPython`, `STM32 HAL`, `Raspberry Pi`, `Jetson Orin`), and the `ColorTool` HSV calibration desktop studio:

| Feature | **ESP32-S3-CAM** *(This Repo)* | [**ESP32-P4C5-CAM**](https://github.com/HanerQAQ/ESP32-P4C5-CAM) *(Twin Flagship)* |
| :--- | :--- | :--- |
| **SoC Architecture** | ESP32-S3 Dual-Core LX7 @ 240 MHz | ESP32-P4 (Dual-Core RISC-V @ 400 MHz) + ESP32-C5 (RISC-V @ 240 MHz) |
| **Camera Interface** | Parallel DVP (`GC2145` 2MP) | 2-Lane MIPI-CSI (`GC2145` 2MP) + Hardware ISP / JPEG / PPA |
| **Wireless Connectivity** | 2.4 GHz Wi-Fi 4 (`802.11 b/g/n`) + BLE 5.0 | Dual-Band 2.4 & 5 GHz Wi-Fi 6 (`802.11ax`) + BLE 5.0 |
| **I²C Slave Address** | `0x52` (`SDA=GPIO47`, `SCL=GPIO48`) | `0x54` Offline AI / `0x52` Streaming (`SDA=GPIO46`, `SCL=GPIO47`) |
| **Firmware Applications** | **7** ESP-IDF v6.1 projects (Cat/Dog, Face Detect/Recognition, Color/Line, QR, AP/STA Stream) | **12** ESP-IDF v6.1 projects (YOLO11n 80-Class, 17-Keypoint Pose, Hand Gesture, AprilTag 6-DoF, Face, Cat, Dog, Pedestrian, Color/Line, QR, AP/STA Stream) |
| **Dimensions** | `42.0 × 27.0 × 19.0 mm` (Stacked Core + Carrier, `40 × 27 mm` PCB) | `42.0 × 27.0 × 19.0 mm` (Stacked Core + Carrier, `40 × 27 mm` PCB) |
| **Purchase Link (Taobao)** | [ESP32-S3-CAM Hardware Store](https://item.taobao.com/item.htm?id=791851247779) | [ESP32-P4C5-CAM Hardware Store](https://item.taobao.com/item.htm?id=1089874435189) |

---

## 2. Hardware Specifications & Interfaces

<div align="center">
  <img src="docs/assets/pinout.png" width="460" alt="ESP32-S3-CAM Pinout and Interfaces"/>
</div>

| Component | Specification |
| :--- | :--- |
| **Compute & Memory** | ESP32-S3-N8R8 Dual-Core LX7 @ `240 MHz`, `8 MB` Octal PSRAM, `8 MB` SPI Flash |
| **Image Sensor** | `GC2145` 2MP CMOS sensor via DVP (`XCLK = 20 MHz`, SCCB `SDA=GPIO4`, `SCL=GPIO5`) |
| **USB Debug & Flash** | Onboard Type-C receptacle via CH340 USB-UART bridge (`GPIO43 TXD0`, `GPIO44 RXD0`) |
| **I²C Host Bus (`0x52`)** | `5264-4P` keyed connector (`5V`, `GND`, `SCL=GPIO48`, `SDA=GPIO47`, `3.3V` TTL) |
| **Controls & LED** | `BOOT` (`GPIO0` active low; 1s hold in STA firmware resets Wi-Fi), `RST`, Status LED (`GPIO2`) |
| **Power Requirements** | **`DC 5.0 V ≥ 2.0 A`** (shared `GND` with host controller mandatory for I²C signal integrity) |

- **Hardware Documentation**: [Assembly & Lens Focus Guide](docs/hardware_guide_en.md) | [Schematics & Datasheets](hardware/README.md) | [Mechanical Dimensions SVG](hardware/mechanical_dimensions.svg) | [`SCH_S3-Cam_V1.0.pdf`](hardware/SCH_S3-Cam_V1.0.pdf)

---

## 3. Firmware Matrix (`ESP-IDF v6.1`)

All 7 source projects live in [`firmware/`](firmware/README.md) and compile into single `0x0`-offset merged binaries (see [`prebuilt_firmware/README.md`](prebuilt_firmware/README.md) and **[GitHub Releases](https://github.com/HanerQAQ/ESP32-S3-CAM/releases)**).

| Firmware Project | Vision / Streaming Capability | Default SoftAP & Web UI | UART Baud | Source |
| :--- | :--- | :--- | :---: | :--- |
| **`Image_transmission_function_STA`** | Dual-mode STA + SoftAP provisioning Web MJPEG stream *(Factory Default)* | `ESP32-S3-CAM-XXXXXX` (`192.168.5.1/wifi`) | `115200` | [`firmware/Image_transmission_function_STA`](firmware/Image_transmission_function_STA) |
| **`Image_transmission_function_AP`** | Dedicated SoftAP Wi-Fi MJPEG stream (AI disabled for max FPS) | `ESP32-S3-CAM-WIFI` (`192.168.4.1`) | `115200` | [`firmware/Image_transmission_function_AP`](firmware/Image_transmission_function_AP) |
| **`cat_detect`** | Offline Cat & Dog dual-class object detection (`ESP-DL`) | `ESP32-S3-CAM-WIFI` (`192.168.4.1`) | `115200` | [`firmware/cat_detect`](firmware/cat_detect) |
| **`color_detect`** | 5-channel HSV color blob detection & line tracking (`Flash` config) | `ESP32-S3-CAM-WIFI` (`192.168.4.1`) | **`921600`** | [`firmware/color_detect`](firmware/color_detect) |
| **`human_face_detect`** | Real-time human face bounding-box detection | `ESP32-S3-CAM-WIFI` (`192.168.4.1`) | `115200` | [`firmware/human_face_detect`](firmware/human_face_detect) |
| **`human_face_recognition`** | Face enrollment, deletion & recognition persisted in Flash `fr` partition | `ESP32-S3-CAM-WIFI` (`192.168.4.1`) | `115200` | [`firmware/human_face_recognition`](firmware/human_face_recognition) |
| **`qrcode_recognition`** | Offline QR code localization & UTF-8 text decoding | `ESP32-S3-CAM-WIFI` (`192.168.4.1`) | `115200` | [`firmware/qrcode_recognition`](firmware/qrcode_recognition) |

<details>
<summary>Preview: Web Streaming & STA Wi-Fi Provisioning UI</summary>

<p align="center">
  <img src="docs/assets/web_ap_stream.png" width="380" alt="AP Stream UI"/>
  <img src="docs/assets/web_sta_prov.png" width="380" alt="STA Provisioning UI"/>
</p>

</details>

---

## 4. Quick Start

### 4.1 Build Firmware from Source (`ESP-IDF v6.1`)

Activate your **ESP-IDF v6.1** shell environment with the `xtensa-esp-elf` (`esp32s3`) toolchain installed, then build a single project or batch-export all 7 merged binaries:

```bash
# Batch build all 7 firmware projects & generate 0x0 merged binaries in prebuilt_firmware/
cd firmware
python build_and_verify_all.py --clean

# Or build & flash a single project directly:
cd cat_detect
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

### 4.2 One-Click Flash Prebuilt Binaries (`0x0`)

Generate `0x0`-offset merged `.bin` images locally via `firmware/build_and_verify_all.py` (or download release assets from **[GitHub Releases](https://github.com/HanerQAQ/ESP32-S3-CAM/releases)** when published) into `prebuilt_firmware/`, then flash at offset `0x0`:

```bash
# Interactive launcher (Windows: flash.bat | Linux/macOS: ./flash.sh)
python tools/flash_firmware.py --port COMx --bin prebuilt_firmware/cat_detect.bin --erase

# Or invoke esptool directly:
python -m esptool --chip esp32s3 -p COMx -b 921600 write-flash 0x0 prebuilt_firmware/cat_detect.bin
```

### 4.3 Verify I²C Host Wiring (`0x52`)

```bash
# Dry-run protocol frame validation (no hardware required)
python tools/i2c_diagnose.py --mock

# Live Linux SBC / Raspberry Pi / Jetson probe
python tools/i2c_diagnose.py --bus 1 --addr 0x52
```

---

## 5. Multi-Platform Host SDKs & Examples

| Host Platform | Language / Framework | Driver Path | Demo Coverage | Guide |
| :--- | :--- | :--- | :---: | :--- |
| **Python SDK** *(Unified Core)* | Python 3.7+ (`smbus` / `ioctl`) | [`examples/python_sdk/`](examples/python_sdk/) | All 6 protocol modes | [`examples/python_sdk/README.md`](examples/python_sdk/README.md) |
| **Raspberry Pi** | Python (`/dev/i2c-1`) | [`examples/raspberry_pi/`](examples/raspberry_pi/) | `01` ~ `06` | [`examples/raspberry_pi/README.md`](examples/raspberry_pi/README.md) |
| **NVIDIA Jetson Orin** | Python (`/dev/i2c-7`) | [`examples/jetson_orin/`](examples/jetson_orin/) | `01` ~ `06` | [`examples/jetson_orin/README.md`](examples/jetson_orin/README.md) |
| **ESP32 Arduino / PlatformIO** | C++ Library (`ESP32_S3_CAM`) | [`examples/esp32_arduino/`](examples/esp32_arduino/) | `01` ~ `06` | [`examples/esp32_arduino/README.md`](examples/esp32_arduino/README.md) |
| **ESP32 MicroPython** | MicroPython (`machine.I2C`) | [`examples/esp32_micropython/`](examples/esp32_micropython/) | `01` ~ `06` | [`examples/esp32_micropython/README.md`](examples/esp32_micropython/README.md) |
| **STM32 HAL** | C Driver + CMake Interface | [`examples/stm32_hal/`](examples/stm32_hal/) | `01` ~ `06` | [`examples/stm32_hal/README.md`](examples/stm32_hal/README.md) |

```python
from esp32_s3_cam import ESP32_S3_CAM, RawI2C

with RawI2C(bus=1) as bus:
    cam = ESP32_S3_CAM(bus)
    result = cam.get_cat_dog_result()
    print(result)
```

---

## 6. ColorTool Desktop Studio (`tools/color_tool/`)

<div align="center">
  <img src="docs/assets/color_tool_ui.png" width="660" alt="ColorTool v1.0 Desktop Calibration UI"/>
</div>

A **PySide6 + OpenCV** calibration utility for `color_detect` firmware (`921600` baud over USB Type-C): capture live camera frames, click pixels to sample HSV bounds (including red wrap-around), inspect real-time segmentation masks, manage JSON lighting presets, and save parameters permanently into Flash.

```bash
cd tools/color_tool
pip install -r requirements.txt
python main.py
```

---

## 7. Repository Structure

```text
ESP32-S3-CAM/
├── .github/
│   ├── ISSUE_TEMPLATE/             # Bug report & feature request forms
│   ├── PULL_REQUEST_TEMPLATE.md    # Pull request verification checklist
│   └── workflows/
│       ├── ci.yml                  # Python matrix tests, SDK/Arduino checks, links & PlatformIO build
│       └── release.yml             # Windows ColorTool executable packaging & GitHub Release
├── docs/
│   ├── assets/                     # UI screenshots, module renders, pinout diagrams
│   ├── hardware_guide_en.md        # Hardware assembly & lens focus guide (EN)
│   ├── hardware_guide_zh.md        # Hardware assembly & lens focus guide (ZH)
│   ├── i2c_protocol_en.md          # Complete 0x52 I2C protocol specification (EN)
│   ├── i2c_protocol_zh.md          # Complete 0x52 I2C protocol specification (ZH)
│   ├── uart_protocol_en.md         # ColorTool UART binary framing protocol (EN)
│   └── uart_protocol_zh.md         # ColorTool UART binary framing protocol (ZH)
├── examples/
│   ├── python_sdk/                 # Installable 'esp32-s3-cam' Python package
│   ├── raspberry_pi/               # Raspberry Pi /dev/i2c-1 demos (01 ~ 06)
│   ├── jetson_orin/                # NVIDIA Jetson Orin /dev/i2c-7 demos (01 ~ 06)
│   ├── esp32_arduino/              # Arduino 1.5 library ('ESP32_S3_CAM') + PlatformIO config
│   ├── esp32_micropython/          # MicroPython library & scripts (01 ~ 06)
│   └── stm32_hal/                  # Portable STM32 HAL C driver & CMake targets
├── firmware/
│   ├── components/                 # Shared ESP-IDF v6.1 components (esp32-camera, esp-dl, etc.)
│   ├── Image_transmission_function_STA/
│   ├── Image_transmission_function_AP/
│   ├── cat_detect/
│   ├── color_detect/
│   ├── human_face_detect/
│   ├── human_face_recognition/
│   ├── qrcode_recognition/
│   ├── build_and_verify_all.py     # Batch ESP-IDF v6.1 compiler & merge-bin exporter
│   └── clean_for_sharing.py        # Firmware build artifact cleaner
├── hardware/                       # PDF schematics, SoC/sensor datasheets & mechanical SVG
├── prebuilt_firmware/              # Staging directory & instructions for 0x0 merged binaries
├── tests/                          # Automated unit tests (driver, tools, firmware & hygiene)
├── tools/
│   ├── color_tool/                 # PySide6 HSV threshold tuning studio
│   ├── git_hooks/                  # Pre-commit repository hygiene hook
│   ├── clean_repo.py               # Python cache & artifact cleaner
│   ├── flash_firmware.py           # Cross-platform esptool wrapper
│   └── i2c_diagnose.py             # I2C bus & frame integrity diagnostic CLI
├── CODE_OF_CONDUCT.md
├── CONTRIBUTING.md
├── LICENSE                         # MIT License (Copyright (c) 2026 HanerQAQ)
├── NOTICE
└── SECURITY.md
```

---

## 8. Documentation Index

- **Protocol Specifications**: [I²C Vision Protocol (EN)](docs/i2c_protocol_en.md) | [I²C 视觉协议 (中文)](docs/i2c_protocol_zh.md) | [UART Calibration Protocol (EN)](docs/uart_protocol_en.md) | [串口调参协议 (中文)](docs/uart_protocol_zh.md)
- **Hardware & Mechanics**: [Hardware & Focus Guide (EN)](docs/hardware_guide_en.md) | [硬件装配与调焦 (中文)](docs/hardware_guide_zh.md) | [Schematics & Pinout](hardware/README.md)
- **Firmware & Tools**: [Firmware Guide](firmware/README.md) | [Prebuilt Firmware Guide](prebuilt_firmware/README.md) | [Developer Tools](tools/README.md) | [Unit Tests](tests/README.md)

---

## 9. Contributing & License

- **Contributions**: Please read [`CONTRIBUTING.md`](CONTRIBUTING.md), [`CODE_OF_CONDUCT.md`](CODE_OF_CONDUCT.md), and [`SECURITY.md`](SECURITY.md) before opening issues or pull requests.
- **License**: Released under the **[MIT License](LICENSE)** (`Copyright (c) 2026 HanerQAQ`). See [`NOTICE`](NOTICE) for bundled open-source third-party component attributions (`esp32-camera`, `esp-dl`, `esp-code-scanner`, `mdns`, `PySide6`, `OpenCV`).
