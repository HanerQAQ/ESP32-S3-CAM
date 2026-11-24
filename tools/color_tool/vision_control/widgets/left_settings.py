from __future__ import annotations

from PySide6.QtCore import QSize, Qt, Signal
from PySide6.QtGui import QColor, QPainter, QPen
from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QDialog,
    QFrame,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QRadioButton,
    QSizePolicy,
    QWidget,
)

from vision_control.models import UiState
from vision_control.widgets.common import LineIcon, hbox, make_icon, panel, vbox

DEFAULT_SERIAL_BAUDRATE = 921600


def tr(text: str) -> str:
    try:
        return text.encode("ascii").decode("unicode_escape")
    except Exception:
        return text.replace("\\", "")


class PortComboBox(QComboBox):
    popup_opened = Signal()
    popup_closed = Signal()

    def showPopup(self) -> None:
        self.popup_opened.emit()
        super().showPopup()

    def hidePopup(self) -> None:
        super().hidePopup()
        self.popup_closed.emit()


class StatusBadge(QWidget):
    """Minimal inline connection status indicator: dot + label, no background."""
    def __init__(self, parent=None):
        super().__init__(parent)
        self._online = False
        self._language = "zh"
        self._text = "未连接"
        self.setFixedHeight(20)
        self._update_width()

    def set_online(self, online: bool, port: str = "") -> None:
        self._online = online
        if online:
            self._text = "Connected" if self._language == "en" else "已连接"
        else:
            self._text = "Disconnected" if self._language == "en" else "未连接"
        self._update_width()
        self.update()

    def set_language(self, language: str) -> None:
        self._language = language
        self.set_online(self._online)

    def _update_width(self) -> None:
        fm = self.fontMetrics()
        text_width = fm.horizontalAdvance(self._text)
        self.setFixedWidth(text_width + 18)  # 8 dot area + 4 gap + text + 6 right margin

    def paintEvent(self, event) -> None:
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        h = self.height()
        w = self.width()

        # Small status dot
        dot_r = 4
        dot_cx = dot_r + 1
        dot_cy = h // 2
        if self._online:
            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(QColor(46, 200, 100))
            painter.drawEllipse(dot_cx - dot_r, dot_cy - dot_r, dot_r * 2, dot_r * 2)
        else:
            painter.setPen(QPen(QColor("#484f58"), 1.5))
            painter.setBrush(Qt.BrushStyle.NoBrush)
            painter.drawEllipse(dot_cx - dot_r, dot_cy - dot_r, dot_r * 2, dot_r * 2)

        # Label text (muted, unobtrusive)
        text_color = QColor("#a3b3cc") if self._online else QColor("#585e6a")
        painter.setPen(text_color)
        text_x = dot_cx + dot_r + 5
        painter.drawText(text_x, 0, w - text_x, h,
                         Qt.AlignmentFlag.AlignVCenter | Qt.AlignmentFlag.AlignLeft,
                         self._text)


class LeftSettingsPanel(QWidget):
    connect_requested = Signal(str, int, str, str, str)
    disconnect_requested = Signal()
    serial_ports_popup_monitor_started = Signal()
    serial_ports_popup_monitor_stopped = Signal()
    serial_ports_connection_monitor_started = Signal()
    serial_ports_connection_monitor_stopped = Signal()
    mode_changed = Signal(str)
    recognition_colors_changed = Signal(tuple)
    line_color_changed = Signal(str)
    notification_requested = Signal(str, str)
    language_changed = Signal(str)

    def __init__(self, state: UiState, parent=None):
        super().__init__(parent)
        self._state = state
        self._connected = state.connected
        self._last_connection_error = ""
        self._mode = "recognition"
        self._recognition_colors = {color.name for color in state.colors[:5]}
        self._line_color = state.active_color or state.colors[0].name
        self._color_checks = {}
        self._language = "zh"
        self.setMinimumHeight(52)
        self.setMaximumHeight(60)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        self._build()

    def _build(self) -> None:
        root = vbox(self, margins=(0, 0, 0, 0), spacing=0)
        frame = panel()
        root.addWidget(frame)
        layout = hbox(frame, margins=(14, 6, 14, 6), spacing=12)

        # Header: Icon + Title + Status Pill
        header = hbox(spacing=10)
        header.addWidget(LineIcon("device", 28))
        self._title = QLabel()
        self._title.setProperty("sectionTitle", True)
        header.addWidget(self._title)
        header.addSpacing(8)

        self._status_badge = StatusBadge()
        header.addWidget(self._status_badge)

        layout.addLayout(header)
        layout.addStretch()

        # Language dropdown
        self._language_combo = QComboBox()
        self._language_combo.addItems(["中文", "English"])
        self._language_combo.setFixedSize(96, 32)
        self._language_combo.setProperty("languageSwitch", True)
        self._language_combo.setCursor(Qt.CursorShape.PointingHandCursor)
        self._language_combo.currentIndexChanged.connect(self._handle_language_changed)
        layout.addWidget(self._language_combo)

        # Port dropdown
        self._serial_port_combo = PortComboBox()
        self._serial_port_combo.setPlaceholderText(tr("\选\择\端\口"))
        self._serial_port_combo.addItems(["COM1", "COM2", "COM3", "COM4"])
        if self._state.serial.port:
            self._serial_port_combo.setCurrentText(self._state.serial.port)
        else:
            self._serial_port_combo.setCurrentIndex(-1)
        self._serial_port_combo.popup_opened.connect(self.serial_ports_popup_monitor_started.emit)
        self._serial_port_combo.popup_closed.connect(self.serial_ports_popup_monitor_stopped.emit)
        self._serial_port_combo.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)
        self._serial_port_combo.setFixedWidth(120)
        self._serial_port_combo.setFixedHeight(32)
        self._serial_port_combo.setCursor(Qt.CursorShape.PointingHandCursor)
        layout.addWidget(self._serial_port_combo)

        # Action button (Connect / Disconnect)
        self._action_button = self._button("", 118)
        self._action_button.setIcon(make_icon("connect", 20))
        self._action_button.clicked.connect(self._handle_action_clicked)
        layout.addWidget(self._action_button)

        self._connection_hint = QLabel()
        self._connection_hint.setProperty("connectionHint", True)
        self._connection_hint.hide()
        layout.addWidget(self._connection_hint)

        self._build_device_config_dialog()
        self.set_connection_state(self._connected)
        self.set_language(self._language)

    def _button(self, text: str, width: int) -> QPushButton:
        button = QPushButton(text)
        button.setProperty("outlinePrimary", True)
        button.setProperty("deviceAction", True)
        button.setCursor(Qt.CursorShape.PointingHandCursor)
        button.setIconSize(QSize(18, 18))
        button.setFixedWidth(width)
        button.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)
        button.setFixedHeight(32)
        return button

    def _build_device_config_dialog(self) -> None:
        self._device_config_dialog = QDialog(self)
        self._device_config_dialog.setWindowFlags(Qt.WindowType.FramelessWindowHint | Qt.WindowType.Dialog)
        self._device_config_dialog.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground, True)
        self._device_config_dialog.setModal(True)
        self._device_config_dialog.setMinimumWidth(340)

        root = vbox(self._device_config_dialog, margins=(0, 0, 0, 0), spacing=0)
        shell = QFrame()
        shell.setProperty("dialogShell", True)
        root.addWidget(shell)
        layout = vbox(shell, margins=(20, 20, 20, 18), spacing=14)

        title = QLabel(tr("\当\前\配\置"))
        title.setProperty("dialogTitle", True)
        layout.addWidget(title)

        self._recognition_mode = QRadioButton(tr("\颜\色\识\别"))
        self._line_mode = QRadioButton(tr("\巡\线\模\式"))
        self._recognition_mode.setChecked(True)
        self._recognition_mode.toggled.connect(lambda checked: checked and self._set_mode("recognition"))
        self._line_mode.toggled.connect(lambda checked: checked and self._set_mode("line"))
        layout.addWidget(self._recognition_mode)
        layout.addWidget(self._line_mode)

        color_panel = QFrame()
        color_panel.setProperty("modeColorPanel", True)
        self._color_layout = vbox(color_panel, margins=(12, 12, 12, 12), spacing=8)
        layout.addWidget(color_panel)
        for color in self._state.colors[:5]:
            self._add_color_check(color.name, color.swatch)
        self._refresh_color_checks()

        actions = hbox(spacing=10)
        actions.addStretch()
        close = QPushButton(tr("\关\闭"))
        close.setFixedHeight(34)
        close.clicked.connect(self._device_config_dialog.reject)
        actions.addWidget(close)
        download = QPushButton(tr("\下\载"))
        download.setProperty("primaryAccent", True)
        download.setFixedHeight(34)
        download.clicked.connect(self._device_config_dialog.accept)
        actions.addWidget(download)
        layout.addLayout(actions)

    def set_language(self, language: str) -> None:
        self._language = language
        self._title.setText("System Settings" if language == "en" else tr("\系\统\设\置"))
        self._serial_port_combo.setPlaceholderText("Select Port" if language == "en" else tr("\选\择\端\口"))
        self._status_badge.set_language(language)
        self.set_connection_state(self._connected)

    def _handle_language_changed(self, index: int) -> None:
        language = "en" if index == 1 else "zh"
        self.set_language(language)
        self.language_changed.emit(language)

    def _add_color_check(self, name: str, swatch: str) -> None:
        check = QCheckBox(name)
        check.setProperty("colorChoiceText", True)
        check.setStyleSheet(f"QCheckBox {{ color: #f0f6fc; }} QCheckBox::indicator:checked {{ background-color: {swatch}; border-color: {swatch}; }}")
        check.toggled.connect(lambda _checked, color_name=name: self._handle_color_toggled(color_name))
        self._color_checks[name] = check
        self._color_layout.addWidget(check)

    def _open_device_config_dialog(self) -> None:
        self._device_config_dialog.exec()

    def _set_mode(self, mode: str) -> None:
        self._mode = mode
        self._refresh_color_checks()
        self.mode_changed.emit(mode)

    def _handle_color_toggled(self, color_name: str) -> None:
        if color_name not in self._color_checks:
            return
        if self._mode == "recognition":
            selected = {name for name, check in self._color_checks.items() if check.isChecked()}
            if not selected:
                self._color_checks[color_name].setChecked(True)
                return
            self._recognition_colors = set(list(selected)[:5])
            self.recognition_colors_changed.emit(self.current_target_colors())
            return
        self._line_color = color_name
        self._refresh_color_checks()
        self.line_color_changed.emit(color_name)

    def _refresh_color_checks(self) -> None:
        for name, check in self._color_checks.items():
            check.blockSignals(True)
            if self._mode == "recognition":
                check.setChecked(name in self._recognition_colors)
                check.setAutoExclusive(False)
            else:
                check.setChecked(name == self._line_color)
                check.setAutoExclusive(True)
            check.blockSignals(False)

    def set_connection_state(self, connected: bool) -> None:
        self._connected = connected
        if connected:
            self._last_connection_error = ""
        if self._language == "en":
            self._action_button.setText("Disconnect" if connected else "Connect")
        else:
            self._action_button.setText(tr("\断\开\连\接" if connected else "\连\接\设\备"))
        self._action_button.setProperty("connected", connected)
        self._action_button.style().unpolish(self._action_button)
        self._action_button.style().polish(self._action_button)

        port = self._serial_port_combo.currentText().strip()
        self._status_badge.set_online(connected, port)

    def set_serial_ports(self, ports: tuple[str, ...]) -> None:
        current = self._serial_port_combo.currentText()
        current_ports = tuple(self._serial_port_combo.itemText(index) for index in range(self._serial_port_combo.count()))
        if ports and ports == current_ports:
            return
        selected_port_missing = bool(current and ports and current not in ports)
        selected_port_removed = bool(current and not ports)
        self._serial_port_combo.blockSignals(True)
        self._serial_port_combo.clear()
        if ports:
            self._serial_port_combo.addItems(list(ports))
            if current in ports:
                self._serial_port_combo.setCurrentText(current)
            else:
                self._serial_port_combo.setCurrentIndex(-1)
        else:
            self._serial_port_combo.addItem(tr("\未\检\测\到\串\口"))
        self._serial_port_combo.blockSignals(False)
        if self._connected and (selected_port_missing or selected_port_removed):
            return

    def set_serial_ports_scan_error(self, message: str) -> None:
        self._serial_port_combo.blockSignals(True)
        self._serial_port_combo.clear()
        self._serial_port_combo.addItem(message)
        self._serial_port_combo.blockSignals(False)

    def set_connection_error(self, message: str) -> None:
        if not self._connected and message == self._last_connection_error:
            return
        self._last_connection_error = message
        self.set_connection_state(False)
        title = "Disconnected" if self._language == "en" else tr("\连\接\已\断\开")
        self.notification_requested.emit(title, "")

    def _set_connection_hint(self, message: str) -> None:
        self._connection_hint.setText(message)
        self._connection_hint.hide()

    def _handle_action_clicked(self) -> None:
        if self._connected:
            self.disconnect_requested.emit()
            self.set_connection_state(False)
            self.serial_ports_connection_monitor_stopped.emit()
            return
        if not self._can_request_connection():
            return
        self._set_connection_hint("")
        self.connect_requested.emit(
            self._serial_port_combo.currentText().strip(),
            DEFAULT_SERIAL_BAUDRATE,
            self._state.serial.data_bits,
            self._state.serial.parity,
            self._state.serial.stop_bits,
        )

    def _can_request_connection(self) -> bool:
        has_port = bool(self._serial_port_combo.currentText().strip())
        if not has_port:
            if self._language == "en":
                self.notification_requested.emit("Action Required", "Select a port first.")
            else:
                self.notification_requested.emit(tr("\操\作\提\示"), tr("\请\先\选\择\端\口\。"))
            return False
        return True

    def current_mode(self) -> str:
        return self._mode

    def current_target_colors(self) -> tuple[str, ...]:
        if self._mode == "recognition":
            return tuple(name for name in self._color_checks if name in self._recognition_colors)
        return (self._line_color,)

    def set_color_options(self, colors: tuple[tuple[str, str, bool], ...]) -> None:
        self._recognition_colors = {name for name in self._recognition_colors if name in self._color_checks}
        if not self._recognition_colors and self._color_checks:
            self._recognition_colors = {next(iter(self._color_checks))}
        self._refresh_color_checks()
