from __future__ import annotations

from PySide6.QtCore import QPoint, QPointF, QRect, QRectF, QSize, QTimer, Qt, Signal
from PySide6.QtGui import QColor, QFont, QImage, QPainter, QPainterPath, QPen, QPixmap
from PySide6.QtWidgets import QFrame, QLabel, QPushButton, QSizePolicy, QWidget

from vision_control.widgets.common import LineIcon, hbox, make_icon, panel, vbox


def tr(text: str) -> str:
    try:
        return text.encode("ascii").decode("unicode_escape")
    except Exception:
        return text.replace("\\", "")


class PreviewCanvas(QFrame):
    selection_finished = Signal(tuple)
    selection_cancelled = Signal()

    def __init__(self, empty_text: str, selectable: bool = False, parent=None):
        super().__init__(parent)
        self._empty_text = empty_text
        self._language = "zh"
        self._empty_kind = "hsv" if "HSV" in empty_text else "original"
        self._pixmap = QPixmap()
        self._selection_enabled = False
        self._selectable = selectable
        self._hover_pos = None
        self._pick_radius = 10
        self._pulse = 0
        self._loading = False
        self._loading_step = 0
        self._pulse_timer = QTimer(self)
        self._pulse_timer.setInterval(60)
        self._pulse_timer.timeout.connect(self._advance_pulse)
        self._loading_timer = QTimer(self)
        self._loading_timer.setInterval(60)
        self._loading_timer.timeout.connect(self._advance_loading)
        self.setMinimumSize(360, 300)
        self.setProperty("previewCanvas", True)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        self.setMouseTracking(True)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)

    def set_preview_image(self, image: QImage) -> None:
        pixmap = QPixmap.fromImage(image)
        if pixmap.isNull():
            return
        self._pixmap = pixmap
        self.update()

    def set_loading(self, loading: bool) -> None:
        self._loading = loading
        if loading:
            self._loading_step = 0
            if not self._loading_timer.isActive():
                self._loading_timer.start()
        else:
            self._loading_timer.stop()
        self.update()

    def set_language(self, language: str) -> None:
        self._language = language
        if self._empty_kind == "hsv":
            self._empty_text = "No HSV Image" if language == "en" else tr("\暂\无 HSV \图\像")
        else:
            self._empty_text = "No Original Image" if language == "en" else tr("\暂\无\原\图")
        self.update()

    def set_selection_enabled(self, enabled: bool) -> None:
        self._selection_enabled = enabled and self._selectable
        self.setCursor(Qt.CursorShape.CrossCursor if self._selection_enabled else Qt.CursorShape.ArrowCursor)
        if self._selection_enabled:
            self._pulse_timer.start()
            self.setFocus(Qt.FocusReason.OtherFocusReason)
        else:
            self._pulse_timer.stop()
        if not self._selection_enabled:
            self._hover_pos = None
            self.update()

    def mousePressEvent(self, event) -> None:
        if self._selection_enabled and event.button() == Qt.MouseButton.LeftButton and not self._pixmap.isNull():
            image_rect = self._point_to_image_rect(event.pos())
            self.set_selection_enabled(False)
            if image_rect is not None:
                self.selection_finished.emit(image_rect)
            return
        super().mousePressEvent(event)

    def mouseMoveEvent(self, event) -> None:
        if self._selection_enabled and not self._pixmap.isNull():
            self._hover_pos = event.pos()
            self.update()
            return
        super().mouseMoveEvent(event)

    def wheelEvent(self, event) -> None:
        if self._selection_enabled:
            delta = 2 if event.angleDelta().y() > 0 else -2
            self._pick_radius = max(2, min(60, self._pick_radius + delta))
            self.update()
            event.accept()
            return
        super().wheelEvent(event)

    def leaveEvent(self, event) -> None:
        if self._selection_enabled:
            self._hover_pos = None
            self.update()
        super().leaveEvent(event)

    def keyPressEvent(self, event) -> None:
        if self._selection_enabled and event.key() == Qt.Key.Key_Escape:
            self.set_selection_enabled(False)
            self.selection_cancelled.emit()
            event.accept()
            return
        super().keyPressEvent(event)

    def paintEvent(self, event) -> None:
        super().paintEvent(event)
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        if not self._pixmap.isNull():
            target = self._scaled_pixmap()
            x = (self.width() - target.width()) // 2
            y = (self.height() - target.height()) // 2
            clip_path = QPainterPath()
            clip_path.addRoundedRect(self.rect(), 10, 10)
            painter.setClipPath(clip_path)
            painter.drawPixmap(x, y, target)
            self._draw_corner_badge(painter)
            if self._selection_enabled and self._hover_pos is not None:
                self._draw_pick_cursor(painter)
            if self._loading:
                self._draw_loading_state(painter)
            return

        self._draw_empty_state(painter)
        if self._loading:
            self._draw_loading_state(painter)

    def _draw_corner_badge(self, painter: QPainter) -> None:
        if self._pixmap.isNull():
            return
        painter.save()
        tag = "RAW SENSOR" if self._empty_kind == "original" else "HSV MASK"
        painter.setFont(QFont("Arial", 9, QFont.Weight.Bold))
        painter.setPen(QPen(QColor(240, 246, 252, 140), 1))
        painter.setBrush(QColor(18, 23, 34, 180))
        painter.drawRoundedRect(QRectF(12, 12, 86, 20), 4, 4)
        painter.setPen(QColor(240, 246, 252, 210))
        painter.drawText(QRectF(12, 12, 86, 20), Qt.AlignmentFlag.AlignCenter, tag)
        painter.restore()

    def _scaled_pixmap(self) -> QPixmap:
        return self._pixmap.scaled(
            self.size(),
            Qt.AspectRatioMode.KeepAspectRatioByExpanding,
            Qt.TransformationMode.SmoothTransformation,
        )

    def _point_to_image_rect(self, point: QPoint) -> tuple[int, int, int, int] | None:
        if self._pixmap.isNull():
            return None
        target = self._scaled_pixmap()
        target_rect = QRect(
            (self.width() - target.width()) // 2,
            (self.height() - target.height()) // 2,
            target.width(),
            target.height(),
        )
        if not target_rect.contains(point):
            return None
        selection = QRect(
            point.x() - self._pick_radius,
            point.y() - self._pick_radius,
            self._pick_radius * 2,
            self._pick_radius * 2,
        )
        clipped = selection.intersected(target_rect)
        if clipped.width() < 4 or clipped.height() < 4:
            return None
        scale_x = self._pixmap.width() / target.width()
        scale_y = self._pixmap.height() / target.height()
        return (
            int((clipped.x() - target_rect.x()) * scale_x),
            int((clipped.y() - target_rect.y()) * scale_y),
            int(clipped.width() * scale_x),
            int(clipped.height() * scale_y),
        )

    def _draw_pick_cursor(self, painter: QPainter) -> None:
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        pos = self._hover_pos
        radius = self._pick_radius

        # Reticle outer dashed circle
        pen = QPen(QColor("#ff7a00"), 1.8, Qt.PenStyle.DashLine)
        painter.setPen(pen)
        painter.setBrush(QColor(255, 122, 0, 30))
        painter.drawEllipse(pos, radius, radius)

        # Pulse halo
        pulse_color = QColor(255, 122, 0, max(15, 120 - self._pulse * 10))
        painter.setPen(QPen(pulse_color, 1.2))
        painter.setBrush(Qt.BrushStyle.NoBrush)
        painter.drawEllipse(pos, radius + self._pulse, radius + self._pulse)

        # Crosshair lines
        painter.setPen(QPen(QColor("#ff7a00"), 1.5))
        d = 6
        painter.drawLine(pos.x() - radius - d, pos.y(), pos.x() - radius + 2, pos.y())
        painter.drawLine(pos.x() + radius - 2, pos.y(), pos.x() + radius + d, pos.y())
        painter.drawLine(pos.x(), pos.y() - radius - d, pos.x(), pos.y() - radius + 2)
        painter.drawLine(pos.x(), pos.y() + radius - 2, pos.x(), pos.y() + radius + d)

        # Center dot
        painter.setPen(Qt.PenStyle.NoPen)
        painter.setBrush(QColor("#ffffff"))
        painter.drawEllipse(pos, 2, 2)

        # Radius HUD badge
        hud_text = f"R: {radius}px"
        painter.setFont(QFont("Arial", 9, QFont.Weight.Bold))
        hud_w, hud_h = 56, 20
        hud_x = pos.x() + radius + 10
        hud_y = pos.y() - 10
        if hud_x + hud_w > self.width():
            hud_x = pos.x() - radius - hud_w - 10
        painter.setBrush(QColor(13, 17, 23, 210))
        painter.setPen(QPen(QColor("#ff7a00"), 1))
        painter.drawRoundedRect(QRectF(hud_x, hud_y, hud_w, hud_h), 4, 4)
        painter.setPen(QColor("#ffffff"))
        painter.drawText(QRectF(hud_x, hud_y, hud_w, hud_h), Qt.AlignmentFlag.AlignCenter, hud_text)

    def _draw_empty_state(self, painter: QPainter) -> None:
        center_x = self.width() / 2
        block_y = self.height() / 2 - 35

        self._draw_robot_icon(painter, center_x, block_y)

        # Title
        title_font = painter.font()
        title_font.setPointSize(16)
        title_font.setBold(True)
        painter.setFont(title_font)
        painter.setPen(QColor("#f0f6fc"))
        title_rect = QRectF(0, block_y + 36, self.width(), 26)
        painter.drawText(title_rect, Qt.AlignmentFlag.AlignHCenter | Qt.AlignmentFlag.AlignVCenter, self._empty_text)

        # Subtitle / Hint
        hint_font = painter.font()
        hint_font.setPointSize(11)
        hint_font.setBold(False)
        painter.setFont(hint_font)
        painter.setPen(QColor("#8b97b0"))
        hint_rect = QRectF(24, block_y + 68, self.width() - 48, 22)
        painter.drawText(hint_rect, Qt.AlignmentFlag.AlignHCenter | Qt.AlignmentFlag.AlignVCenter, self._empty_hint())

    def _draw_robot_icon(self, painter: QPainter, cx: float, cy: float) -> None:
        painter.save()
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        # Robot outline & feature colors
        robot_color = QColor("#72829d")
        feature_color = QColor("#1c2236")

        # 1. Top antenna
        painter.setPen(QPen(robot_color, 2.5, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap))
        painter.drawLine(QPointF(cx, cy - 26), QPointF(cx, cy - 18))
        painter.setPen(Qt.PenStyle.NoPen)
        painter.setBrush(robot_color)
        painter.drawEllipse(QPointF(cx, cy - 28), 3.5, 3.5)

        # 2. Left / Right ears
        painter.drawRoundedRect(QRectF(cx - 28, cy - 8, 5, 12), 2.5, 2.5)
        painter.drawRoundedRect(QRectF(cx + 23, cy - 8, 5, 12), 2.5, 2.5)

        # 3. Head box
        head_rect = QRectF(cx - 23, cy - 18, 46, 34)
        painter.drawRoundedRect(head_rect, 8, 8)

        # 4. Eyes
        painter.setBrush(feature_color)
        painter.drawRoundedRect(QRectF(cx - 15, cy - 9, 8, 9), 2.5, 2.5)
        painter.drawRoundedRect(QRectF(cx + 7, cy - 9, 8, 9), 2.5, 2.5)

        # 5. Mouth (slight gentle inverted curve)
        painter.setPen(QPen(feature_color, 2.4, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap))
        painter.setBrush(Qt.BrushStyle.NoBrush)
        mouth_path = QPainterPath()
        mouth_path.moveTo(cx - 9, cy + 8)
        mouth_path.quadTo(cx, cy + 4, cx + 9, cy + 8)
        painter.drawPath(mouth_path)

        painter.restore()

    def _draw_sensor_icon(self, painter: QPainter, cx: float, cy: float) -> None:
        painter.save()
        # Viewfinder bounds
        pen = QPen(QColor(255, 122, 0, 180), 2, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap)
        painter.setPen(pen)
        painter.setBrush(Qt.BrushStyle.NoBrush)

        d = 20
        # 4 corners
        painter.drawLine(QPointF(cx - d, cy - d + 8), QPointF(cx - d, cy - d))
        painter.drawLine(QPointF(cx - d, cy - d), QPointF(cx - d + 8, cy - d))

        painter.drawLine(QPointF(cx + d - 8, cy - d), QPointF(cx + d, cy - d))
        painter.drawLine(QPointF(cx + d, cy - d), QPointF(cx + d, cy - d + 8))

        painter.drawLine(QPointF(cx - d, cy + d - 8), QPointF(cx - d, cy + d))
        painter.drawLine(QPointF(cx - d, cy + d), QPointF(cx - d + 8, cy + d))

        painter.drawLine(QPointF(cx + d - 8, cy + d), QPointF(cx + d, cy + d))
        painter.drawLine(QPointF(cx + d, cy + d), QPointF(cx + d, cy + d - 8))

        # Center aperture circle
        painter.setPen(QPen(QColor(255, 122, 0, 120), 1.5))
        painter.drawEllipse(QPointF(cx, cy), 11, 11)
        painter.setBrush(QColor(255, 122, 0, 60))
        painter.drawEllipse(QPointF(cx, cy), 4, 4)
        painter.restore()

    def _draw_spectrum_icon(self, painter: QPainter, cx: float, cy: float) -> None:
        painter.save()
        # 3 overlapping color rings (HSV filter mask icon)
        painter.setPen(Qt.PenStyle.NoPen)
        painter.setBrush(QColor(239, 35, 60, 140))  # Red
        painter.drawEllipse(QPointF(cx - 7, cy - 5), 11, 11)

        painter.setBrush(QColor(18, 183, 106, 140))  # Green
        painter.drawEllipse(QPointF(cx + 7, cy - 5), 11, 11)

        painter.setBrush(QColor(47, 128, 237, 140))  # Blue
        painter.drawEllipse(QPointF(cx, cy + 7), 11, 11)

        # Center highlight
        painter.setBrush(QColor(255, 255, 255, 180))
        painter.drawEllipse(QPointF(cx, cy), 3.5, 3.5)
        painter.restore()

    def _draw_loading_state(self, painter: QPainter) -> None:
        painter.save()
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.setPen(Qt.PenStyle.NoPen)
        painter.setBrush(QColor(24, 31, 48, 225))
        painter.drawRoundedRect(self.rect(), 10, 10)

        center = QPoint(self.width() // 2, self.height() // 2 - 10)
        radius = 28
        rect = QRectF(center.x() - radius, center.y() - radius, radius * 2, radius * 2)
        start_angle = (90 - self._loading_step * 18) * 16
        painter.setPen(QPen(QColor("#ff7a00"), 4.0, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap))
        painter.setBrush(Qt.BrushStyle.NoBrush)
        painter.drawArc(rect, start_angle, -4000)

        label_font = painter.font()
        label_font.setPointSize(12)
        label_font.setBold(True)
        painter.setFont(label_font)
        painter.setPen(QColor("#ffffff"))
        label = "Loading Frame..." if self._language == "en" else tr("\图\像\加\载\中...")
        painter.drawText(
            QRectF(0, center.y() + 32, self.width(), 24),
            Qt.AlignmentFlag.AlignHCenter | Qt.AlignmentFlag.AlignVCenter,
            label,
        )
        painter.restore()

    def _empty_hint(self) -> str:
        if self._language == "en":
            if self._empty_kind == "hsv":
                return "After capture, the HSV preview appears here"
            return "Click Capture above to get an image"
        if self._empty_kind == "hsv":
            return tr("\获\取\图\像\后\，\这\里\会\显\示 HSV \预\览\效\果")
        return tr("\请\点\击\上\方\「\图\像\帧\获\取\」\按\钮\获\取\图\像")

    def _advance_pulse(self) -> None:
        self._pulse = (self._pulse + 1) % 12
        if self._hover_pos is not None:
            self.update()

    def _advance_loading(self) -> None:
        self._loading_step = (self._loading_step + 1) % 20
        self.update()


class CenterWorkspace(QWidget):
    selection_finished = Signal(tuple)
    selection_cancelled = Signal()
    device_capture_requested = Signal(object)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._language = "zh"
        self._capture_loading = False
        self._capture_minimum_elapsed = False
        self._pending_original_image = None
        self._pending_hsv_image = None
        self._capture_delay_timer = QTimer(self)
        self._capture_delay_timer.setSingleShot(True)
        self._capture_delay_timer.setInterval(600)
        self._capture_delay_timer.timeout.connect(self._handle_capture_delay_elapsed)
        self._build()

    def _build(self) -> None:
        root = vbox(self, margins=(0, 0, 0, 0), spacing=0)
        frame = panel()
        root.addWidget(frame)
        layout = vbox(frame, margins=(16, 12, 16, 14), spacing=12)

        header = hbox(spacing=10)
        header.addWidget(LineIcon("image", 28))
        self._title = QLabel()
        self._title.setProperty("sectionTitle", True)
        header.addWidget(self._title)
        header.addStretch()

        self._capture_button = QPushButton()
        self._capture_button.setProperty("outlinePrimary", True)
        self._capture_button.setProperty("deviceAction", True)
        self._capture_button.setCursor(Qt.CursorShape.PointingHandCursor)
        self._capture_button.setIcon(make_icon("camera", 20))
        self._capture_button.setIconSize(QSize(20, 20))
        self._capture_button.setFixedSize(118, 36)
        self._capture_button.clicked.connect(self._handle_capture_clicked)
        header.addWidget(self._capture_button)
        layout.addLayout(header)

        previews = hbox(spacing=12)
        self._original_preview = PreviewCanvas(tr("\暂\无\原\图"), selectable=True)
        self._original_preview.selection_finished.connect(self.selection_finished.emit)
        self._original_preview.selection_cancelled.connect(self.selection_cancelled.emit)
        self._hsv_preview = PreviewCanvas(tr("\暂\无 HSV \图\像"))
        previews.addWidget(self._original_preview, 1)
        previews.addWidget(self._hsv_preview, 1)
        layout.addLayout(previews, 1)
        self.set_language(self._language)

    def set_language(self, language: str) -> None:
        self._language = language
        if language == "en":
            self._title.setText("Current Image")
            self._capture_button.setText("Loading..." if self._capture_loading else "Capture")
        else:
            self._title.setText(tr("\当\前\图\像"))
            self._capture_button.setText(tr("\获\取\中...") if self._capture_loading else tr("\图\像\帧\获\取"))
        self._original_preview.set_language(language)
        self._hsv_preview.set_language(language)

    def set_original_image(self, image: QImage) -> None:
        if self._capture_loading:
            self._pending_original_image = image.copy()
            self._try_finish_capture_loading()
            return
        self._original_preview.set_preview_image(image)

    def set_hsv_image(self, image: QImage) -> None:
        if self._capture_loading:
            self._pending_hsv_image = image.copy()
            self._try_finish_capture_loading()
            return
        self._hsv_preview.set_preview_image(image)

    def set_preview_image(self, image: QImage) -> None:
        self.set_original_image(image)

    def set_selection_enabled(self, enabled: bool) -> None:
        self._original_preview.set_selection_enabled(enabled)

    def cancel_capture_loading(self) -> None:
        self._set_capture_loading(False)
        self._pending_original_image = None
        self._pending_hsv_image = None

    def _handle_capture_clicked(self) -> None:
        if self._capture_loading:
            return
        self.device_capture_requested.emit(self._begin_capture_loading)

    def _begin_capture_loading(self) -> None:
        if self._capture_loading:
            return
        self._pending_original_image = None
        self._pending_hsv_image = None
        self._capture_minimum_elapsed = False
        self._set_capture_loading(True)
        self._capture_delay_timer.start()

    def _handle_capture_delay_elapsed(self) -> None:
        self._capture_minimum_elapsed = True
        self._try_finish_capture_loading()

    def _try_finish_capture_loading(self) -> None:
        if not self._capture_loading or not self._capture_minimum_elapsed:
            return
        if self._pending_original_image is not None:
            self._original_preview.set_preview_image(self._pending_original_image)
        if self._pending_hsv_image is not None:
            self._hsv_preview.set_preview_image(self._pending_hsv_image)
        self._pending_original_image = None
        self._pending_hsv_image = None
        self._set_capture_loading(False)

    def _set_capture_loading(self, loading: bool) -> None:
        self._capture_loading = loading
        if not loading:
            self._capture_delay_timer.stop()
            self._capture_minimum_elapsed = False
        self._capture_button.setEnabled(not loading)
        self._capture_button.setCursor(Qt.CursorShape.ArrowCursor if loading else Qt.CursorShape.PointingHandCursor)
        self._original_preview.set_loading(loading)
        self._hsv_preview.set_loading(loading)
        self.set_language(self._language)
