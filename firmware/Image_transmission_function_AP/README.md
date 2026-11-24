# Image_transmission_function_AP (ESP32-S3 / ESP-IDF 6.1)

无线图传 AP 模式 (Image Transmission AP)

## 目录结构
- `main/`: 应用源码（摄像头采集、AI 推理 / 图传、HTTP Server、UART/I2C 通信协议）
- `frontend/gzipped/`: 当前例程专属网页界面资源（`index.html.gz`、`index_ov2640.html.gz`、`index_gc2145.html.gz`、`monitor.html.gz`）
- `partitions.csv`: Flash 分区表
- `sdkconfig.defaults`: ESP-IDF 6.1 默认配置（`esp32s3` + 8MB Flash + OPI PSRAM）
- `../components/`: 公共驱动与组件（由 `EXTRA_COMPONENT_DIRS` 引用）

## 编译与烧录 (ESP-IDF v6.1)
```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

生成一体化合并固件（烧录地址 `0x0`）：
```powershell
idf.py merge-bin -o ..\固件\Image_transmission_function_AP.bin
```
