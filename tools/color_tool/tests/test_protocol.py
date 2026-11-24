"""Automated unit tests for ColorTool serial communication protocol and models.

Tests cover:
- Frame packing (build_frame, build_json_frame)
- XOR checksum calculation and validation
- Stream framing and recovery from garbage bytes (SerialFrameParser)
- Multi-fragment payload reassembly in-order and out-of-order (FragmentAssembler)
- Color threshold wrap-around validation for HSV bounds
"""

from __future__ import annotations

import json
import os
import sys
import pytest

# Ensure vision_control is importable regardless of current working directory
_TOOL_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _TOOL_DIR not in sys.path:
    sys.path.insert(0, _TOOL_DIR)

from vision_control.protocol.serial_protocol import (
    FRAME_HEADER,
    FUNC_IMAGE_CAPTURE,
    FUNC_SETTINGS_READ,
    FUNC_SETTINGS_SAVE,
    MAX_DATA_SIZE,
    MAX_PAYLOAD_SIZE,
    FragmentAssembler,
    ProtocolError,
    ProtocolFrame,
    SerialFrameParser,
    build_frame,
    build_json_frame,
    decode_json_payload,
    xor_checksum,
)
from vision_control.models import ColorConfig, HsvThreshold


class TestXorChecksum:
    """Test XOR checksum calculation logic."""

    def test_empty_bytes(self):
        assert xor_checksum(b"") == 0

    def test_single_byte(self):
        assert xor_checksum(b"\x5a") == 0x5A
        assert xor_checksum(b"\x00") == 0
        assert xor_checksum(b"\xff") == 0xFF

    def test_known_pattern(self):
        # 0x01 ^ 0x02 = 0x03; 0x03 ^ 0x03 = 0x00
        assert xor_checksum(bytes([0x01, 0x02, 0x03])) == 0x00
        # 0xAA ^ 0x55 = 0xFF
        assert xor_checksum(bytes([0xAA, 0x55])) == 0xFF

    def test_self_cancellation(self):
        payload = b"ESP32-S3-CAM Vision Test Data"
        chk = xor_checksum(payload)
        # XOR-ing payload + checksum byte must yield 0
        assert xor_checksum(payload + bytes((chk,))) == 0


class TestBuildFrame:
    """Test UART packet frame generation."""

    def test_valid_single_frame(self):
        payload = b"test_payload"
        frame = build_frame(FUNC_SETTINGS_READ, payload, fragment_id=1, total_fragments=1)

        # Header: 2 bytes
        assert frame[:2] == FRAME_HEADER
        # Data length: function (1B) + payload (12B) = 13B
        data_len = int.from_bytes(frame[2:4], "little")
        assert data_len == 1 + len(payload)
        # Fragment ID & Total Fragments: 2B each
        frag_id = int.from_bytes(frame[4:6], "little")
        total_frag = int.from_bytes(frame[6:8], "little")
        assert frag_id == 1
        assert total_frag == 1
        # Data area: function + payload
        data_area = frame[8:-1]
        assert data_area[0] == FUNC_SETTINGS_READ
        assert data_area[1:] == payload
        # Checksum: last byte
        checksum = frame[-1]
        assert checksum == xor_checksum(data_area)

    def test_invalid_function_raises(self):
        with pytest.raises(ProtocolError):
            build_frame(-1, b"")
        with pytest.raises(ProtocolError):
            build_frame(256, b"")

    def test_payload_exceeding_max_raises(self):
        huge_payload = b"X" * (MAX_PAYLOAD_SIZE + 1)
        with pytest.raises(ProtocolError):
            build_frame(FUNC_IMAGE_CAPTURE, huge_payload)

    def test_invalid_fragment_indices_raises(self):
        with pytest.raises(ProtocolError):
            build_frame(FUNC_SETTINGS_SAVE, b"abc", fragment_id=0, total_fragments=1)
        with pytest.raises(ProtocolError):
            build_frame(FUNC_SETTINGS_SAVE, b"abc", fragment_id=2, total_fragments=1)


class TestJsonPayload:
    """Test JSON payload packaging and parsing."""

    def test_build_and_decode_json(self):
        cmd = "get_info"
        params = {"color": "red", "threshold": [170, 10, 80, 255, 80, 255]}
        frame = build_json_frame(FUNC_SETTINGS_READ, cmd, params)

        parser = SerialFrameParser()
        frames = parser.feed(frame)
        assert len(frames) == 1
        parsed = decode_json_payload(frames[0].payload)
        assert parsed["command"] == cmd
        assert parsed["params"]["color"] == "red"
        assert parsed["params"]["threshold"] == [170, 10, 80, 255, 80, 255]

    def test_decode_corrupt_json_raises(self):
        with pytest.raises(ProtocolError):
            decode_json_payload(b"Not a valid JSON string {")

    def test_decode_non_dict_json_raises(self):
        with pytest.raises(ProtocolError):
            decode_json_payload(b"\"just a string\"")


class TestSerialFrameParser:
    """Test streaming serial parser with byte chunks and error recovery."""

    def test_parser_normal_frame(self):
        frame_bytes = build_frame(FUNC_SETTINGS_SAVE, b'{"success":true}')
        parser = SerialFrameParser()
        frames = parser.feed(frame_bytes)
        assert len(frames) == 1
        assert frames[0].function == FUNC_SETTINGS_SAVE
        assert frames[0].payload == b'{"success":true}'

    def test_parser_chunked_stream(self):
        frame_bytes = build_frame(FUNC_IMAGE_CAPTURE, b"ImageChunk0123456789")
        parser = SerialFrameParser()

        # Feed 3 bytes at a time
        frames = []
        for i in range(0, len(frame_bytes), 3):
            frames.extend(parser.feed(frame_bytes[i:i + 3]))

        assert len(frames) == 1
        assert frames[0].payload == b"ImageChunk0123456789"

    def test_parser_skips_garbage_bytes(self):
        garbage = b"\x00\xff\x12\xaa\xaa\x10\x55RandomNoise"
        valid_frame = build_frame(FUNC_SETTINGS_READ, b"config_req")
        parser = SerialFrameParser()

        # Feed garbage followed by valid frame
        frames = parser.feed(garbage + valid_frame)
        assert len(frames) == 1
        assert frames[0].payload == b"config_req"

    def test_parser_rejects_corrupted_checksum(self):
        valid_frame = bytearray(build_frame(FUNC_SETTINGS_READ, b"clean"))
        # Corrupt the checksum byte
        valid_frame[-1] ^= 0x5A

        parser = SerialFrameParser()
        frames = parser.feed(bytes(valid_frame))
        assert len(frames) == 0

    def test_parser_multiple_frames_in_stream(self):
        f1 = build_frame(FUNC_SETTINGS_READ, b"req1", fragment_id=1, total_fragments=2)
        f2 = build_frame(FUNC_SETTINGS_READ, b"req2", fragment_id=2, total_fragments=2)

        parser = SerialFrameParser()
        frames = parser.feed(f1 + f2)
        assert len(frames) == 2
        assert frames[0].payload == b"req1"
        assert frames[1].payload == b"req2"


class TestFragmentAssembler:
    """Test multi-fragment reassembly."""

    def test_single_fragment(self):
        assembler = FragmentAssembler()
        frame = ProtocolFrame(fragment_id=1, total_fragments=1, function=FUNC_IMAGE_CAPTURE, payload=b"full_image")
        result = assembler.add(frame)
        assert result == b"full_image"

    def test_multi_fragment_in_order(self):
        assembler = FragmentAssembler()
        f1 = ProtocolFrame(1, 3, FUNC_IMAGE_CAPTURE, b"Part1-")
        f2 = ProtocolFrame(2, 3, FUNC_IMAGE_CAPTURE, b"Part2-")
        f3 = ProtocolFrame(3, 3, FUNC_IMAGE_CAPTURE, b"Part3")

        assert assembler.add(f1) is None
        assert assembler.add(f2) is None
        result = assembler.add(f3)
        assert result == b"Part1-Part2-Part3"

    def test_multi_fragment_out_of_order(self):
        assembler = FragmentAssembler()
        f1 = ProtocolFrame(1, 3, FUNC_IMAGE_CAPTURE, b"AAA")
        f2 = ProtocolFrame(2, 3, FUNC_IMAGE_CAPTURE, b"BBB")
        f3 = ProtocolFrame(3, 3, FUNC_IMAGE_CAPTURE, b"CCC")

        # Arrival: 2, 3, 1
        assert assembler.add(f2) is None
        assert assembler.add(f3) is None
        result = assembler.add(f1)
        assert result == b"AAABBBCCC"

    def test_inconsistent_function_raises(self):
        assembler = FragmentAssembler()
        f1 = ProtocolFrame(1, 2, FUNC_IMAGE_CAPTURE, b"AAA")
        f2 = ProtocolFrame(2, 2, FUNC_SETTINGS_READ, b"BBB")
        assembler.add(f1)
        with pytest.raises(ProtocolError):
            assembler.add(f2)


class TestColorConfigModel:
    """Test color models and HSV bounds."""

    def test_hsv_threshold_fields(self):
        t = HsvThreshold(h_min=170, h_max=10, s_min=80, s_max=255, v_min=80, v_max=255)
        assert t.h_min == 170
        assert t.h_max == 10
        # Red crosses 0/180 hue boundary
        is_wrap_around = t.h_min > t.h_max
        assert is_wrap_around is True

    def test_standard_hsv_does_not_wrap(self):
        blue = HsvThreshold(h_min=95, h_max=125, s_min=70, s_max=255, v_min=70, v_max=255)
        assert blue.h_min < blue.h_max


class TestColorPresets:
    """Test JSON preset serialization and deserialization."""

    def test_export_and_import_preset(self):
        from vision_control.presets import export_preset_to_dict, import_preset_from_dict
        configs = (
            ColorConfig("color1", True, "#ef233c", HsvThreshold(170, 10, 80, 255, 80, 255), 100),
            ColorConfig("color2", True, "#2f80ed", HsvThreshold(95, 125, 70, 255, 70, 255), 150),
        )
        data = export_preset_to_dict(configs, description="Test Preset")
        assert data["format"] == "ColorTool_Preset"
        assert len(data["colors"]) == 2

        restored = import_preset_from_dict(data)
        assert len(restored) == 2
        assert restored[0].name == "color1"
        assert restored[0].threshold.h_min == 170
        assert restored[0].threshold.h_max == 10
        assert restored[0].area_min == 100
        assert restored[1].swatch == "#2f80ed"

    def test_save_and_load_file(self, tmp_path):
        from vision_control.presets import save_preset, load_preset
        configs = (
            ColorConfig("color1", True, "#ff0000", HsvThreshold(0, 10, 100, 255, 100, 255)),
        )
        target = tmp_path / "sub" / "preset.json"
        save_preset(target, configs, "Temporary Profile")
        assert target.is_file()

        loaded = load_preset(target)
        assert len(loaded) == 1
        assert loaded[0].threshold.h_max == 10

    def test_invalid_preset_raises(self):
        from vision_control.presets import import_preset_from_dict, PresetError
        with pytest.raises(PresetError):
            import_preset_from_dict({"no_colors": 123})
        with pytest.raises(PresetError):
            import_preset_from_dict({"colors": [{"name": "bad", "hsv_min": [1, 2]}]})

    def test_recognition_panel_preset_integration(self):
        import os
        os.environ["QT_QPA_PLATFORM"] = "offscreen"
        QtWidgets = pytest.importorskip("PySide6.QtWidgets")
        QApplication = QtWidgets.QApplication
        _app = QApplication.instance() or QApplication([])
        from vision_control.models import UiState
        from vision_control.widgets.recognition_panel import RightRecognitionPanel

        panel = RightRecognitionPanel(UiState())
        assert hasattr(panel, "import_preset_requested")
        assert hasattr(panel, "export_preset_requested")
        assert panel._import_button is not None
        assert panel._export_button is not None

    def test_factory_presets_integrity(self):
        from pathlib import Path
        from vision_control.presets import load_preset

        presets_dir = Path(__file__).resolve().parent.parent / "presets"
        assert presets_dir.is_dir()

        files = list(presets_dir.glob("*.json"))
        assert len(files) >= 2, f"Expected at least 2 factory preset files, found {len(files)}"

        for p in files:
            configs = load_preset(p)
            assert len(configs) == 5, f"{p.name} must contain exactly 5 color configurations"
            for c in configs:
                assert c.name.startswith("color")
                assert 0 <= c.threshold.s_min <= 255
                assert 0 <= c.threshold.s_max <= 255
                assert 0 <= c.threshold.v_min <= 255
                assert 0 <= c.threshold.v_max <= 255

