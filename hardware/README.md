# Hardware Documents & Pinout / 硬件资料与引脚说明

This folder contains lightweight PDF technical references for the **ESP32-S3-CAM V1.0** module:

本目录包含 **ESP32-S3-CAM V1.0** 模块的电路原理图与核心芯片数据手册：

- [`SCH_S3-Cam_V1.0.pdf`](SCH_S3-Cam_V1.0.pdf) — Complete module schematic (power, USB-UART CH340, camera FPC, I2C header & ESP32-S3 pin routing) / 模块完整电路原理图
- [`ESP32-S3_Datasheet.pdf`](ESP32-S3_Datasheet.pdf) — Espressif ESP32-S3 SoC Datasheet / 乐鑫 ESP32-S3 芯片规格书
- [`GC2145_Datasheet.pdf`](GC2145_Datasheet.pdf) — GalaxyCore GC2145 2MP CMOS Image Sensor Datasheet / 格科微 GC2145 摄像头传感器规格书
- [Hardware Assembly & Lens Focus Guide (English)](../docs/hardware_guide_en.md) | [硬件装配与镜头调焦指南（中文）](../docs/hardware_guide_zh.md)

---

## Module Interface Overview / 模块接口概览

<div align="center">
  <img src="../docs/assets/pinout.png" alt="ESP32-S3 CAM Hardware Interface" width="420"/>
</div>

| Interface / Pin | ESP32-S3 GPIO | Description / 说明 |
| :--- | :---: | :--- |
| **I2C Slave SDA** | `GPIO47` | Shared on 4-pin `5264-4P` IIC connector & `2×8P` headers |
| **I2C Slave SCL** | `GPIO48` | Shared on 4-pin `5264-4P` IIC connector & `2×8P` headers |
| **USB-UART (CH340)** | `GPIO43` (TXD0), `GPIO44` (RXD0) | Firmware flashing, serial logs & ColorTool calibration |
| **Camera SCCB** | `GPIO4` (SDA), `GPIO5` (SCL) | Internal camera sensor bus |
| **Onboard Blue LED** | `GPIO2` | Active low status LED |
| **BOOT / User Button** | `GPIO0` | Download mode on reset; in STA firmware: short press = status, 1s hold = AP provisioning |
| **Power Supply** | `5V`, `GND` | Recommended `DC 5V >= 2A` |

---

## Mechanical Dimensions & Physical Specifications / 结构尺寸与物理规格

<div align="center">
  <img src="mechanical_dimensions.svg" alt="ESP32-S3-CAM Mechanical Dimensions" width="840"/>
</div>

| Parameter / 规格参数 | Value / 数值 | Notes / 备注 |
| :--- | :--- | :--- |
| **Overall Assembled / 整机堆叠尺寸** | `27.0 mm × 42.0 mm × 19.0 mm` (`W × H × D`) | Core + Carrier stacked (incl. `5264-4P` IIC & lens) / 含底板、镜头与 IIC 端子 |
| **Core & Carrier PCB / 单板 PCB 尺寸** | `27.0 mm × 40.0 mm` | PCB thickness: `1.6 mm` each / 双板厚各 `1.6 mm` |
| **Mounting Holes / 底板安装定位孔** | `2 × ⌀ 4.00 mm` | Horizontal pitch `8.00 mm` in lower Carrier Back / 底板背面下部横向双孔，孔距 `8.00 mm` |
| **Board-to-Board Package / 板间封装排针** | `DIP-16` (`2 × 8P`, `2.54 mm` pitch) | Row-to-row spacing `22.86 mm` (`0.9"`) / 双排间距 `22.86 mm` |
| **Camera Optics / 摄像头参数** | `GC2145 2MP` (`FOV 123°`) | `24-Pin 0.5 mm` FPC (DVP interface) |
| **I2C Connector / I2C 通讯端子** | `4-Pin 5264-4P` | Keyed 4-pin header (`5V`, `GND`, `SCL=IO48`, `SDA=IO47`) |
| **USB Interface / USB 接口** | USB Type-C Receptacle | Onboard CH340 USB-UART bridge |
| **Weight / 整机重量** | `~14 g` | Stacked Core + Carrier module / 含核心板与底板 |
| **Operating Voltage / 工作电压** | `DC 5.0 V` (`4.75 ~ 5.25 V`) | Recommended supply current `≥ 2.0 A` |
| **Operating Temperature / 工作温度** | `-20 °C ~ +70 °C` | Storage: `-40 °C ~ +125 °C` (`< 90% RH`) |

> **3D CAD Models (`.stp` / `.stl`)**: To avoid bloating Git history (~69 MB total), raw 3D STEP / STL mechanical CAD models are omitted here and can be attached to **GitHub Releases**.
