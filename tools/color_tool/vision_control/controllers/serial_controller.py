from __future__ import annotations

import colorsys
import os
import sys
from datetime import datetime
from pathlib import Path
from typing import Any

from PySide6.QtCore import QObject, QTimer, Signal

from vision_control.models import ColorConfig, HsvThreshold
from vision_control.protocol.serial_protocol import (
    FUNC_IMAGE_CAPTURE,
    FUNC_SETTINGS_READ,
    FUNC_SETTINGS_SAVE,
    FragmentAssembler,
    ProtocolError,
    ProtocolFrame,
    SerialFrameParser,
    build_json_command,
    decode_json_payload,
)
from vision_control.services.serial_service import SerialService

COMMAND_VISION_SETTING = "vision_setting"
PARAM_GET_IMAGE_FRAME = "get_img_frame"
PARAM_GET_COLOR_CONFIG = "get_color_config"
IMAGE_CAPTURE_TIMEOUT_MS = 2000
IMAGE_CAPTURE_DEBUG_ENV = "COLOR_TOOL_CAPTURE_DEBUG"
IMAGE_CAPTURE_DEBUG_SWITCH = "image_capture_debug.enabled"
IMAGE_CAPTURE_DEBUG_LOG = "image_capture_debug.log"


class SerialController(QObject):
    serial_ports_changed = Signal(tuple)
    serial_ports_scan_error = Signal(str)
    connection_state_changed = Signal(bool)
    connection_error_occurred = Signal(str)
    image_frame_error_occurred = Signal(str)
    image_frame_received = Signal(bytes)
    settings_received = Signal(tuple)
    settings_save_result_received = Signal(bool)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._serial_service = SerialService(self)
        self._frame_parser = SerialFrameParser()
        self._fragment_assemblers = {}
        self._serial_service.connected.connect(self._handle_connected)
        self._serial_service.disconnected.connect(self._handle_disconnected)
        self._serial_service.data_received.connect(self._handle_data_received)
        self._serial_service.error_occurred.connect(self.connection_error_occurred.emit)
        self._last_ports = ()
        self._last_error = ""
        self._popup_monitoring = False
        self._connection_monitoring = False
        self._image_capture_in_progress = False
        self._image_capture_total_fragments = 0
        self._image_capture_received_fragments = set()
        self._image_capture_timeout_timer = QTimer(self)
        self._image_capture_timeout_timer.setSingleShot(True)
        self._image_capture_timeout_timer.setInterval(IMAGE_CAPTURE_TIMEOUT_MS)
        self._image_capture_timeout_timer.timeout.connect(self._handle_image_capture_timeout)
        self._ports_timer = QTimer(self)
        self._ports_timer.setInterval(800)
        self._ports_timer.timeout.connect(self.handle_serial_ports_scan_requested)

    def handle_serial_ports_popup_monitor_started(self) -> None:
        self._popup_monitoring = True
        self._refresh_monitor_timer()

    def handle_serial_ports_popup_monitor_stopped(self) -> None:
        self._popup_monitoring = False
        self._refresh_monitor_timer()

    def handle_serial_ports_connection_monitor_started(self) -> None:
        self._connection_monitoring = True
        self._refresh_monitor_timer()

    def handle_serial_ports_connection_monitor_stopped(self) -> None:
        self._connection_monitoring = False
        self._refresh_monitor_timer()

    def handle_connect_requested(
        self,
        port: str,
        baudrate: int,
        data_bits: str,
        parity: str,
        stop_bits: str,
    ) -> None:
        try:
            self._serial_service.open(port, baudrate, data_bits, parity, stop_bits)
        except Exception as exc:
            self.connection_error_occurred.emit(str(exc))

    def handle_disconnect_requested(self) -> None:
        self._serial_service.close()

    def is_connected(self) -> bool:
        return self._serial_service.is_open()

    def handle_device_capture_requested(self) -> None:
        self._fragment_assemblers.pop(FUNC_IMAGE_CAPTURE, None)
        self._image_capture_total_fragments = 0
        self._image_capture_received_fragments.clear()
        self._image_capture_in_progress = True
        if self._send_json_command(COMMAND_VISION_SETTING, PARAM_GET_IMAGE_FRAME):
            _write_capture_debug_log(f"request sent, timeout={IMAGE_CAPTURE_TIMEOUT_MS}ms")
            self._image_capture_timeout_timer.start()
            return
        self._image_capture_in_progress = False
        _write_capture_debug_log("request send failed")

    def handle_settings_read_requested(self) -> None:
        self._send_json_command(COMMAND_VISION_SETTING, PARAM_GET_COLOR_CONFIG)

    def handle_settings_save_requested(
        self,
        mode: str,
        target_colors: tuple[str, ...],
        configs: tuple[ColorConfig, ...],
    ) -> None:
        configs = configs[:5]
        color_indexes = _color_indexes(configs)
        params = {
            "color_config": [
                _color_config_to_dict(config, _color_index(config.name, color_indexes))
                for config in configs
            ]
        }
        self._send_json_command(COMMAND_VISION_SETTING, params)

    def _refresh_monitor_timer(self) -> None:
        if not self._popup_monitoring and not self._connection_monitoring:
            self._ports_timer.stop()
            return
        self.handle_serial_ports_scan_requested()
        if not self._ports_timer.isActive():
            self._ports_timer.start()

    def handle_serial_ports_scan_requested(self) -> None:
        try:
            ports = self._serial_service.list_ports()
        except Exception as exc:
            message = str(exc)
            if message != self._last_error:
                self._last_error = message
                self.serial_ports_scan_error.emit(message)
            return
        self._last_error = ""
        if ports == self._last_ports:
            return
        self._last_ports = ports
        self.serial_ports_changed.emit(ports)

    def _handle_connected(self) -> None:
        self._connection_monitoring = True
        self._refresh_monitor_timer()
        self.connection_state_changed.emit(True)

    def _handle_disconnected(self) -> None:
        self._connection_monitoring = False
        self._refresh_monitor_timer()
        self._clear_protocol_state()
        self.connection_state_changed.emit(False)

    def _handle_data_received(self, data: bytes) -> None:
        for frame in self._frame_parser.feed(data):
            self._handle_frame(frame)

    def _handle_frame(self, frame: ProtocolFrame) -> None:
        self._record_image_fragment(frame)
        try:
            payload = self._assemble_payload(frame)
        except ProtocolError as exc:
            if frame.function == FUNC_IMAGE_CAPTURE or self._image_capture_in_progress:
                self._print_image_fragment_status("protocol_error")
                self._handle_image_frame_error(str(exc), frame.function)
                return
            self.connection_error_occurred.emit(str(exc))
            return
        if payload is None:
            return

        if frame.function == FUNC_IMAGE_CAPTURE:
            self._handle_image_payload(payload)
            return
        if frame.function in {FUNC_SETTINGS_READ, FUNC_SETTINGS_SAVE}:
            self._handle_json_payload(frame.function, payload)
            return

        message = f"未知协议功能号：0x{frame.function:02X}。"
        if self._image_capture_in_progress:
            self._handle_image_frame_error(message, frame.function)
            return
        self.connection_error_occurred.emit(message)

    def _assemble_payload(self, frame: ProtocolFrame) -> bytes | None:
        if frame.total_fragments == 1:
            self._fragment_assemblers.pop(frame.function, None)
            return frame.payload
        assembler = self._fragment_assemblers.setdefault(frame.function, FragmentAssembler())
        payload = assembler.add(frame)
        if payload is not None:
            self._fragment_assemblers.pop(frame.function, None)
        return payload

    def _handle_json_payload(self, function: int, payload: bytes) -> None:
        try:
            message = decode_json_payload(payload)
        except ProtocolError as exc:
            self.connection_error_occurred.emit(str(exc))
            return
        self._handle_protocol_message(function, message)

    def _handle_protocol_message(self, function: int, message: dict[str, Any]) -> None:
        if function == FUNC_SETTINGS_READ:
            configs = _parse_color_configs(message.get("params"))
            if configs is None:
                self.connection_error_occurred.emit("读取设置返回参数格式无效。")
                return
            self.settings_received.emit(configs)
            return

        params = message.get("params")
        if not isinstance(params, dict) or "is_save" not in params:
            self.connection_error_occurred.emit("保存设置返回参数格式无效。")
            return
        is_save = bool(params["is_save"])
        self.settings_save_result_received.emit(is_save)
        if not is_save:
            self.connection_error_occurred.emit("设备保存设置失败。")

    def _send_json_command(self, command: str, params: Any) -> bool:
        payload = build_json_command(command, params)
        try:
            self._serial_service.write(payload)
        except Exception as exc:
            self.connection_error_occurred.emit(str(exc))
            return False
        return True

    def _handle_image_payload(self, payload: bytes) -> None:
        self._print_image_fragment_status("assembled")
        self._print_jpeg_marker_status(payload)
        if not _looks_like_jpeg(payload):
            self._handle_image_frame_error("图像解析错误：设备返回的数据不是 JPEG。", FUNC_IMAGE_CAPTURE)
            return
        self._image_capture_timeout_timer.stop()
        self._image_capture_in_progress = False
        self.image_frame_received.emit(payload)

    def _handle_image_capture_timeout(self) -> None:
        if not self._image_capture_in_progress:
            return
        self._print_image_fragment_status("timeout")
        seconds = IMAGE_CAPTURE_TIMEOUT_MS // 1000
        self._handle_image_frame_error(f"图像帧获取超时：设备未在 {seconds} 秒内返回完整 JPEG 数据。", FUNC_IMAGE_CAPTURE)

    def _handle_image_frame_error(self, message: str, function: int | None = None) -> None:
        if function is not None:
            self._fragment_assemblers.pop(function, None)
        self._fragment_assemblers.pop(FUNC_IMAGE_CAPTURE, None)
        self._image_capture_timeout_timer.stop()
        self._image_capture_in_progress = False
        self.image_frame_error_occurred.emit(message)

    def _clear_protocol_state(self) -> None:
        self._frame_parser = SerialFrameParser()
        self._fragment_assemblers.clear()
        self._image_capture_timeout_timer.stop()
        self._image_capture_in_progress = False
        self._image_capture_total_fragments = 0
        self._image_capture_received_fragments.clear()

    def _record_image_fragment(self, frame: ProtocolFrame) -> None:
        if frame.function != FUNC_IMAGE_CAPTURE:
            return
        if self._image_capture_total_fragments != frame.total_fragments:
            self._image_capture_total_fragments = frame.total_fragments
            self._image_capture_received_fragments.clear()
        self._image_capture_received_fragments.add(frame.fragment_id)

    def _print_image_fragment_status(self, reason: str) -> None:
        total = self._image_capture_total_fragments
        received = sorted(self._image_capture_received_fragments)
        if total <= 0:
            _write_capture_debug_log(f"fragments {reason}: no image fragments received")
            return
        expected = set(range(1, total + 1))
        missing = sorted(expected.difference(received))
        complete = not missing
        _write_capture_debug_log(
            f"fragments {reason}: received={len(received)}/{total}, complete={complete}, received_ids={received}, missing_ids={missing}"
        )

    def _print_jpeg_marker_status(self, payload: bytes) -> None:
        starts_with_soi = payload.startswith(b"\xff\xd8")
        ends_with_eoi = payload.endswith(b"\xff\xd9")
        head = payload[:2].hex(" ").upper() if payload else ""
        tail = payload[-2:].hex(" ").upper() if payload else ""
        _write_capture_debug_log(
            f"jpeg markers: bytes={len(payload)}, starts_ff_d8={starts_with_soi}, ends_ff_d9={ends_with_eoi}, head={head}, tail={tail}"
        )


def _capture_debug_dir() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent
    return Path.cwd()


def _capture_debug_enabled() -> bool:
    env_value = os.environ.get(IMAGE_CAPTURE_DEBUG_ENV, "").strip().lower()
    if env_value in {"1", "true", "yes", "on"}:
        return True
    return (_capture_debug_dir() / IMAGE_CAPTURE_DEBUG_SWITCH).exists()


def _write_capture_debug_log(message: str) -> None:
    if not _capture_debug_enabled():
        return
    try:
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
        log_path = _capture_debug_dir() / IMAGE_CAPTURE_DEBUG_LOG
        with log_path.open("a", encoding="utf-8") as file:
            file.write(f"[{timestamp}] [ImageCapture] {message}\n")
    except OSError:
        return


def _color_config_to_dict(config: ColorConfig, index: int | None = None) -> dict:
    return {
        "index": _color_index(config.name) if index is None else index,
        "hsv_min": [config.threshold.h_min, config.threshold.s_min, config.threshold.v_min],
        "hsv_max": [config.threshold.h_max, config.threshold.s_max, config.threshold.v_max],
        "box_color": _hex_color_to_rgb(config.swatch),
    }


def _hex_color_to_rgb(value: str) -> list[int]:
    color = value.lstrip("#")
    if len(color) != 6:
        return [0, 0, 0]
    return [int(color[index : index + 2], 16) for index in (0, 2, 4)]


def _color_indexes(configs: tuple[ColorConfig, ...]) -> dict[str, int]:
    return {config.name: _color_index(config.name, fallback=fallback) for fallback, config in enumerate(configs, start=1)}


def _color_index(name: str, indexes: dict[str, int] | None = None, fallback: int = 1) -> int:
    if indexes is not None and name in indexes:
        return indexes[name]
    lowered = name.lower()
    if lowered.startswith("color") and lowered[5:].isdigit():
        return max(1, min(5, int(lowered[5:])))
    return fallback


def _parse_color_configs(params: Any) -> tuple[ColorConfig, ...] | None:
    if not isinstance(params, list):
        return None

    configs = []
    for fallback, item in enumerate(params, start=1):
        if not isinstance(item, dict):
            return None
        hsv_min = item.get("hsv_min")
        hsv_max = item.get("hsv_max")
        box_color = item.get("box_color")
        if not _is_int_list(hsv_min, 3) or not _is_int_list(hsv_max, 3):
            return None
        if not _is_int_list(box_color, 3):
            box_color = _threshold_to_rgb(hsv_min, hsv_max)
        elif int(hsv_min[0]) > int(hsv_max[0]):
            # If hue wraps around 0 (red) but box_color was previously saved as cyan [G, B high, R low]
            if box_color[0] < 120 and box_color[1] > 140 and box_color[2] > 140:
                box_color = _threshold_to_rgb(hsv_min, hsv_max)

        raw_index = item.get("index", fallback)
        try:
            color_index = int(raw_index)
        except (TypeError, ValueError):
            color_index = fallback
        color_index = max(1, min(5, color_index))
        configs.append(
            ColorConfig(
                name=f"color{color_index}",
                enabled=True,
                swatch=_rgb_to_hex(box_color),
                threshold=HsvThreshold(
                    int(hsv_min[0]),
                    int(hsv_max[0]),
                    int(hsv_min[1]),
                    int(hsv_max[1]),
                    int(hsv_min[2]),
                    int(hsv_max[2]),
                ),
                area_min=int(item.get("area_thr", 0)),
            )
        )
    return tuple(configs)


def _is_int_list(value: Any, length: int) -> bool:
    return isinstance(value, list) and len(value) == length and all(isinstance(item, int) for item in value)


def _rgb_to_hex(value: list[int]) -> str:
    channels = [min(255, max(0, int(channel))) for channel in value[:3]]
    return f"#{channels[0]:02x}{channels[1]:02x}{channels[2]:02x}"


def _threshold_to_rgb(hsv_min: list[int], hsv_max: list[int]) -> list[int]:
    h_min = int(hsv_min[0])
    h_max = int(hsv_max[0])
    if h_min <= h_max:
        hue = (h_min + h_max) / 2 / 180
    else:
        span = (180 - h_min) + h_max
        hue = ((h_min + span / 2) % 180) / 180
    saturation = (int(hsv_min[1]) + int(hsv_max[1])) / 2 / 255
    value = (int(hsv_min[2]) + int(hsv_max[2])) / 2 / 255
    red, green, blue = colorsys.hsv_to_rgb(hue % 1.0, max(0, min(1, saturation)), max(0, min(1, value)))
    return [round(red * 255), round(green * 255), round(blue * 255)]


def _looks_like_jpeg(payload: bytes) -> bool:
    return len(payload) >= 4 and payload.startswith(b"\xff\xd8") and payload.endswith(b"\xff\xd9")
