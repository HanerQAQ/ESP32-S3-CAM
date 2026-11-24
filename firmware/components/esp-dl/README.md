# ESP-DL (ESP32-S3 Tailored) [[中文]](./README_cn.md)

ESP-DL is Espressif's high-performance deep learning library for ESP32 series SoCs.

> **Component Note**: This folder packages the curated **ESP32-S3** architecture precompiled libraries (`lib/esp32s3/libdl.a` and `lib/esp32s3/libmfn.a`) and header files (`include/`) required by the vision projects in this repository (`cat_detect`, `human_face_detect`, `human_face_recognition`). For the full multi-chip upstream repository, please visit [espressif/esp-dl](https://github.com/espressif/esp-dl).

---

## Overview

ESP-DL provides APIs for **Neural Network (NN) Inference**, **Image Processing**, **Math Operations** and **Deep Learning Models** (such as Face Detection, Face Recognition, Cat/Dog Detection).

- Official Documentation: [ESP-DL User Guide](https://docs.espressif.com/projects/esp-dl/en/latest/esp32s3/index.html)
- Upstream Examples: [ESP-DL Examples on GitHub](https://github.com/espressif/esp-dl/tree/master/examples)
- Upstream TVM Tutorial: [ESP-DL TVM Example on GitHub](https://github.com/espressif/esp-dl/tree/master/tutorial/tvm_example)

## Pre-integrated Vision Models in this Repository

| Vision Model | Firmware Project in this Repo | Upstream Reference |
| :--- | :--- | :--- |
| **Human Face Detection** | [`firmware/human_face_detect`](../../human_face_detect) | [human_face_detect](https://github.com/espressif/esp-dl/tree/master/examples/human_face_detect) |
| **Human Face Recognition** | [`firmware/human_face_recognition`](../../human_face_recognition) | [face_recognition](https://github.com/espressif/esp-dl/tree/master/examples/face_recognition) |
| **Cat / Dog Detection** | [`firmware/cat_detect`](../../cat_detect) | [cat_face_detect](https://github.com/espressif/esp-dl/tree/master/examples/cat_face_detect) |

## Feedback & Upstream Issues

For upstream ESP-DL questions, submit an issue to [espressif/esp-dl/issues](https://github.com/espressif/esp-dl/issues).
