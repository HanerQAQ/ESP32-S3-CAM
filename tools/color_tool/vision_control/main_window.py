from __future__ import annotations

from PySide6.QtCore import Qt
from PySide6.QtWidgets import QFileDialog, QMainWindow, QVBoxLayout, QWidget

from vision_control.controllers.image_controller import ImageController
from vision_control.controllers.serial_controller import SerialController
from vision_control.models import HsvThreshold, UiState
from vision_control.widgets.center_workspace import CenterWorkspace
from vision_control.widgets.common import AppToast
from vision_control.widgets.left_settings import LeftSettingsPanel
from vision_control.widgets.recognition_panel import RightRecognitionPanel


def tr(text: str) -> str:
    try:
        return text.encode("ascii").decode("unicode_escape")
    except Exception:
        return text.replace("\\", "")


class MainWindow(QMainWindow):
    def __init__(self, state: UiState | None = None):
        super().__init__()
        self._state = state or UiState()
        self._language = "zh"
        self.setWindowTitle("ColorTool_v1.0")
        self.resize(1320, 860)
        self.setMinimumSize(1120, 760)
        self._build()

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        if hasattr(self, "_toast") and self._toast.isVisible():
            parent = self._toast.parentWidget()
            if parent is not None:
                self._toast.move((parent.width() - self._toast.width()) // 2, 14)

    def _build(self) -> None:
        root = QWidget()
        root.setObjectName("Root")
        self.setCentralWidget(root)
        self._toast = AppToast(root)

        page = QVBoxLayout(root)
        page.setContentsMargins(0, 0, 0, 0)
        page.setSpacing(0)

        content = QVBoxLayout()
        content.setContentsMargins(12, 10, 12, 10)
        content.setSpacing(10)
        page.addLayout(content, 1)

        self._left_settings = LeftSettingsPanel(self._state)
        self._recognition_panel = RightRecognitionPanel(self._state)
        self._center_workspace = CenterWorkspace()
        self._image_controller = ImageController(self)
        self._serial_controller = SerialController(self)

        self._left_settings.serial_ports_popup_monitor_started.connect(self._serial_controller.handle_serial_ports_popup_monitor_started)
        self._left_settings.serial_ports_popup_monitor_stopped.connect(self._serial_controller.handle_serial_ports_popup_monitor_stopped)
        self._left_settings.serial_ports_connection_monitor_started.connect(self._serial_controller.handle_serial_ports_connection_monitor_started)
        self._left_settings.serial_ports_connection_monitor_stopped.connect(self._serial_controller.handle_serial_ports_connection_monitor_stopped)
        self._serial_controller.serial_ports_changed.connect(self._left_settings.set_serial_ports)
        self._serial_controller.serial_ports_scan_error.connect(self._left_settings.set_serial_ports_scan_error)
        self._serial_controller.serial_ports_scan_error.connect(lambda message: self._show_notification(self._text("Serial scan failed", tr("\串\口\扫\描\失\败")), message))
        self._serial_controller.connection_error_occurred.connect(lambda _message: self._center_workspace.cancel_capture_loading())
        self._left_settings.notification_requested.connect(self._show_notification)
        self._left_settings.language_changed.connect(self._handle_language_changed)
        self._left_settings.connect_requested.connect(self._serial_controller.handle_connect_requested)
        self._left_settings.disconnect_requested.connect(self._serial_controller.handle_disconnect_requested)
        self._serial_controller.connection_state_changed.connect(self._left_settings.set_connection_state)
        self._serial_controller.connection_error_occurred.connect(self._left_settings.set_connection_error)
        self._serial_controller.connection_error_occurred.connect(lambda _message: self._serial_controller.handle_disconnect_requested())
        self._serial_controller.image_frame_error_occurred.connect(lambda _message: self._center_workspace.cancel_capture_loading())
        self._serial_controller.image_frame_error_occurred.connect(self._show_capture_error)
        self._serial_controller.settings_save_result_received.connect(self._show_device_settings_save_result)
        self._recognition_panel.read_settings_requested.connect(self._handle_settings_read_requested)
        self._center_workspace.device_capture_requested.connect(self._handle_device_capture_requested)
        self._serial_controller.settings_received.connect(self._recognition_panel.set_filter_configs)
        self._serial_controller.settings_received.connect(lambda _configs: self._show_settings_read_result())
        self._serial_controller.image_frame_received.connect(self._image_controller.handle_device_image_received)
        self._recognition_panel.save_settings_requested.connect(self._handle_settings_save_requested)
        self._recognition_panel.import_preset_requested.connect(self._handle_import_preset_requested)
        self._recognition_panel.export_preset_requested.connect(self._handle_export_preset_requested)

        self._center_workspace.selection_finished.connect(self._image_controller.handle_selection_finished)
        self._center_workspace.selection_cancelled.connect(self._image_controller.handle_selection_cancelled)
        self._image_controller.original_image_changed.connect(self._center_workspace.set_original_image)
        self._image_controller.hsv_image_changed.connect(self._center_workspace.set_hsv_image)
        self._image_controller.selection_mode_changed.connect(self._center_workspace.set_selection_enabled)
        self._image_controller.threshold_picked.connect(self._handle_threshold_picked)
        self._image_controller.error_occurred.connect(lambda _message: self._center_workspace.cancel_capture_loading())
        self._image_controller.error_occurred.connect(self._show_error)
        self._image_controller.device_image_error_occurred.connect(lambda _message: self._center_workspace.cancel_capture_loading())
        self._image_controller.device_image_error_occurred.connect(self._show_capture_error)
        self._image_controller.pick_color_error_occurred.connect(self._show_pick_color_error)

        self._recognition_panel.color_configs_changed.connect(self._left_settings.set_color_options)
        self._recognition_panel.filter_configs_changed.connect(self._image_controller.handle_color_configs_changed)
        self._recognition_panel.active_threshold_changed.connect(self._image_controller.handle_active_threshold_changed)
        self._recognition_panel.pick_color_requested.connect(self._image_controller.handle_pick_color_requested)
        self._image_controller.handle_color_configs_changed(self._recognition_panel.current_filter_configs())

        top = QVBoxLayout()
        top.setContentsMargins(0, 0, 0, 0)
        top.setSpacing(10)
        top.addWidget(self._left_settings, 0)
        content.addLayout(top, 0)
        content.addWidget(self._center_workspace, 1)
        content.addWidget(self._recognition_panel, 0)

    def keyPressEvent(self, event) -> None:
        if event.key() == Qt.Key.Key_Escape:
            self._image_controller.handle_selection_cancelled()
            self._center_workspace.set_selection_enabled(False)
            event.accept()
            return
        super().keyPressEvent(event)

    def _handle_language_changed(self, language: str) -> None:
        self._language = language
        self._recognition_panel.set_language(language)
        self._center_workspace.set_language(language)

    def _text(self, en: str, zh: str) -> str:
        return en if self._language == "en" else zh

    def _show_error(self, message: str) -> None:
        self._show_notification(self._text("Image open failed", tr("\图\像\打\开\失\败")), self._translate_message(message))

    def _show_capture_error(self, message: str) -> None:
        self._show_notification(self._text("Image capture failed", tr("\图\像\帧\获\取\失\败")), self._translate_message(message))

    def _handle_settings_read_requested(self) -> None:
        if not self._ensure_serial_connected():
            return
        self._serial_controller.handle_settings_read_requested()

    def _handle_settings_save_requested(self) -> None:
        if not self._ensure_serial_connected():
            return
        self._serial_controller.handle_settings_save_requested(
            self._left_settings.current_mode(),
            self._left_settings.current_target_colors(),
            self._recognition_panel.current_filter_configs(),
        )

    def _handle_export_preset_requested(self) -> None:
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            self._text("Export Color Preset", tr("\导\出\颜\色\预\设")),
            "color_presets.json",
            "JSON Files (*.json);;All Files (*)",
        )
        if not file_path:
            return
        try:
            from vision_control.presets import save_preset
            configs = self._recognition_panel.current_filter_configs()
            save_preset(file_path, configs)
            self._show_notification(self._text("Preset exported successfully", tr("\预\设\导\出\成\功")))
        except Exception as err:
            self._show_notification(self._text("Export failed", tr("\导\出\失\败")), str(err))

    def _handle_import_preset_requested(self) -> None:
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            self._text("Import Color Preset", tr("\导\入\颜\色\预\设")),
            "",
            "JSON Files (*.json);;All Files (*)",
        )
        if not file_path:
            return
        try:
            from vision_control.presets import load_preset
            configs = load_preset(file_path)
            self._recognition_panel.set_filter_configs(configs)
            self._show_notification(self._text("Preset loaded successfully", tr("\预\设\加\载\成\功")))
        except Exception as err:
            self._show_notification(self._text("Import failed", tr("\导\入\失\败")), str(err))

    def _handle_device_capture_requested(self, begin_capture_loading) -> None:
        if not self._ensure_serial_connected():
            return
        begin_capture_loading()
        self._serial_controller.handle_device_capture_requested()

    def _ensure_serial_connected(self) -> bool:
        if self._serial_controller.is_connected():
            return True
        self._show_notification(self._text("Serial port is not connected", tr("\串\口\未\连\接")))
        return False

    def _show_pick_color_error(self, message: str) -> None:
        self._show_notification(self._text("Pick color failed", tr("\取\色\失\败")), self._translate_message(message))

    def _handle_threshold_picked(self, color_name: str, threshold: HsvThreshold) -> None:
        self._recognition_panel.set_color_threshold(color_name, threshold)
        self._show_notification(self._text("Applied", tr("\已\应\用")))

    def _show_settings_read_result(self) -> None:
        self._show_notification(self._text("Read successfully", tr("\读\取\成\功")))

    def _show_device_settings_save_result(self, is_save: bool) -> None:
        if is_save:
            self._show_notification(self._text("Downloaded", tr("\下\载\成\功")))
            return
        self._show_notification(self._text("Download failed", tr("\下\载\失\败")))

    def _show_notification(self, title: str, message: str = "") -> None:
        self._toast.show_message(self._translate_title(title), self._translate_message(message))

    def _translate_title(self, title: str) -> str:
        if self._language != "en":
            return title
        translations = {
            tr("\串\口\扫\描\失\败"): "Serial scan failed",
            tr("\图\像\打\开\失\败"): "Image open failed",
            tr("\取\色\失\败"): "Pick color failed",
            tr("\取\色\已\应\用"): "Pick color applied",
            tr("\设\备\配\置\下\载\成\功"): "Device configuration downloaded",
            tr("\设\备\配\置\下\载\失\败"): "Device configuration download failed",
            tr("\图\像\捕\获\异\常"): "Image capture error",
            tr("\图\像\帧\获\取\失\败"): "Image capture failed",
            tr("\连\接\错\误"): "Connection Error",
            tr("\连\接\已\断\开"): "Disconnected",
            tr("\操\作\提\示"): "Action Required",
            tr("\串\口\未\连\接"): "Serial port is not connected",
            tr("\读\取\成\功"): "Read successfully",
            tr("\预\设\导\出\成\功"): "Preset exported successfully",
            tr("\预\设\加\载\成\功"): "Preset loaded successfully",
            tr("\导\出\失\败"): "Export failed",
            tr("\导\入\失\败"): "Import failed",
        }
        return translations.get(title, title)

    def _translate_message(self, message: str) -> str:
        if self._language != "en":
            return message
        translations = {
            tr("\请\先\打\开\或\获\取\一\张\图\像\。"): "Open or capture an image first.",
            tr("\读\取\设\置\返\回\参\数\格\式\无\效\。"): "The returned settings data format is invalid.",
            tr("\保\存\设\置\返\回\参\数\格\式\无\效\。"): "The save-settings response format is invalid.",
            tr("\设\备\保\存\设\置\失\败\。"): "The device failed to save settings.",
            tr("\图\像\解\析\错\误\：\设\备\返\回\的\数\据\不\是 JPEG\。"): "Image parsing error: the device returned data that is not JPEG.",
            tr("\图\像\帧\获\取\超\时\：\设\备\未\在 2 \秒\内\返\回\完\整 JPEG \数\据\。"): "Image capture timed out: the device did not return complete JPEG data within 2 seconds.",
            tr("\串\口\未\连\接\。"): "Serial port is not connected.",
            tr("\请\先\选\择\端\口\。"): "Select a port first.",
        }
        if message in translations:
            return translations[message]
        prefix_translations = (
            (tr("\串\口\打\开\失\败\："), "Failed to open serial port: "),
            (tr("\串\口\发\送\失\败\："), "Failed to send serial data: "),
            (tr("\串\口\读\取\失\败\："), "Failed to read serial data: "),
            (tr("\未\知\协\议\功\能\号\："), "Unknown protocol function: "),
        )
        for source, target in prefix_translations:
            if message.startswith(source):
                return target + message.removeprefix(source)
        return message
