from __future__ import annotations

from collections.abc import Iterable

from vision_control.models import HsvThreshold


def apply_hsv_mask(frame, thresholds: Iterable[HsvThreshold]):
    import cv2
    import numpy as np

    if frame is None:
        raise ValueError("没有可处理的图像。")

    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    mask = np.zeros(frame.shape[:2], dtype=np.uint8)
    for threshold in thresholds:
        lower = np.array([threshold.h_min, threshold.s_min, threshold.v_min], dtype=np.uint8)
        upper = np.array([threshold.h_max, threshold.s_max, threshold.v_max], dtype=np.uint8)
        if threshold.h_min <= threshold.h_max:
            mask = cv2.bitwise_or(mask, cv2.inRange(hsv, lower, upper))
            continue

        lower_a = np.array([0, threshold.s_min, threshold.v_min], dtype=np.uint8)
        upper_a = np.array([threshold.h_max, threshold.s_max, threshold.v_max], dtype=np.uint8)
        lower_b = np.array([threshold.h_min, threshold.s_min, threshold.v_min], dtype=np.uint8)
        upper_b = np.array([179, threshold.s_max, threshold.v_max], dtype=np.uint8)
        mask = cv2.bitwise_or(mask, cv2.inRange(hsv, lower_a, upper_a))
        mask = cv2.bitwise_or(mask, cv2.inRange(hsv, lower_b, upper_b))
    return mask


def mask_to_bgr(mask):
    import cv2

    return cv2.cvtColor(mask, cv2.COLOR_GRAY2BGR)


def estimate_hsv_threshold(frame, rect: tuple[int, int, int, int], margin: int = 12) -> HsvThreshold:
    import cv2

    x, y, width, height = rect
    if width <= 0 or height <= 0:
        raise ValueError("框选区域太小，无法计算 HSV 参数。")

    frame_height, frame_width = frame.shape[:2]
    x1 = max(0, min(frame_width, x))
    y1 = max(0, min(frame_height, y))
    x2 = max(0, min(frame_width, x + width))
    y2 = max(0, min(frame_height, y + height))
    if x2 <= x1 or y2 <= y1:
        raise ValueError("框选区域不在图像范围内。")

    roi = frame[y1:y2, x1:x2]
    hsv = cv2.cvtColor(roi, cv2.COLOR_BGR2HSV)
    h_channel = hsv[:, :, 0].flatten()
    s_channel = hsv[:, :, 1].flatten()
    v_channel = hsv[:, :, 2].flatten()

    s_min = max(0, int(s_channel.min()) - margin)
    s_max = min(255, int(s_channel.max()) + margin)
    v_min = max(0, int(v_channel.min()) - margin)
    v_max = min(255, int(v_channel.max()) + margin)

    # Detect if hue wraps around 0/180 (typical for red)
    spread_normal = int(h_channel.max()) - int(h_channel.min())
    h_shifted = (h_channel.astype(int) + 90) % 180
    spread_shifted = int(h_shifted.max()) - int(h_shifted.min())

    if spread_normal > 100 and spread_shifted < spread_normal:
        # Hue wraps around 0/180: e.g. red
        low_shifted = int(h_shifted.min()) - margin
        high_shifted = int(h_shifted.max()) + margin
        h_min = int((low_shifted - 90) % 180)
        h_max = int((high_shifted - 90) % 180)
    else:
        h_min = max(0, int(h_channel.min()) - margin)
        h_max = min(179, int(h_channel.max()) + margin)

    return HsvThreshold(
        h_min=h_min,
        h_max=h_max,
        s_min=s_min,
        s_max=s_max,
        v_min=v_min,
        v_max=v_max,
    )
