# color_detect (ESP32-S3 / ESP-IDF 6.1)

颜色识别与视觉巡线 (Color Detection & Line Tracking)

## 功能说明
- **Wi-Fi 热点**：`ESP32-S3-CAM-WIFI`（`192.168.4.1`，无密码）
- **Web 图传**：`http://192.168.4.1/`、流地址 `http://192.168.4.1:81/stream`、抓拍 `http://192.168.4.1/capture`
- **UART0 波特率**：`921600` bps（配套上位机 `ColorTool_v1.0.exe` 使用该波特率进行实时取色和阈值下发）
- **I2C 协议 (`0x52`, `SDA=GPIO47`, `SCL=GPIO48`)**：
  - `vision_setting` (`mode=0` 色块追踪 / `mode=1` 巡线，`target` 指定颜色 ID)
  - `get_info color_config` / `get_info color_result`
  - 颜色 ID：`1=Red`, `2=Blue`, `3=Green`, `4=Yellow`, `5=Purple`

## 目录结构
- `main/`: 应用源码（摄像头采集、颜色/巡线算法、调色串口协议、HTTP Server、I2C 从机）
- `frontend/gzipped/`: 网页前端资源
- `partitions.csv`: Flash 分区表
- `sdkconfig.defaults`: ESP-IDF 6.1 默认配置（`esp32s3` + 8MB Flash + OPI PSRAM）
- `../components/`: 公共驱动与组件

## 编译与烧录 (ESP-IDF v6.1)
```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor -b 921600
```

生成一体化合并固件（烧录地址 `0x0`）：
```powershell
idf.py merge-bin -o ..\固件\color_detect.bin
```
