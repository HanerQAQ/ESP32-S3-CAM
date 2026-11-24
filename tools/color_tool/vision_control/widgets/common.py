from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import QEasingCurve, QEvent, QObject, QPoint, QPointF, QPropertyAnimation, QRectF, QTimer, Qt
from PySide6.QtGui import QColor, QIcon, QPainter, QPainterPath, QPen, QPixmap
from PySide6.QtWidgets import (
    QFrame,
    QGraphicsDropShadowEffect,
    QGraphicsOpacityEffect,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QSpinBox,
    QToolTip,
    QVBoxLayout,
    QWidget,
)

ASSET_DIR = Path(__file__).resolve().parent.parent / "assets"
SVG_ICON_PATHS = {
    "connect": ASSET_DIR / "icon_connect.svg",
    "camera": ASSET_DIR / "icon_camera.svg",
    "read": ASSET_DIR / "icon_read.svg",
    "download": ASSET_DIR / "icon_download.svg",
    "picker": ASSET_DIR / "icon_picker.svg",
}


class _ButtonGlowFilter(QObject):
    """Subtle hover feedback filter."""
    def eventFilter(self, watched, event) -> bool:
        return super().eventFilter(watched, event)


def install_hover_glow(widget: QWidget) -> QWidget:
    return widget


def install_button_glow(button: QPushButton) -> QPushButton:
    return button


class _DelayedToolTipFilter(QObject):
    def __init__(self, widget: QWidget, delay_ms: int = 1000):
        super().__init__(widget)
        self._widget = widget
        self._timer = QTimer(self)
        self._timer.setSingleShot(True)
        self._timer.setInterval(delay_ms)
        self._timer.timeout.connect(self._show_tooltip)

    def eventFilter(self, watched, event) -> bool:
        if event.type() == QEvent.Type.Enter:
            if self._tooltip_text():
                self._timer.start()
        elif event.type() in (QEvent.Type.Leave, QEvent.Type.Hide, QEvent.Type.MouseButtonPress):
            self._timer.stop()
            QToolTip.hideText()
        return super().eventFilter(watched, event)

    def _tooltip_text(self) -> str:
        return str(self._widget.property("delayedToolTip") or "")

    def _show_tooltip(self) -> None:
        text = self._tooltip_text()
        if text and self._widget.isVisible() and self._widget.underMouse():
            position = self._widget.mapToGlobal(self._widget.rect().bottomLeft() + QPoint(0, 8))
            QToolTip.showText(position, text, self._widget)


def install_delayed_tooltip(widget: QWidget, text: str, delay_ms: int = 500) -> QWidget:
    widget.setToolTip("")
    widget.setProperty("delayedToolTip", text)
    if not hasattr(widget, "_delayed_tooltip_filter"):
        tooltip_filter = _DelayedToolTipFilter(widget, delay_ms)
        widget.installEventFilter(tooltip_filter)
        widget._delayed_tooltip_filter = tooltip_filter
    return widget


def panel() -> QFrame:
    frame = QFrame()
    frame.setProperty("panel", True)
    return frame


def make_icon(kind: str, size: int = 18, color: str = "#ff7a00") -> QIcon:
    svg_path = SVG_ICON_PATHS.get(kind)
    if svg_path is not None and svg_path.exists():
        return QIcon(str(svg_path))

    pixel_ratio = 3
    pixmap = QPixmap(size * pixel_ratio, size * pixel_ratio)
    pixmap.fill(Qt.GlobalColor.transparent)
    painter = QPainter(pixmap)
    painter.setRenderHint(QPainter.RenderHint.Antialiasing)
    painter.scale(pixel_ratio, pixel_ratio)
    _draw_line_icon(painter, kind, size, QColor(color))
    painter.end()
    pixmap.setDevicePixelRatio(pixel_ratio)
    return QIcon(pixmap)


class LineIcon(QWidget):
    """Badge-style section header icon."""
    def __init__(self, kind: str, size: int = 28, parent=None):
        super().__init__(parent)
        self._kind = kind
        self._size = size
        self.setFixedSize(size, size)

    def paintEvent(self, event) -> None:
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        # Draw subtle badge background
        badge_rect = QRectF(0.5, 0.5, self.width() - 1, self.height() - 1)
        painter.setPen(QPen(QColor(255, 122, 0, 45), 1))
        painter.setBrush(QColor(255, 122, 0, 20))
        painter.drawRoundedRect(badge_rect, 7, 7)

        # Center and draw the icon inside
        inner_size = int(self._size * 0.65)
        offset = (self._size - inner_size) / 2
        painter.save()
        painter.translate(offset, offset)
        _draw_line_icon(painter, self._kind, inner_size, QColor("#ff7a00"))
        painter.restore()


def _draw_line_icon(painter: QPainter, kind: str, size: int, color: QColor) -> None:
    scale = size / 20
    painter.save()
    painter.scale(scale, scale)
    painter.setPen(QPen(color, 1.8, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap, Qt.PenJoinStyle.RoundJoin))
    painter.setBrush(Qt.BrushStyle.NoBrush)

    if kind == "device":
        # Microcontroller / chip icon
        painter.drawRoundedRect(4.5, 4.5, 11, 11, 2, 2)
        # Pins
        for p in (7, 10, 13):
            painter.drawLine(QPointF(2, p), QPointF(4.5, p))
            painter.drawLine(QPointF(15.5, p), QPointF(18, p))
            painter.drawLine(QPointF(p, 2), QPointF(p, 4.5))
            painter.drawLine(QPointF(p, 15.5), QPointF(p, 18))
        # Center core
        painter.setBrush(color)
        painter.setPen(Qt.PenStyle.NoPen)
        painter.drawRoundedRect(7.5, 7.5, 5, 5, 1, 1)

    elif kind == "palette":
        # Artist color palette
        path = QPainterPath()
        path.moveTo(16.5, 10.0)
        path.cubicTo(16.2, 5.5, 12.8, 3.2, 9.0, 3.8)
        path.cubicTo(5.0, 4.5, 2.8, 7.8, 3.5, 11.5)
        path.cubicTo(4.4, 15.5, 8.8, 17.5, 12.8, 15.8)
        path.cubicTo(14.0, 15.3, 13.5, 13.8, 12.2, 13.8)
        painter.drawPath(path)
        painter.setBrush(color)
        painter.setPen(Qt.PenStyle.NoPen)
        painter.drawEllipse(QPointF(7.0, 8.0), 1.2, 1.2)
        painter.drawEllipse(QPointF(10.0, 6.5), 1.2, 1.2)
        painter.drawEllipse(QPointF(7.5, 12.0), 1.2, 1.2)

    elif kind == "image":
        # Camera / viewfinder / picture icon
        painter.drawRoundedRect(3, 3.5, 14, 13, 2.5, 2.5)
        painter.drawEllipse(QPointF(13.5, 7.0), 1.3, 1.3)
        path = QPainterPath()
        path.moveTo(4.5, 14.5)
        path.lineTo(8.0, 10.5)
        path.lineTo(11.0, 13.0)
        path.lineTo(13.0, 11.0)
        path.lineTo(15.5, 14.5)
        painter.drawPath(path)

    elif kind == "picker":
        # Crosshair color eyedropper
        painter.drawEllipse(QPointF(10.0, 10.0), 5.5, 5.5)
        painter.drawLine(QPointF(10.0, 2.5), QPointF(10.0, 5.5))
        painter.drawLine(QPointF(10.0, 14.5), QPointF(10.0, 17.5))
        painter.drawLine(QPointF(2.5, 10.0), QPointF(5.5, 10.0))
        painter.drawLine(QPointF(14.5, 10.0), QPointF(17.5, 10.0))
        painter.setBrush(color)
        painter.setPen(Qt.PenStyle.NoPen)
        painter.drawEllipse(QPointF(10.0, 10.0), 1.8, 1.8)

    elif kind == "camera":
        painter.drawRoundedRect(3.5, 6.0, 13.0, 10.0, 2.5, 2.5)
        painter.drawLine(QPointF(7.0, 6.0), QPointF(8.0, 4.5))
        painter.drawLine(QPointF(8.0, 4.5), QPointF(12.0, 4.5))
        painter.drawLine(QPointF(12.0, 4.5), QPointF(13.0, 6.0))
        painter.drawEllipse(QPointF(10.0, 11.0), 2.5, 2.5)

    elif kind == "read":
        painter.drawRoundedRect(4.5, 3.5, 9.0, 13.0, 2.0, 2.0)
        painter.drawLine(QPointF(7.0, 7.0), QPointF(11.0, 7.0))
        painter.drawLine(QPointF(7.0, 9.5), QPointF(10.5, 9.5))
        painter.drawEllipse(QPointF(13.5, 13.0), 2.2, 2.2)
        painter.drawLine(QPointF(15.2, 14.7), QPointF(17.2, 16.7))

    elif kind == "download":
        painter.drawRoundedRect(4.0, 3.5, 12.0, 13.0, 2.0, 2.0)
        painter.drawLine(QPointF(10.0, 6.5), QPointF(10.0, 12.0))
        painter.drawLine(QPointF(7.5, 9.8), QPointF(10.0, 12.3))
        painter.drawLine(QPointF(12.5, 9.8), QPointF(10.0, 12.3))
        painter.drawLine(QPointF(7.0, 14.2), QPointF(13.0, 14.2))

    painter.restore()


class ClickToEditSpinBox(QSpinBox):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFocusPolicy(Qt.FocusPolicy.ClickFocus)
        self.lineEdit().setAlignment(Qt.AlignmentFlag.AlignCenter)

    def wheelEvent(self, event) -> None:
        event.ignore()

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        self.lineEdit().setGeometry(self.rect().adjusted(2, 2, -2, -2))


class AppToast(QFrame):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("AppToast")
        self.setAttribute(Qt.WidgetAttribute.WA_StyledBackground, True)
        self.hide()

        self._label = QLabel()
        self._label.setWordWrap(True)
        self._label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._label.setStyleSheet("color: #f0f6fc; font-size: 13px; font-weight: 500;")

        layout = hbox(self, margins=(18, 9, 18, 9), spacing=0)
        layout.addWidget(self._label)

        self._opacity = QGraphicsOpacityEffect(self)
        self._opacity.setOpacity(1.0)
        self.setGraphicsEffect(self._opacity)

        self._hide_timer = QTimer(self)
        self._hide_timer.setSingleShot(True)
        self._hide_timer.timeout.connect(self._fade_out)
        self._animation = QPropertyAnimation(self._opacity, b"opacity", self)
        self._animation.setDuration(350)
        self._animation.setEasingCurve(QEasingCurve.Type.OutCubic)
        self._animation.finished.connect(self._handle_animation_finished)
        self.setStyleSheet(
            "QFrame#AppToast {"
            "background-color: #1c2230;"
            "border: 1px solid #38455e;"
            "border-radius: 18px;"
            "}"
        )

    def show_message(self, title: str, message: str = "", duration: int = 1600) -> None:
        text = f"{title}：{message}" if message else title
        self._label.setText(text)
        self.adjustSize()
        parent = self.parentWidget()
        if parent is not None:
            width = min(max(self.sizeHint().width(), 240), max(280, parent.width() - 48))
            self.setFixedWidth(width)
            self.adjustSize()
            self.move((parent.width() - self.width()) // 2, 16)
        self._animation.stop()
        self._opacity.setOpacity(0.0)
        self.show()
        self.raise_()
        self._fade_in()
        self._hide_timer.start(duration)

    def _fade_in(self) -> None:
        self._animation.stop()
        self._animation.setDuration(180)
        self._animation.setStartValue(0.0)
        self._animation.setEndValue(1.0)
        self._animation.start()

    def _fade_out(self) -> None:
        self._animation.stop()
        self._animation.setDuration(350)
        self._animation.setStartValue(1.0)
        self._animation.setEndValue(0.0)
        self._animation.start()

    def _handle_animation_finished(self) -> None:
        if self._opacity.opacity() <= 0.05:
            self.hide()


def vbox(parent=None, margins=(16, 14, 16, 14), spacing=12) -> QVBoxLayout:
    layout = QVBoxLayout(parent)
    layout.setContentsMargins(*margins)
    layout.setSpacing(spacing)
    return layout


def hbox(parent=None, margins=(0, 0, 0, 0), spacing=10) -> QHBoxLayout:
    layout = QHBoxLayout(parent)
    layout.setContentsMargins(*margins)
    layout.setSpacing(spacing)
    layout.setAlignment(Qt.AlignmentFlag.AlignVCenter)
    return layout
