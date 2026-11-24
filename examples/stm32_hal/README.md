# STM32 HAL I2C Driver & Examples / STM32 HAL 库 I2C 驱动与例程

This directory extracts the clean, portable **ESP32-S3 CAM HAL driver** and **6 functional application examples (`app.c`)** from the full STM32CubeMX / Keil MDK project, omitting heavyweight vendor CMSIS / HAL / LVGL / build artifacts so you can integrate it into any STM32 project cleanly.

本目录从完整 STM32CubeMX / Keil 工程中提取了纯净可移植的 **ESP32-S3 CAM HAL 通信驱动** 与 **6 大视觉功能应用示例（`app.c`）**，剥离了体积庞大的芯片库与构建产物，方便直接集成到任意 STM32 HAL 工程中。

---

## Directory Contents / 目录结构

```text
stm32_hal/
├── driver/
│   ├── ESP32_S3_CAM.h         # High-level vision API, structs & error codes
│   ├── ESP32_S3_CAM.c         # JSON serializer, 32-byte frame parser, XOR checksum & packet retry
│   ├── ESP32_S3_CAM_port.h    # Bus config: addr 0x52 (HAL 0xA4), 100 kHz, 1024B buffer
│   └── ESP32_S3_CAM_port.c    # STM32 HAL porting layer using extern I2C_HandleTypeDef hi2c2
└── demos/
    ├── 01_cat_dog_detect_app.c
    ├── 02_color_detect_app.c
    ├── 03_face_detect_app.c
    ├── 04_face_recognition_app.c
    ├── 05_image_transmission_app.c
    └── 06_qrcode_app.c
```

---

## Hardware Wiring (Default Reference) / 默认硬件接线

| ESP32-S3 CAM | STM32F4 Reference Board | Function |
| :---: | :---: | :--- |
| `5V` | `5V` | Power input (`>= 2A` recommended) |
| `GND` | `GND` | Common ground (required) |
| `SDA` (`GPIO47`) | `PB11` (`I2C2_SDA`) | I2C Data (`3.3V` TTL) |
| `SCL` (`GPIO48`) | `PB10` (`I2C2_SCL`) | I2C Clock (`100 kHz`) |

---

## Integration Steps / 移植接入步骤

### Option A: Modern CMake Integration / CMake 工程接入（推荐）

Add `examples/stm32_hal` to your STM32 CMake project:

```cmake
add_subdirectory(examples/stm32_hal)
target_link_libraries(your_firmware_target PRIVATE ESP32_S3_CAM::ESP32_S3_CAM)
```

The target automatically provides include directories for `driver/` (`ESP32_S3_CAM.h` and `ESP32_S3_CAM_port.h`).

### Option B: Keil MDK / STM32CubeIDE Integration / 传统工程直接接入

1. **Configure I2C in STM32CubeMX**: Enable `I2C2` (`100 kHz` standard speed, 7-bit addressing).
   - If your board uses a different I2C peripheral (e.g. `hi2c1` on `PB6`/`PB7`), simply add `-DESP32_S3_CAM_I2C_HANDLE=hi2c1` to your compiler definitions, or change `#define ESP32_S3_CAM_I2C_HANDLE hi2c2` in `driver/ESP32_S3_CAM_port.h`. No driver internal modifications needed.
2. **Add Source Files**: Copy `driver/ESP32_S3_CAM.{c,h}` and `driver/ESP32_S3_CAM_port.{c,h}` into your Keil / STM32CubeIDE / Makefile project and add `driver/` to include paths.
3. **Wire Application Logic**: Call `esp32_s3_cam_port_init(&cam)` and query results in your task loop:
   ```c
   #include "ESP32_S3_CAM.h"
   #include "ESP32_S3_CAM_port.h"

   static esp32_s3_cam_handle_t cam;

   int main(void) {
       HAL_Init();
       SystemClock_Config();
       MX_I2C2_Init();
       
       esp32_s3_cam_port_init(&cam);
       
       while (1) {
           if (!esp32_s3_cam_port_is_ready(&cam)) {
               HAL_Delay(100);
               continue;
           }
           
           esp32_s3_cam_cat_dog_detect_result_t result;
           if (esp32_s3_cam_get_cat_dog_detect_result(&cam, &result) == ESP32_S3_CAM_OK) {
               printf("Detected targets: %u\r\n", result.count);
           }
           HAL_Delay(100);
       }
   }
   ```
4. **Flash Matching Firmware**: Ensure the S3-CAM runs the matching `.bin` from `prebuilt_firmware/`.
