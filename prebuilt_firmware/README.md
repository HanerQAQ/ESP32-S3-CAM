# Prebuilt Merged Firmware (`0x0`) / 预编译一体化固件

Merged single-file factory binaries (bootloader + partition table + `ota_data`/`phy_init` + application image) can be downloaded from **[GitHub Releases](https://github.com/HanerQAQ/ESP32-S3-CAM/releases)** or generated locally via ESP-IDF v6.1 (`idf.py merge-bin`). All merged images flash cleanly at offset **`0x0`**.

本目录用于存放通过 ESP-IDF v6.1 `idf.py merge-bin` 生成的单文件完整合并固件（已包含引导程序、分区表与应用程序），烧录起始地址统一为 **`0x0`**。可前往 **[GitHub Releases](https://github.com/HanerQAQ/ESP32-S3-CAM/releases)** 下载官方发布二进制包，或在本地执行一键批量构建生成。

---

## Binary Matrix / 固件清单

| Merged Binary (`0x0`) | Function / 功能说明 | Default Access / 默认热点与地址 |
| :--- | :--- | :--- |
| `cat_detect.bin` | Cat & Dog Detection / 猫狗视觉检测 | `ESP32-S3-CAM-WIFI` (`192.168.4.1`) |
| `color_detect.bin` | Color Tracking & Line Following / 颜色识别与巡线（UART `921600`） | `ESP32-S3-CAM-WIFI` (`192.168.4.1`) |
| `human_face_detect.bin` | Human Face Detection / 人脸检测 | `ESP32-S3-CAM-WIFI` (`192.168.4.1`) |
| `human_face_recognition.bin` | Face Recognition (Flash `fr` partition) / 人脸识别（掉电保存） | `ESP32-S3-CAM-WIFI` (`192.168.4.1`) |
| `Image_transmission_function_AP.bin` | AP Mode Video Streaming / AP 热点实时图传 | `ESP32-S3-CAM-WIFI` (`192.168.4.1`) |
| `Image_transmission_function_STA.bin` | STA Provisioning Video Streaming / STA 网页配网图传 | `ESP32-S3-CAM-XXXXXX` (`192.168.5.1/wifi`) |
| `qrcode_recognition.bin` | QR Code Decoding / 二维码扫描解码 | `ESP32-S3-CAM-WIFI` (`192.168.4.1`) |

To generate all 7 merged binaries (plus `SHA256SUMS.txt` when staged) locally with **ESP-IDF v6.1**:

```bash
cd firmware
python build_and_verify_all.py --clean
```

---

## Flashing Methods / 烧录方法

### Option A: Interactive One-Click Flasher (Recommended) / 一键交互式烧录

- **Windows**: Double-click or run [`flash.bat`](../flash.bat)
- **Linux & macOS**: Run `./flash.sh` (`chmod +x ./flash.sh && ./flash.sh`)
- **Direct Python CLI**:
  ```bash
  python tools/flash_firmware.py --port COMx --bin prebuilt_firmware/cat_detect.bin --erase
  ```

### Option B: Command Line (`esptool`) / 底层命令行烧录

```bash
python -m esptool --chip esp32s3 -p COMx -b 460800 erase-flash
python -m esptool --chip esp32s3 -p COMx -b 921600 write-flash 0x0 prebuilt_firmware/cat_detect.bin
```
