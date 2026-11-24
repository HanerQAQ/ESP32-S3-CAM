from __future__ import annotations

import os
import tempfile
from contextlib import contextmanager
from datetime import datetime
from pathlib import Path

IMAGE_DECODE_DEBUG = False
DEBUG_LOG_PATH = Path.cwd() / "image_decode_debug.log"
DEBUG_JPEG_PATH = Path.cwd() / "image_decode_latest.jpg"


def read_image(path: str):
    try:
        import cv2
        import numpy as np
    except ImportError as exc:
        raise RuntimeError("缺少 OpenCV 依赖，请先安装 requirements.txt 中的 opencv-python。") from exc

    image_path = Path(path)
    if not image_path.is_file():
        raise FileNotFoundError("图片文件不存在。")

    data = np.fromfile(str(image_path), dtype=np.uint8)
    frame = cv2.imdecode(data, cv2.IMREAD_COLOR)
    if frame is None:
        raise ValueError("无法读取该图片文件。")
    return frame


def read_image_bytes(data: bytes):
    try:
        import cv2
        import numpy as np
    except ImportError as exc:
        raise RuntimeError("缺少 OpenCV 依赖，请先安装 requirements.txt 中的 opencv-python。") from exc

    _debug_log(f"device bytes={len(data)}")
    jpeg_data = _extract_jpeg(data)
    _debug_log(f"jpeg bytes={len(jpeg_data)}")
    _debug_write_latest_jpeg(jpeg_data)

    frame, warning_text = _decode_device_jpeg(cv2, np, jpeg_data)
    if warning_text:
        warning = warning_text.strip()
        _debug_log(f"decoder warning={warning}")
        raise ValueError(f"设备返回的 JPEG 图像数据异常：{warning}")
    if frame is None:
        _debug_log("decode failed: frame is None")
        raise ValueError("无法解析设备返回的 JPEG 图像数据。")
    _debug_log(f"decoded shape={frame.shape}")
    return frame


def _extract_jpeg(data: bytes) -> bytes:
    start = data.find(b"\xff\xd8")
    end = data.rfind(b"\xff\xd9")
    _debug_log(f"jpeg markers start={start} end={end}")
    if start < 0 or end < start:
        raise ValueError("设备返回的数据不是 JPEG。")
    return data[start : end + 2]


def _decode_device_jpeg(cv2, np, data: bytes):
    with _captured_stderr() as stderr_file:
        frame = cv2.imdecode(np.frombuffer(data, dtype=np.uint8), cv2.IMREAD_COLOR)
        stderr_file.flush()
        stderr_file.seek(0)
        warning_text = stderr_file.read().decode("utf-8", errors="ignore")
    return frame, warning_text


def _debug_log(message: str) -> None:
    if not IMAGE_DECODE_DEBUG:
        return
    try:
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
        with DEBUG_LOG_PATH.open("a", encoding="utf-8") as file:
            file.write(f"[{timestamp}] {message}\n")
    except OSError:
        return


def _debug_write_latest_jpeg(data: bytes) -> None:
    if not IMAGE_DECODE_DEBUG:
        return
    try:
        DEBUG_JPEG_PATH.write_bytes(data)
        _debug_log(f"saved latest jpeg={DEBUG_JPEG_PATH}")
    except OSError as exc:
        _debug_log(f"save latest jpeg failed={exc}")


@contextmanager
def _captured_stderr():
    original_stderr_fd = os.dup(2)
    with tempfile.TemporaryFile() as stderr_file:
        try:
            os.dup2(stderr_file.fileno(), 2)
            yield stderr_file
        finally:
            os.dup2(original_stderr_fd, 2)
            os.close(original_stderr_fd)
