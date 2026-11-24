# Developer Tools & Utilities / 开发者工具与调试套件

**English** | [简体中文](#简体中文)

This directory provides developer utilities for maintaining, diagnosing, calibrating, and flashing the **ESP32-S3-CAM** edge AI vision platform:

| Tool | Purpose | Usage Example |
| :--- | :--- | :--- |
| `flash_firmware.py` | Auto-detect serial COM ports and flash prebuilt merged binaries to ESP32-S3 at 0x0 | `python tools/flash_firmware.py` |
| `i2c_diagnose.py` | Probe I²C connectivity, measure round-trip latency, verify frame CRC/framing | `python tools/i2c_diagnose.py --mock` |
| `clean_repo.py` | Scrub Python bytecode, `.pytest_cache`, and temporary build artifacts | `python tools/clean_repo.py` |
| `color_tool/` | PySide6 HSV threshold calibration GUI with preset management | `python tools/color_tool/main.py` |

### Detailed Usage

#### 1. Firmware Flashing Tool (`flash_firmware.py`)
Automatically detects connected CH340 / CP210x / USB Serial JTAG adapters, lists prebuilt merged binaries, and writes flash at `0x0`:
```bash
# Interactive menu:
python tools/flash_firmware.py

# Non-interactive CLI:
python tools/flash_firmware.py --port COM3 --bin cat_detect.bin --erase

# Dry-run validation (headless CI / preview):
python tools/flash_firmware.py --dry-run
```

#### 2. I²C Diagnostic Utility (`i2c_diagnose.py`)
Validates high-speed binary protocol frame integrity and measures packet ping-pong latency:
```bash
# Simulated mock test suite (no hardware needed):
python tools/i2c_diagnose.py --mock

# Hardware live probe on Linux / Jetson / Raspberry Pi:
python tools/i2c_diagnose.py --bus 7 --addr 0x52
```

#### 3. Repository Cleaner (`clean_repo.py`)
Removes `.pyc`, `__pycache__`, `.pytest_cache`, and temporary build directories:
```bash
python tools/clean_repo.py
python tools/clean_repo.py --dry-run
```

#### 4. Color Tool (`color_tool/`)
PySide6 desktop GUI for tuning HSV color detection and line tracking thresholds with JSON preset import/export.

---

## 简体中文

本目录包含 **ESP32-S3-CAM** 边缘 AI 视觉模组的开发辅助工具、调优上位机与诊断脚本：

| 工具 | 用途说明 | 常用命令 |
| :--- | :--- | :--- |
| `flash_firmware.py` | 自动识别串口并将官方预编译合包固件烧录至 ESP32-S3 (0x0 地址) | `python tools/flash_firmware.py` |
| `i2c_diagnose.py` | I²C 连通性测试、往返延迟测量与 32 字节帧校验诊断 | `python tools/i2c_diagnose.py --mock` |
| `clean_repo.py` | 仓库缓存深度清理工具（清除 `__pycache__`、`.pytest_cache`、临时文件） | `python tools/clean_repo.py` |
| `color_tool/` | 基于 PySide6 + OpenCV 的颜色识别与巡线 HSV 阈值上位机调参软件 | `python tools/color_tool/main.py` |

### 详细使用说明

#### 1. 固件一键烧录工具 (`flash_firmware.py`)
自动扫描系统中插入的 USB 转串口设备，列出已打包的固件列表，并调用 `esptool` 进行整片擦除与烧录：
```bash
# 交互式引导菜单：
python tools/flash_firmware.py

# 命令行直接指定串口与固件：
python tools/flash_firmware.py --port COM3 --bin cat_detect.bin --erase

# 预演模式（无硬件环境或 CI 测试）：
python tools/flash_firmware.py --dry-run
```

#### 2. I²C 通信诊断脚本 (`i2c_diagnose.py`)
验证 32 字节分片传输帧头（`0xAA 0x55`）和 XOR 异或校验和完整性，并统计通信往返耗时：
```bash
# 软件模拟测试（无需真实硬件）：
python tools/i2c_diagnose.py --mock

# 树莓派 / Jetson Orin 硬件实测：
python tools/i2c_diagnose.py --bus 7 --addr 0x52
```

#### 3. 仓库代码卫生清理工具 (`clean_repo.py`)
自动递归清理所有编译产物、Python 字节码缓存及测试临时目录：
```bash
python tools/clean_repo.py
python tools/clean_repo.py --dry-run
```

#### 4. 颜色与巡线调优上位机 (`color_tool/`)
基于 PySide6 构建的桌面调参软件，支持实时 HSV 阈值滑动调节、掩码二值化预览以及 JSON 预设快速导入导出。
