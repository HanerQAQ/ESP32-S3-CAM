# Image_transmission_function_STA (ESP32-S3 / ESP-IDF 6.1)

无线图传 STA+AP 配网模式 (Image Transmission STA)

## 功能说明
- **默认配网热点**：`ESP32-S3-CAM-XXXXXX`（`192.168.5.1`，无密码，支持 Captive Portal）
- **Web 页面与接口**：配网页面 `http://192.168.5.1/wifi`、图传页面 `/cam`（或路由器分配 STA IP `/`）、流地址 `:81/stream`、抓拍 `/capture`、`/api/wifi/saved`
- **按键与串口 CLI (`115200` bps)**：
  - `BOOT (GPIO0)` 短按打印状态，长按 1 秒重新触发配网热点
  - 串口支持 `status`、`ap_ip`、`sta_ip`、`wifi_scan`、`wifi_set <ssid> <pwd>`、`wifi_clear`、`reset` 等指令
- **I2C 控制 (`0x52`, `SDA=GPIO47`, `SCL=GPIO48`)**：支持 `vision_setting`（`hmirror` / `vflip`）及 `module_reset`

## 目录结构
- `main/`: 应用源码（Wi-Fi STA+AP 配网管理、HTTP Server、UART0 CLI、I2C 从机）
- `frontend/gzipped/`: 网页前端资源
- `partitions.csv`: Flash 分区表
- `sdkconfig.defaults`: ESP-IDF 6.1 默认配置（`esp32s3` + 8MB Flash + OPI PSRAM）
- `../components/`: 公共驱动与组件

## 编译与烧录 (ESP-IDF v6.1)
```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

生成一体化合并固件（烧录地址 `0x0`）：
```powershell
idf.py merge-bin -o ..\固件\Image_transmission_function_STA.bin
```
