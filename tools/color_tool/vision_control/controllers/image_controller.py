from __future__ import annotations

import cv2
from PySide6.QtCore import QObject, Signal
from PySide6.QtGui import QImage

from vision_control.models import ColorConfig, HsvThreshold
from vision_control.vision.hsv_processor import apply_hsv_mask, estimate_hsv_threshold, mask_to_bgr
from vision_control.vision.image_convert import bgr_frame_to_qimage
from vision_control.vision.image_loader import read_image, read_image_bytes


class ImageController(QObject):
    preview_image_changed = Signal(QImage)
    original_image_changed = Signal(QImage)
    hsv_image_changed = Signal(QImage)
    mask_mode_changed = Signal(bool)
    mask_available_changed = Signal(bool)
    selection_mode_changed = Signal(bool)
    threshold_picked = Signal(str, HsvThreshold)
    error_occurred = Signal(str)
    device_image_error_occurred = Signal(str)
    pick_color_error_occurred = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._frame = None
        self._mask_enabled = False
        self._active_color_name = ""
        self._color_configs = ()
        self._pick_color_name = ""

    def handle_local_image_selected(self, path: str) -> None:
        try:
            frame = read_image(path)
            original, hsv_preview = self._preview_images_for_frame(frame)
        except Exception as exc:
            self.error_occurred.emit(str(exc))
            return
        self._frame = frame
        self.mask_available_changed.emit(self._is_mask_available())
        self._emit_preview_images(original, hsv_preview)

    def handle_device_image_received(self, data: bytes) -> None:
        try:
            frame = read_image_bytes(data)
            original, hsv_preview = self._preview_images_for_frame(frame)
        except Exception as exc:
            self.device_image_error_occurred.emit(str(exc))
            return
        self._frame = frame
        self.mask_available_changed.emit(self._is_mask_available())
        self._emit_preview_images(original, hsv_preview)

    def handle_mask_mode_changed(self, enabled: bool) -> None:
        if enabled and not self._is_mask_available():
            self._mask_enabled = False
            self.mask_mode_changed.emit(False)
            return
        self._mask_enabled = enabled
        self._refresh_preview()

    def handle_color_configs_changed(self, configs: tuple[ColorConfig, ...]) -> None:
        self._color_configs = configs
        mask_available = self._is_mask_available()
        self.mask_available_changed.emit(mask_available)
        if not mask_available and self._mask_enabled:
            self._mask_enabled = False
            self.mask_mode_changed.emit(False)
        self._refresh_preview()

    def handle_active_threshold_changed(self, color_name: str, configs: tuple[ColorConfig, ...]) -> None:
        self._active_color_name = color_name
        self._color_configs = configs
        if not self._is_mask_available():
            self._mask_enabled = False
            self.mask_mode_changed.emit(False)
            self.mask_available_changed.emit(False)
            return
        self._mask_enabled = True
        self.mask_mode_changed.emit(True)
        self._refresh_preview()

    def handle_pick_color_requested(self, color_name: str) -> None:
        if self._frame is None:
            self.pick_color_error_occurred.emit("请先打开或获取一张图像。")
            return
        self._pick_color_name = color_name
        self.selection_mode_changed.emit(True)

    def handle_selection_finished(self, rect: tuple[int, int, int, int]) -> None:
        if not self._pick_color_name:
            return
        if self._frame is None:
            self.pick_color_error_occurred.emit("请先打开或获取一张图像。")
            return
        try:
            threshold = estimate_hsv_threshold(self._frame, rect)
        except Exception as exc:
            self.pick_color_error_occurred.emit(str(exc))
            return
        color_name = self._pick_color_name
        self._pick_color_name = ""
        self.selection_mode_changed.emit(False)
        self.threshold_picked.emit(color_name, threshold)

    def handle_selection_cancelled(self) -> None:
        if not self._pick_color_name:
            return
        self._pick_color_name = ""
        self.selection_mode_changed.emit(False)

    def _refresh_preview(self) -> None:
        if self._frame is None:
            return
        try:
            original, hsv_preview = self._preview_images_for_frame(self._frame)
        except Exception as exc:
            self.error_occurred.emit(str(exc))
            return
        self._emit_preview_images(original, hsv_preview)

    def _preview_images_for_frame(self, frame) -> tuple[QImage, QImage]:
        original = bgr_frame_to_qimage(frame)
        hsv_preview = bgr_frame_to_qimage(self._hsv_preview_frame(frame))
        return original, hsv_preview

    def _emit_preview_images(self, original: QImage, hsv_preview: QImage) -> None:
        self.original_image_changed.emit(original)
        self.hsv_image_changed.emit(hsv_preview)
        self.preview_image_changed.emit(original)

    def _processed_frame(self):
        if not self._mask_enabled:
            return self._frame
        thresholds = self._visible_thresholds()
        if not thresholds:
            return self._frame
        return mask_to_bgr(apply_hsv_mask(self._frame, thresholds))

    def _hsv_preview_frame(self, frame=None):
        source_frame = self._frame if frame is None else frame
        thresholds = self._visible_thresholds()
        if thresholds:
            return mask_to_bgr(apply_hsv_mask(source_frame, thresholds))
        hsv = cv2.cvtColor(source_frame, cv2.COLOR_BGR2HSV)
        return cv2.cvtColor(hsv, cv2.COLOR_HSV2BGR)

    def _visible_thresholds(self) -> tuple[HsvThreshold, ...]:
        return tuple(config.threshold for config in self._color_configs if config.enabled)

    def _has_visible_config(self) -> bool:
        return any(config.enabled for config in self._color_configs)

    def _is_mask_available(self) -> bool:
        return self._frame is not None and self._has_visible_config()
