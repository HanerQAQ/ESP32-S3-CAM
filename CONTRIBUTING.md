# Contributing to ESP32-S3-CAM

**English** | [简体中文](#简体中文)

Thank you for your interest in contributing to the **ESP32-S3-CAM** open-source project! We welcome code contributions, bug reports, documentation improvements, and new vision examples.

---

## Code of Conduct

Please be constructive and respectful in all community interactions. See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

## How to Contribute

1. **Reporting Bugs**: Search existing [Issues](https://github.com/HanerQAQ/ESP32-S3-CAM/issues) first; if unreported, open a **Bug Report** with wiring details, ESP-IDF version, and serial logs.
2. **Pull Requests**:
   - Fork the repository and branch from `main` (`git checkout -b feature/my-feature`).
   - **C/C++ (ESP-IDF & Embedded SDKs)**: Follow Espressif C conventions & `.clang-format` (4-space indentation).
   - **Python (`tools/` & `examples/`)**: Adhere to PEP 8 and verify with `pytest -v` and `python -m compileall examples/ firmware/ tools/`.
   - **Clean Build Artifacts**: Never commit `build/`, `sdkconfig`, `__pycache__/`, `.pyc`, or large binaries (`> 50 MB`). Run `python tools/clean_repo.py` before committing.

---

## 简体中文

感谢您对 **ESP32-S3-CAM** 开源项目的关注与支持！我们欢迎代码贡献、缺陷反馈、文档完善及跨平台示例扩充。

### 行为准则

所有社区交流请遵守 [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)。

### 参与贡献流程

1. **反馈问题**：提交前请先在 [Issues](https://github.com/HanerQAQ/ESP32-S3-CAM/issues) 搜索是否已有记录；提报时请附上硬件接线、ESP-IDF 版本及串口日志。
2. **提交 Pull Request**：
   - Fork 本仓库并从 `main` 创建功能分支（`git checkout -b feature/my-feature`）。
   - **C/C++（ESP-IDF 固件与主控驱动）**：遵循 Espressif 编码规范与 `.clang-format`（4 空格缩进）。
   - **Python（工具与主控 SDK）**：遵循 PEP 8，确保通过 `pytest -v` 与 `python -m compileall examples/ firmware/ tools/`。
   - **仓库整洁**：切勿提交 `build/`、`sdkconfig`、`__pycache__/`、`.pyc` 或超大二进制文件，提交前请运行 `python tools/clean_repo.py`。
