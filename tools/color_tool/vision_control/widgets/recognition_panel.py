from __future__ import annotations

from dataclasses import replace

from PySide6.QtCore import QPointF, QRectF, QSize, Qt, QTimer, Signal
from PySide6.QtGui import QColor, QFont, QLinearGradient, QPainter, QPen
from PySide6.QtWidgets import (
    QButtonGroup,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QSizePolicy,
    QSpinBox,
    QWidget,
)

from vision_control.models import ColorConfig, HsvThreshold, UiState
from vision_control.widgets.common import (
    ClickToEditSpinBox,
    LineIcon,
    hbox,
    install_delayed_tooltip,
    make_icon,
    panel,
    vbox,
)


def tr(text: str) -> str:
    try:
        return text.encode("ascii").decode("unicode_escape")
    except Exception:
        return text.replace("\\", "")


class RangeSlider(QWidget):
    value_changed = Signal(int, int)

    def __init__(self, low: int, high: int, maximum: int, gradient: str, allow_invert: bool = False, parent=None):
        super().__init__(parent)
        self._low = low
        self._high = high
        self._maximum = maximum
        self._gradient = gradient
        self._allow_invert = allow_invert
        self._active_handle = ""
        self._hover_handle = ""
        self.setMinimumHeight(32)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        self.setCursor(Qt.CursorShape.PointingHandCursor)
        self.setMouseTracking(True)

    def values(self) -> tuple[int, int]:
        return self._low, self._high

    def set_values(self, low: int, high: int) -> None:
        self._low = max(0, min(self._maximum, low))
        self._high = max(0, min(self._maximum, high))
        if not self._allow_invert and self._low > self._high:
            self._low, self._high = self._high, self._low
        self.update()

    def paintEvent(self, event) -> None:
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        groove = self._groove_rect()

        # Draw gradient track
        painter.setPen(Qt.PenStyle.NoPen)
        painter.setBrush(self._make_gradient(groove))
        painter.drawRoundedRect(groove, 4.0, 4.0)

        low_x = self._value_to_x(self._low)
        high_x = self._value_to_x(self._high)

        # Dim unselected region to highlight active selection
        dim_brush = QColor(24, 31, 48, 205)
        painter.setPen(Qt.PenStyle.NoPen)
        painter.setBrush(dim_brush)

        if self._low <= self._high:
            if low_x > groove.left():
                painter.drawRoundedRect(QRectF(groove.left(), groove.top(), low_x - groove.left(), groove.height()), 3.0, 3.0)
            if high_x < groove.right():
                painter.drawRoundedRect(QRectF(high_x, groove.top(), groove.right() - high_x, groove.height()), 3.0, 3.0)
        else:
            # Inverted mode (hue wrap around 0): [high_x, low_x] is inactive
            if low_x > high_x:
                painter.drawRect(QRectF(high_x, groove.top(), low_x - high_x, groove.height()))

        # Draw handles
        center_y = groove.center().y()
        self._draw_handle(painter, low_x, center_y, is_active=(self._active_handle == "low" or self._hover_handle == "low"))
        self._draw_handle(painter, high_x, center_y, is_active=(self._active_handle == "high" or self._hover_handle == "high"))

    def _draw_handle(self, painter: QPainter, x: float, y: float, is_active: bool = False) -> None:
        r = 7.5 if not is_active else 8.5
        # Outer ring / shadow
        painter.setPen(QPen(QColor(0, 0, 0, 90), 2.5))
        painter.setBrush(Qt.BrushStyle.NoBrush)
        painter.drawEllipse(QPointF(x, y + 0.5), r, r)

        # Handle body
        border_color = QColor("#ff7a00") if is_active else QColor("#e2e8f0")
        painter.setPen(QPen(border_color, 2.0))
        painter.setBrush(QColor("#1f2638"))
        painter.drawEllipse(QPointF(x, y), r, r)

        # Center pip
        pip_color = QColor("#ff7a00") if is_active else QColor("#ffffff")
        painter.setPen(Qt.PenStyle.NoPen)
        painter.setBrush(pip_color)
        painter.drawEllipse(QPointF(x, y), 2.5, 2.5)

    def mousePressEvent(self, event) -> None:
        value = self._x_to_value(event.position().x())
        self._active_handle = "low" if abs(value - self._low) <= abs(value - self._high) else "high"
        self._set_active_value(value)

    def mouseMoveEvent(self, event) -> None:
        pos_x = event.position().x()
        val = self._x_to_value(pos_x)
        self._hover_handle = "low" if abs(val - self._low) <= abs(val - self._high) else "high"
        if self._active_handle:
            self._set_active_value(val)
        else:
            self.update()

    def leaveEvent(self, event) -> None:
        self._hover_handle = ""
        self.update()
        super().leaveEvent(event)

    def mouseReleaseEvent(self, event) -> None:
        self._active_handle = ""
        self.update()

    def _set_active_value(self, value: int) -> None:
        if self._allow_invert:
            if self._active_handle == "low":
                self._low = value
            elif self._active_handle == "high":
                self._high = value
        else:
            if self._active_handle == "low":
                self._low = min(value, self._high)
            elif self._active_handle == "high":
                self._high = max(value, self._low)
        self.update()
        self.value_changed.emit(self._low, self._high)

    def _groove_rect(self) -> QRectF:
        return QRectF(10, (self.height() - 6) / 2, max(1, self.width() - 20), 6)

    def _value_to_x(self, value: int) -> float:
        groove = self._groove_rect()
        return groove.left() + groove.width() * value / self._maximum

    def _x_to_value(self, x: float) -> int:
        groove = self._groove_rect()
        ratio = (x - groove.left()) / groove.width()
        return max(0, min(self._maximum, round(ratio * self._maximum)))

    def _make_gradient(self, rect: QRectF) -> QLinearGradient:
        gradient = QLinearGradient(rect.left(), rect.center().y(), rect.right(), rect.center().y())
        if self._gradient == "hue":
            for pos, color in (
                (0.0, "#ff0000"),
                (0.17, "#ffff00"),
                (0.33, "#00ff00"),
                (0.50, "#00ffff"),
                (0.67, "#0000ff"),
                (0.83, "#ff00ff"),
                (1.0, "#ff0000"),
            ):
                gradient.setColorAt(pos, QColor(color))
            return gradient
        if self._gradient == "sat":
            gradient.setColorAt(0.0, QColor("#f1f5f9"))
            gradient.setColorAt(1.0, QColor("#1e70e4"))
            return gradient
        gradient.setColorAt(0.0, QColor("#121622"))
        gradient.setColorAt(1.0, QColor("#f1f5f9"))
        return gradient


class HsvLine(QWidget):
    value_changed = Signal(int, int)

    def __init__(
        self,
        title_zh: str,
        title_en: str,
        low: int,
        high: int,
        maximum: int = 255,
        gradient: str = "value",
        allow_invert: bool = False,
        parent=None,
    ):
        super().__init__(parent)
        self._maximum = maximum
        self._title_zh = title_zh
        self._title_en = title_en
        self._allow_invert = allow_invert

        layout = QGridLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setHorizontalSpacing(10)
        layout.setVerticalSpacing(0)

        # Col 0: Title Label
        self._title_label = QLabel(title_zh)
        self._title_label.setProperty("compactLabel", True)
        self._title_label.setFixedWidth(54)
        layout.addWidget(self._title_label, 0, 0)

        # Col 1: RangeSlider
        self._slider = RangeSlider(low, high, maximum, gradient, allow_invert=allow_invert)
        self._slider.setMinimumWidth(320)
        layout.addWidget(self._slider, 0, 1)

        # Col 2: Min Label
        self._low_label = self._value_label(tr("\最\小\："))
        layout.addWidget(self._low_label, 0, 2)

        # Col 3: Min SpinBox
        self._low_spin = self._spin(low)
        layout.addWidget(self._low_spin, 0, 3)

        # Col 4: Max Label
        self._high_label = self._value_label(tr("\最\大\："))
        layout.addWidget(self._high_label, 0, 4)

        # Col 5: Max SpinBox
        self._high_spin = self._spin(high)
        layout.addWidget(self._high_spin, 0, 5)

        layout.setColumnStretch(1, 1)
        layout.setColumnMinimumWidth(2, 44)
        layout.setColumnMinimumWidth(3, 56)
        layout.setColumnMinimumWidth(4, 44)
        layout.setColumnMinimumWidth(5, 56)

        self._slider.value_changed.connect(self._handle_slider_changed)
        self._low_spin.valueChanged.connect(self._handle_spin_changed)
        self._high_spin.valueChanged.connect(self._handle_spin_changed)

    def _spin(self, value: int) -> QSpinBox:
        spin = ClickToEditSpinBox()
        spin.setRange(0, self._maximum)
        spin.setValue(value)
        spin.setButtonSymbols(QSpinBox.ButtonSymbols.NoButtons)
        spin.setAlignment(Qt.AlignmentFlag.AlignCenter)
        spin.setProperty("hsvValue", True)
        spin.setFixedSize(56, 30)
        return spin

    def _value_label(self, text: str) -> QLabel:
        label = QLabel(text)
        label.setProperty("compactLabel", True)
        label.setAlignment(Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)
        label.setFixedWidth(44)
        return label

    def values(self) -> tuple[int, int]:
        return self._slider.values()

    def set_values(self, low: int, high: int) -> None:
        for widget in (self._low_spin, self._high_spin):
            widget.blockSignals(True)
        self._slider.set_values(low, high)
        self._low_spin.setValue(low)
        self._high_spin.setValue(high)
        for widget in (self._low_spin, self._high_spin):
            widget.blockSignals(False)

    def set_language(self, language: str) -> None:
        self._title_label.setText(self._title_en if language == "en" else self._title_zh)
        self._low_label.setText("Min:" if language == "en" else tr("\最\小\："))
        self._high_label.setText("Max:" if language == "en" else tr("\最\大\："))

    def _handle_slider_changed(self, low: int, high: int) -> None:
        self._low_spin.blockSignals(True)
        self._high_spin.blockSignals(True)
        self._low_spin.setValue(low)
        self._high_spin.setValue(high)
        self._low_spin.blockSignals(False)
        self._high_spin.blockSignals(False)
        self._emit_value_changed()

    def _handle_spin_changed(self) -> None:
        if self._allow_invert:
            low = self._low_spin.value()
            high = self._high_spin.value()
        else:
            low = min(self._low_spin.value(), self._high_spin.value())
            high = max(self._low_spin.value(), self._high_spin.value())
        self._slider.set_values(low, high)
        self._low_spin.blockSignals(True)
        self._high_spin.blockSignals(True)
        self._low_spin.setValue(low)
        self._high_spin.setValue(high)
        self._low_spin.blockSignals(False)
        self._high_spin.blockSignals(False)
        self._emit_value_changed()

    def _emit_value_changed(self) -> None:
        self.value_changed.emit(*self.values())


class ColorDotButton(QPushButton):
    """Jewel-like color channel selector button."""
    def __init__(self, name: str, swatch: str, parent=None):
        super().__init__(parent)
        self._name = name
        self._swatch = QColor(swatch)
        self._ring_angle = 0
        self._ring_timer = QTimer(self)
        self._ring_timer.setInterval(40)
        self._ring_timer.timeout.connect(self._advance_ring)
        self.setCheckable(True)
        self.setCursor(Qt.CursorShape.PointingHandCursor)
        self.setFixedSize(38, 38)
        self.toggled.connect(self._handle_toggled)

    def set_swatch(self, color: str) -> None:
        self._swatch = QColor(color)
        self.update()

    def paintEvent(self, event) -> None:
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        center = QPointF(self.width() / 2, self.height() / 2)

        if self.isChecked():
            # Glowing active indicator ring
            radius = 16.5
            rect = QRectF(center.x() - radius, center.y() - radius, radius * 2, radius * 2)
            painter.setPen(QPen(QColor(255, 122, 0, 230), 2.4, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap))
            painter.setBrush(Qt.BrushStyle.NoBrush)
            painter.drawArc(rect, int(self._ring_angle * 16), 3800)

            # Inner spacer ring
            painter.setPen(QPen(QColor("#1f2638"), 2.0))
            painter.setBrush(Qt.BrushStyle.NoBrush)
            painter.drawEllipse(center, 13.5, 13.5)

            # Center color dot
            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(self._swatch)
            painter.drawEllipse(center, 11.5, 11.5)
            return

        # Idle state
        painter.setPen(QPen(QColor("#334060"), 2.0))
        painter.setBrush(self._swatch)
        painter.drawEllipse(center, 12.5, 12.5)

    def _handle_toggled(self, checked: bool) -> None:
        if checked:
            self._ring_timer.start()
        else:
            self._ring_timer.stop()
        self.update()

    def _advance_ring(self) -> None:
        self._ring_angle = (self._ring_angle + 8) % 360
        self.update()


class SharedHsvPanel(QFrame):
    threshold_changed = Signal()
    pick_color_requested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setProperty("innerCard", True)
        self.setMinimumHeight(196)
        self._loading = False
        self._language = "zh"
        self._active_color_name = "color1"
        self._build()

    def _build(self) -> None:
        layout = vbox(self, margins=(16, 12, 16, 14), spacing=10)

        # Header: Left (Active color pill) | Right (Invert Button & Pick Button)
        header = hbox(spacing=10)

        # Color swatch dot + Label
        self._color_indicator = QLabel()
        self._color_indicator.setFixedSize(14, 14)
        self._color_indicator.setStyleSheet("border-radius: 7px; background-color: #ef233c; border: 1px solid rgba(255,255,255,0.4);")
        header.addWidget(self._color_indicator)

        self._active_color_label = QLabel()
        self._active_color_label.setProperty("activeColorTitle", True)
        header.addWidget(self._active_color_label)

        header.addStretch()

        # Invert Toggle Button
        self._invert_button = QPushButton(tr("\取\反"))
        self._invert_button.setCheckable(True)
        self._invert_button.setProperty("invertButton", True)
        self._invert_button.setFixedSize(76, 32)
        self._invert_button.setCursor(Qt.CursorShape.PointingHandCursor)
        self._invert_button.clicked.connect(self._handle_invert_clicked)
        header.addWidget(self._invert_button)

        # Quick Pick Button
        self._quick_pick = QPushButton()
        self._quick_pick.setIcon(make_icon("picker", 18, "#ff7a00"))
        self._quick_pick.setIconSize(QSize(18, 18))
        self._quick_pick.setProperty("hsvPickButton", True)
        self._quick_pick.setCursor(Qt.CursorShape.PointingHandCursor)
        self._quick_pick.setFixedSize(88, 32)
        self._quick_pick.clicked.connect(self.pick_color_requested.emit)
        header.addWidget(self._quick_pick)

        layout.addLayout(header)

        # The 3 HSV Lines
        self._hue_line = HsvLine(tr("\色\相"), "Hue", 0, 179, maximum=179, gradient="hue", allow_invert=True)
        self._sat_line = HsvLine(tr("\饱\和\度"), "Saturation", 0, 255, gradient="sat")
        self._val_line = HsvLine(tr("\明\度"), "Value", 0, 255, gradient="value")

        self._hue_line.value_changed.connect(self._handle_hue_changed)
        self._sat_line.value_changed.connect(self._emit_threshold_changed)
        self._val_line.value_changed.connect(self._emit_threshold_changed)

        layout.addWidget(self._hue_line)
        layout.addWidget(self._sat_line)
        layout.addWidget(self._val_line)

        self.set_language(self._language)

    def _update_invert_state(self) -> None:
        low, high = self._hue_line.values()
        is_inverted = low > high
        self._invert_button.setChecked(is_inverted)
        base = "Invert" if self._language == "en" else tr("\取\反")
        self._invert_button.setText(f"✓ {base}" if is_inverted else base)

    def _handle_hue_changed(self, low: int, high: int) -> None:
        self._update_invert_state()
        self._emit_threshold_changed()

    def _handle_invert_clicked(self, checked: bool) -> None:
        low, high = self._hue_line.values()
        if checked:
            if low <= high:
                if low == 0:
                    low = max(0, 179 - max(1, high))
                else:
                    low, high = high, low
        else:
            if low > high:
                low, high = min(low, high), max(low, high)
        self._hue_line.set_values(low, high)
        self._update_invert_state()
        self._emit_threshold_changed()

    def load_config(self, config: ColorConfig) -> None:
        self._loading = True
        self._active_color_name = config.name
        self._active_color_label.setText(self._display_color_name(config.name))
        self._color_indicator.setStyleSheet(f"border-radius: 7px; background-color: {config.swatch}; border: 1px solid rgba(255,255,255,0.4);")
        t = config.threshold
        self._hue_line.set_values(t.h_min, t.h_max)
        self._sat_line.set_values(t.s_min, t.s_max)
        self._val_line.set_values(t.v_min, t.v_max)
        self._update_invert_state()
        self._loading = False

    def threshold(self) -> HsvThreshold:
        h_min, h_max = self._hue_line.values()
        s_min, s_max = self._sat_line.values()
        v_min, v_max = self._val_line.values()
        return HsvThreshold(h_min, h_max, s_min, s_max, v_min, v_max)

    def set_threshold(self, threshold: HsvThreshold) -> None:
        self._hue_line.set_values(threshold.h_min, threshold.h_max)
        self._sat_line.set_values(threshold.s_min, threshold.s_max)
        self._val_line.set_values(threshold.v_min, threshold.v_max)
        self._update_invert_state()
        self._emit_threshold_changed()

    def _emit_threshold_changed(self, *_values) -> None:
        if not self._loading:
            self.threshold_changed.emit()

    def _display_color_name(self, name: str) -> str:
        if name.startswith("color"):
            prefix = "Color" if self._language == "en" else tr("\颜\色")
            return f"{prefix} {name.removeprefix('color')}"
        return name

    def set_language(self, language: str) -> None:
        self._language = language
        self._update_invert_state()
        invert_tip = (
            "Invert hue selection (supports wrap-around range for Red)"
            if language == "en"
            else tr("\色\相\取\反\（\跨 0 \度\选\择\红\色\）")
        )
        install_delayed_tooltip(self._invert_button, invert_tip, 500)

        self._quick_pick.setText("Pick Color" if language == "en" else tr("\取\色"))
        if language == "en":
            install_delayed_tooltip(
                self._quick_pick,
                "Move the mouse to the left image display area and click to sample HSV parameters. Use the mouse wheel to adjust the sampling radius.",
                500,
            )
        else:
            install_delayed_tooltip(
                self._quick_pick,
                tr("\将\鼠\标\移\动\到\左\侧\图\片\显\示\区\域\，\点\击\即\可\获\取\当\前\范\围\的HSV\参\数\，\滚\动\鼠\标\滚\轮\可\调\节\取\色\半\径"),
                500,
            )
        for line in (self._hue_line, self._sat_line, self._val_line):
            line.set_language(language)
        self._active_color_label.setText(self._display_color_name(self._active_color_name))


class RightRecognitionPanel(QWidget):
    color_configs_changed = Signal(tuple)
    filter_configs_changed = Signal(tuple)
    active_threshold_changed = Signal(str, tuple)
    pick_color_requested = Signal(str)
    read_settings_requested = Signal()
    save_settings_requested = Signal()
    import_preset_requested = Signal()
    export_preset_requested = Signal()
    MAX_COLOR_CARDS = 5

    def __init__(self, state: UiState, parent=None):
        super().__init__(parent)
        self._configs = [replace(config, enabled=True) for config in state.colors[: self.MAX_COLOR_CARDS]]
        self._active_index = 0
        self._color_dots = {}
        self._language = "zh"
        self.setMinimumHeight(280)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.MinimumExpanding)
        self._build()

    def _build(self) -> None:
        root = vbox(self, margins=(0, 0, 0, 0), spacing=0)
        frame = panel()
        root.addWidget(frame)
        layout = vbox(frame, margins=(16, 12, 16, 12), spacing=10)

        # Header: Left (Icon + Title) | Center (5 Color Dots) | Right (Read + Download)
        header = QGridLayout()
        header.setContentsMargins(0, 0, 0, 0)
        header.setHorizontalSpacing(12)
        header.setVerticalSpacing(0)
        header.setColumnStretch(0, 1)
        header.setColumnStretch(1, 1)
        header.setColumnStretch(2, 1)

        # Title group (Left)
        title_group = hbox(spacing=10)
        title_group.addWidget(LineIcon("palette", 28))
        self._title = QLabel()
        self._title.setProperty("sectionTitle", True)
        title_group.addWidget(self._title)
        title_group.addStretch()
        header.addLayout(title_group, 0, 0, Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter)

        # 5 Color Dots (Center)
        color_group = hbox(spacing=14)
        self._current_label = QLabel()
        self._current_label.hide()
        self._dot_group = QButtonGroup(self)
        self._dot_group.setExclusive(True)
        for config in self._configs:
            dot = ColorDotButton(config.name, config.swatch)
            dot.clicked.connect(lambda _checked=False, color_name=config.name: self.set_active_color(color_name))
            self._dot_group.addButton(dot)
            self._color_dots[config.name] = dot
            color_group.addWidget(dot)
        header.addLayout(color_group, 0, 0, 1, 3, Qt.AlignmentFlag.AlignHCenter | Qt.AlignmentFlag.AlignVCenter)

        # Action Buttons (Right)
        actions = QHBoxLayout()
        actions.setContentsMargins(0, 0, 0, 0)
        actions.setSpacing(8)

        self._import_button = self._header_button("", "picker", 88)
        self._import_button.clicked.connect(self.import_preset_requested.emit)
        actions.addWidget(self._import_button)

        self._export_button = self._header_button("", "download", 88)
        self._export_button.clicked.connect(self.export_preset_requested.emit)
        actions.addWidget(self._export_button)

        self._read_button = self._header_button("", "read", 96)
        self._read_button.clicked.connect(self.read_settings_requested.emit)
        actions.addWidget(self._read_button)

        self._download_button = self._header_button("", "download", 108, is_primary=True)
        self._download_button.clicked.connect(self.save_settings_requested.emit)
        actions.addWidget(self._download_button)

        header.addLayout(actions, 0, 2, Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)
        layout.addLayout(header)

        # Inner HSV Panel
        self._hsv_panel = SharedHsvPanel()
        self._hsv_panel.threshold_changed.connect(self._handle_shared_threshold_changed)
        self._hsv_panel.pick_color_requested.connect(self._request_quick_pick)
        layout.addWidget(self._hsv_panel, 1)

        self.set_language(self._language)
        self._refresh_active_ui()

    def _header_button(self, text: str, icon: str, width: int, is_primary: bool = False) -> QPushButton:
        button = QPushButton(text)
        button.setProperty("outlinePrimary", True)
        button.setProperty("deviceAction", True)
        if is_primary:
            button.setProperty("primaryAccent", True)
        button.setCursor(Qt.CursorShape.PointingHandCursor)
        button.setIcon(make_icon(icon, 20))
        button.setIconSize(QSize(20, 20))
        button.setFixedSize(width, 36)
        return button

    def set_language(self, language: str) -> None:
        self._language = language
        if language == "en":
            self._title.setText("Color Parameter Configuration")
            self._import_button.setText("Import")
            self._export_button.setText("Export")
            self._read_button.setText("Read")
            self._download_button.setText("Download")
            install_delayed_tooltip(self._import_button, "Import color presets from a JSON file.", 500)
            install_delayed_tooltip(self._export_button, "Export current color presets to a JSON file.", 500)
            install_delayed_tooltip(self._read_button, "Read all color parameter configurations from the device.", 500)
            install_delayed_tooltip(self._download_button, "Download all color parameter configurations to the device.", 500)
        else:
            self._title.setText(tr("\颜\色\参\数\配\置"))
            self._import_button.setText(tr("\导\入"))
            self._export_button.setText(tr("\导\出"))
            self._read_button.setText(tr("\一\键\读\取"))
            self._download_button.setText(tr("\一\键\下\载"))
            install_delayed_tooltip(self._import_button, tr("\从 JSON \文\件\导\入\阈\值\预\设"), 500)
            install_delayed_tooltip(self._export_button, tr("\导\出\当\前\阈\值\到 JSON \预\设\文\件"), 500)
            install_delayed_tooltip(self._read_button, tr("\从\设\备\读\取\所\有\颜\色\参\数\配\置"), 500)
            install_delayed_tooltip(self._download_button, tr("\下\载\所\有\颜\色\参\数\配\置\到\设\备"), 500)
        self._hsv_panel.set_language(language)

    def set_active_color(self, color_name: str) -> None:
        for index, config in enumerate(self._configs):
            if config.name == color_name:
                self._active_index = index
                self._refresh_active_ui()
                self.active_threshold_changed.emit(color_name, self._active_filter_configs())
                return

    def _refresh_active_ui(self) -> None:
        if not self._configs:
            return
        active = self._configs[self._active_index]
        for name, dot in self._color_dots.items():
            dot.setChecked(name == active.name)
        self._hsv_panel.load_config(active)

    def _update_active_config(self, **changes) -> None:
        if not self._configs:
            return
        if "threshold" in changes and "swatch" not in changes:
            changes["swatch"] = _threshold_to_swatch(changes["threshold"])
        self._configs[self._active_index] = replace(self._configs[self._active_index], **changes)
        active = self._configs[self._active_index]
        self._update_color_swatch(active.name, active.swatch)
        self._emit_color_configs_changed()
        self.active_threshold_changed.emit(active.name, self._active_filter_configs())

    def _handle_shared_threshold_changed(self) -> None:
        self._update_active_config(threshold=self._hsv_panel.threshold())

    def _request_quick_pick(self) -> None:
        if self._configs:
            self.pick_color_requested.emit(self._configs[self._active_index].name)

    def _emit_color_configs_changed(self) -> None:
        self.color_configs_changed.emit(tuple((config.name, config.swatch, True) for config in self._configs))
        self.filter_configs_changed.emit(self._active_filter_configs())

    def current_filter_configs(self) -> tuple[ColorConfig, ...]:
        return tuple(self._configs)

    def _active_filter_configs(self) -> tuple[ColorConfig, ...]:
        return tuple(replace(config, enabled=index == self._active_index) for index, config in enumerate(self._configs))

    def set_filter_configs(self, configs: tuple[ColorConfig, ...]) -> None:
        if not configs:
            return
        incoming_by_name = {config.name: replace(config, enabled=True) for config in configs[: self.MAX_COLOR_CARDS]}
        merged_configs = []
        for fallback, current in enumerate(self._configs[: self.MAX_COLOR_CARDS]):
            incoming = incoming_by_name.get(current.name)
            if incoming is None:
                incoming = incoming_by_name.get(f"color{fallback + 1}")
            merged_configs.append(incoming if incoming is not None else current)
        self._configs = merged_configs
        self._active_index = min(self._active_index, len(self._configs) - 1)
        for config in self._configs:
            self._update_color_swatch(config.name, config.swatch)
        self._refresh_active_ui()
        self._emit_color_configs_changed()

    def set_color_threshold(self, color_name: str, threshold: HsvThreshold) -> None:
        for index, config in enumerate(self._configs):
            if config.name == color_name:
                swatch = _threshold_to_swatch(threshold)
                self._configs[index] = replace(config, threshold=threshold, swatch=swatch)
                self._update_color_swatch(color_name, swatch)
                if index == self._active_index:
                    self._hsv_panel.load_config(self._configs[index])
                self._emit_color_configs_changed()
                if index == self._active_index:
                    self.active_threshold_changed.emit(color_name, self._active_filter_configs())
                return

    def _update_color_swatch(self, color_name: str, swatch: str) -> None:
        if color_name in self._color_dots:
            self._color_dots[color_name].set_swatch(swatch)

    def _display_color_name(self, name: str) -> str:
        if name.startswith("color"):
            prefix = "Color" if self._language == "en" else tr("\颜\色")
            return f"{prefix} {name.removeprefix('color')}"
        return name


def _threshold_to_swatch(threshold: HsvThreshold) -> str:
    if threshold.h_min <= threshold.h_max:
        hue = round((threshold.h_min + threshold.h_max) / 2 * 2) % 360
    else:
        h1 = threshold.h_min * 2
        h2 = threshold.h_max * 2
        span = (360 - h1) + h2
        hue = round(h1 + span / 2) % 360

    saturation = round((threshold.s_min + threshold.s_max) / 2)
    value = round((threshold.v_min + threshold.v_max) / 2)
    color = QColor.fromHsv(
        max(0, min(359, hue)),
        max(0, min(255, saturation)),
        max(0, min(255, value)),
    )
    return color.name()
