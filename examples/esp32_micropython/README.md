# ESP32 MicroPython Driver & Examples / MicroPython 驱动与例程

This folder provides a pure MicroPython I2C master driver (`ESP32_S3_CAM.py`) and 6 runnable example scripts for communicating with the **ESP32-S3-CAM** vision module.

本目录提供纯 MicroPython 实现的 I2C 主机驱动（`ESP32_S3_CAM.py`）与 6 大视觉功能例程，通过 I2C 接口（从机地址 `0x52`、速率 `100 kHz`）与 **ESP32-S3 CAM** 模块交互。

---

## Hardware Connection / 硬件接线

| ESP32-S3 CAM | Host ESP32 (MicroPython) | Description |
| :---: | :---: | :--- |
| `5V` | `5V` | Supply `DC 5V >= 2A` to the CAM module |
| `GND` | `GND` | Common ground (must be connected) |
| `SDA` (`GPIO47`) | `GPIO21` (`I2C_SDA`) | Standard MicroPython ESP32 I2C data |
| `SCL` (`GPIO48`) | `GPIO22` (`I2C_SCL`) | Standard MicroPython ESP32 I2C clock |

---

## Directory Structure / 目录结构

```text
esp32_micropython/
├── lib/
│   └── ESP32_S3_CAM.py        # Central driver module (deploy to /lib on ESP32)
├── 01_cat_dog_detect/         # Case 1: Cat & Dog detection
│   └── main.py
├── 02_color_detect/           # Case 2: Color block & line tracking
│   └── main.py
├── 03_face_detect/            # Case 3: Human face detection
│   └── main.py
├── 04_face_recognition/       # Case 4: Face enrollment & recognition
│   └── main.py
├── 05_image_transmission/     # Case 5: Camera flip/mirror control while AP streaming
│   └── main.py
└── 06_qrcode/                 # Case 6: QR code string decoding
    └── main.py
```

---

## How to Run / 使用方法

### Method 1: Global `/lib` Deployment / 全局模块安装（推荐）

Upload `lib/ESP32_S3_CAM.py` once to your board's `/lib` folder. Then any script can simply `import ESP32_S3_CAM`:

- **Using `mpremote` (CLI)**:
  ```bash
  mpremote connect COMx fs cp lib/ESP32_S3_CAM.py :lib/ESP32_S3_CAM.py
  mpremote connect COMx run 01_cat_dog_detect/main.py
  ```
- **Using Thonny IDE**:
  1. Connect your ESP32 running MicroPython.
  2. In Thonny's file pane, create a `lib` folder on the ESP32 device if it doesn't exist.
  3. Upload `lib/ESP32_S3_CAM.py` into the device's `/lib/` folder.
  4. Open any `main.py` in Thonny and click **Run**.

### Method 2: Running with Local Driver Fallback / 本地路径自动回退

Each subfolder's `main.py` is configured with automatic path discovery (`sys.path.append('/lib', '..', '../lib')`). If `ESP32_S3_CAM.py` is placed in `/lib` on your board or inside the repository's `lib/` directory, any example runs directly without copying duplicate driver files into each subfolder:

每个子例程目录中的 `main.py` 均配置了智能路径回退（优先检索 `/lib`，继而回退至父目录及 `../lib/`）。只需将中央库 `lib/ESP32_S3_CAM.py` 存放在开发板的 `/lib` 目录，各子目录即可直接运行，无需在每个子文件夹中重复存放驱动文件。

---

## Example Matrix / 示例对应表

| Demo Folder | Required S3-CAM Firmware (`0x0`) | Description / 说明 |
| :--- | :--- | :--- |
| `01_cat_dog_detect` | `cat_detect.bin` | Query cat/dog coordinates, confidence, bounding boxes |
| `02_color_detect` | `color_detect.bin` | Set target color slot (1..5), read centroid & line-track angle/offset |
| `03_face_detect` | `human_face_detect.bin` | Real-time face count and coordinates |
| `04_face_recognition` | `human_face_recognition.bin` | Register face (`face_register`), identify (`face_id`), delete IDs |
| `05_image_transmission` | `Image_transmission_function_AP.bin` | Toggle camera `hmirror`/`vflip` via I2C |
| `06_qrcode` | `qrcode_recognition.bin` | Read decoded QR payload text and corner coordinates |
