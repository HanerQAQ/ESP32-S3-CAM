# ESP32 Arduino Driver & Examples / Arduino 驱动库与例程

This folder provides Arduino-compatible C++ drivers and 6 functional examples for communicating with the **ESP32-S3-CAM** vision module over I2C (`0x52`, `100 kHz`).

本目录提供适用于 Arduino IDE / PlatformIO 的 C++ 通信驱动与 6 大视觉功能例程，通过 I2C 接口（从机地址 `0x52`、标准速率 `100 kHz`）与 **ESP32-S3 CAM** 视觉模块通信。

---

## Hardware Connection / 硬件接线

| ESP32-S3 CAM | Host ESP32 (e.g. ESP32 Dev Module) | Description |
| :---: | :---: | :--- |
| `5V` | `5V` | Supply `DC 5V >= 2A` to the CAM module |
| `GND` | `GND` | Common ground (must be connected) |
| `SDA` (`GPIO47`) | `GPIO21` (`Wire SDA`) | Standard ESP32 I2C data |
| `SCL` (`GPIO48`) | `GPIO22` (`Wire SCL`) | Standard ESP32 I2C clock |

> **Note**: If using a custom ESP32 expansion board (e.g. SDA=17, SCL=16), adjust `#define I2C_SDA` and `#define I2C_SCL` in the sketch.

---

## Two Usage Options / 两种使用方式

### Option 1: Standard Arduino IDE / 标准 Arduino IDE 使用方式（推荐）

1. Copy the [`ESP32_S3_CAM/`](ESP32_S3_CAM) directory into your Arduino libraries folder:
   - **Windows**: `Documents/Arduino/libraries/ESP32_S3_CAM`
   - **macOS / Linux**: `~/Arduino/libraries/ESP32_S3_CAM`
   - *Or* zip the `ESP32_S3_CAM` folder and click **Sketch -> Include Library -> Add .ZIP Library...** in the Arduino IDE.
2. In Arduino IDE, open **File -> Examples -> ESP32_S3_CAM** and select any demo:
   - `01_cat_dog_detect`
   - `02_color_detect`
   - `03_face_detect`
   - `04_face_recognition`
   - `05_image_transmission`
   - `06_qrcode`
3. Select board **ESP32 Dev Module**, choose the correct COM port, and click **Upload**.
4. Open Serial Monitor at **`115200`** baud.

### Option 2: PlatformIO Command-Line Build / PlatformIO 一键构建

This directory includes a pre-configured [`platformio.ini`](platformio.ini). You can compile and flash without Arduino IDE:

```bash
# Build a specific demo environment
pio run -e cat_dog_detect
pio run -e color_detect

# Build and flash directly to connected ESP32
pio run -e cat_dog_detect -t upload
```

---

## Example Matrix / 示例对应表

| Demo Folder | Required Firmware (`0x0`) | Function / 说明 |
| :--- | :--- | :--- |
| [`01_cat_dog_detect`](ESP32_S3_CAM/examples/01_cat_dog_detect) | `cat_detect.bin` | Detect cat/dog presence, bounding box, confidence |
| [`02_color_detect`](ESP32_S3_CAM/examples/02_color_detect) | `color_detect.bin` | Track color blocks and line-following angle/offset |
| [`03_face_detect`](ESP32_S3_CAM/examples/03_face_detect) | `human_face_detect.bin` | Detect human faces and count in frame |
| [`04_face_recognition`](ESP32_S3_CAM/examples/04_face_recognition) | `human_face_recognition.bin` | Enroll, recognize (`ID=1..N`), and delete face IDs |
| [`05_image_transmission`](ESP32_S3_CAM/examples/05_image_transmission) | `Image_transmission_function_AP.bin` | Control camera `hmirror`/`vflip` via I2C while AP streaming |
| [`06_qrcode`](ESP32_S3_CAM/examples/06_qrcode) | `qrcode_recognition.bin` | Multi-fragment QR code detection & UTF-8 string decoding |
