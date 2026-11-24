# ESP-DL (ESP32-S3 专用精简版) [[English]](./README.md)

ESP-DL 是乐鑫专为 ESP32 系列芯片开发的高性能深度学习资源库。

> **组件说明**：本目录精简保留了供本仓库视觉项目（`cat_detect`、`human_face_detect`、`human_face_recognition`）编译所需的 **ESP32-S3** 架构预编译静态库（`lib/esp32s3/libdl.a` 与 `lib/esp32s3/libmfn.a`）及完整头文件（`include/`）。如需获取包含全平台多芯片的完整上游仓库，请访问 [espressif/esp-dl](https://github.com/espressif/esp-dl)。

---

## 概述

ESP-DL 提供针对 **神经网络 (NN) 推理**、**图像处理**、**数学运算** 和 **深度学习模型**（人脸检测、人脸识别、猫狗检测等）的 C/C++ API。

- 官方文档：[ESP-DL 用户指南](https://docs.espressif.com/projects/esp-dl/zh-CN/latest/esp32s3/index.html)
- 上游模型示例：[ESP-DL 示例工程 (GitHub)](https://github.com/espressif/esp-dl/tree/master/examples)
- 上游 TVM 教程：[ESP-DL TVM 教程 (GitHub)](https://github.com/espressif/esp-dl/tree/master/tutorial/tvm_example)

## 本仓库中集成的视觉工程对应

| 视觉模型 | 本仓库对应固件工程 | 上游对应参考 |
| :--- | :--- | :--- |
| **人脸检测** | [`firmware/human_face_detect`](../../human_face_detect) | [human_face_detect](https://github.com/espressif/esp-dl/tree/master/examples/human_face_detect) |
| **人脸识别** | [`firmware/human_face_recognition`](../../human_face_recognition) | [face_recognition](https://github.com/espressif/esp-dl/tree/master/examples/face_recognition) |
| **猫狗检测** | [`firmware/cat_detect`](../../cat_detect) | [cat_face_detect](https://github.com/espressif/esp-dl/tree/master/examples/cat_face_detect) |

## 上游问题反馈

如遇 ESP-DL 底层模型算法相关问题，请前往 [espressif/esp-dl/issues](https://github.com/espressif/esp-dl/issues) 提交反馈。
