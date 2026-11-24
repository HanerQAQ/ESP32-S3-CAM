from __future__ import annotations

from PySide6.QtGui import QImage


def bgr_frame_to_qimage(frame) -> QImage:
    if frame is None or len(frame.shape) != 3 or frame.shape[2] != 3:
        raise ValueError("图片格式不受支持。")

    height, width, channels = frame.shape
    bytes_per_line = channels * width
    image = QImage(frame.data, width, height, bytes_per_line, QImage.Format.Format_BGR888)
    return image.copy()
