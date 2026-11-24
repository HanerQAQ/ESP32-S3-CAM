# human_face_recognition (ESP32-S3 / ESP-IDF 6.1)

人脸识别 (Human Face Recognition)

## 功能说明
- **Wi-Fi 热点**：`ESP32-S3-CAM-WIFI`（`192.168.4.1`，无密码）
- **Web 图传**：`http://192.168.4.1/`、流地址 `http://192.168.4.1:81/stream`、抓拍 `http://192.168.4.1/capture`
- **掉电存储分区**：`partitions.csv` 内 `fr` 分区（Type `32`, SubType `32`, `128KB`）用于掉电保存已录入人脸特征向量
- **I2C 协议 (`0x52`, `SDA=GPIO47`, `SCL=GPIO48`)**：
  - `get_info face_recognition` / `get_info face_count`
  - `face_register` / `face_delete`（`params=ID` 删除指定人脸，`params=0` 清空全部人脸）
  - 注意：`face_register` 与 `face_delete` 需等待神经网络帧推理及 Flash 操作完成（实测约 `100~200 ms`），主机端需延时或轮询等待应答帧。

## 目录结构
- `main/`: 应用源码（人脸检测与特征识别、HTTP Server、UART/I2C 从机）
- `frontend/gzipped/`: 网页前端资源
- `partitions.csv`: Flash 分区表（含 `fr` 人脸特征分区）
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
idf.py merge-bin -o ..\固件\human_face_recognition.bin
```
