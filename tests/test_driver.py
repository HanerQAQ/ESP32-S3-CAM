"""Automated unit tests for ESP32-S3-CAM Python SDK driver.

Tests cover:
- Binary helpers (_checksum, _u16_le, _float_le)
- I2C 32-byte frame reception and verification
- 0x55 resync recovery when 0xAA header byte is lost
- Error handling (checksum mismatch, malformed frame size, invalid fragment id)
- Multi-fragment packet assembly
- Computer vision target decoders (Color blocks, Face, Cat/Dog, QR Code)
"""

import os
import sys
import struct
import pytest

_REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_SDK_DIR = os.path.join(_REPO_ROOT, "examples", "python_sdk")
if _SDK_DIR not in sys.path:
    sys.path.insert(0, _SDK_DIR)

from esp32_s3_cam.driver import (
    ESP32_S3_CAM,
    ESP32_S3_CAM_Error,
    ESP32_S3_CAM_I2C_ADDR,
    _FRAME_HEADER_H,
    _FRAME_HEADER_L,
    _FRAME_MAX_SIZE,
    _checksum,
    _float_le,
    _u16_le,
)


class MockI2CBus:
    """Mock I2C bus providing readfrom and writeto methods."""

    def __init__(self, read_queue=None):
        self.read_queue = list(read_queue or [])
        self.written_data = []

    def writeto(self, addr, data):
        self.written_data.append((addr, bytes(data)))

    def readfrom(self, addr, count):
        if not self.read_queue:
            # Return dummy 0xFF (I2C bus not ready / pulled high)
            return bytes([0xFF] * count)
        chunk = self.read_queue.pop(0)
        if len(chunk) < count:
            # Pad or truncate
            return chunk + bytes([0x00] * (count - len(chunk)))
        return chunk[:count]


def make_raw_i2c_frame(payload: bytes, fragment_id: int = 1, total_fragments: int = 1) -> bytes:
    """Helper to synthesize a compliant 32-byte slave I2C response frame."""
    data_len = len(payload)
    header = bytes([
        _FRAME_HEADER_H,
        _FRAME_HEADER_L,
        data_len & 0xFF,
        (data_len >> 8) & 0xFF,
        fragment_id & 0xFF,
        (fragment_id >> 8) & 0xFF,
        total_fragments & 0xFF,
        (total_fragments >> 8) & 0xFF,
    ])
    chk = bytes([_checksum(payload)])
    frame = header + payload + chk
    # Pad to 32 bytes
    if len(frame) < _FRAME_MAX_SIZE:
        frame += bytes([0x00] * (_FRAME_MAX_SIZE - len(frame)))
    return frame[:_FRAME_MAX_SIZE]


class TestBinaryHelpers:
    """Test low-level binary packing/unpacking helpers."""

    def test_checksum(self):
        assert _checksum(b"") == 0
        assert _checksum(b"\x12\x34\x56") == (0x12 ^ 0x34 ^ 0x56)
        assert _checksum(bytes([0xAA, 0x55])) == 0xFF

    def test_u16_le(self):
        buf = bytes([0x34, 0x12, 0x00, 0x80])
        assert _u16_le(buf, 0) == 0x1234
        assert _u16_le(buf, 2) == 0x8000

    def test_float_le(self):
        # Pack 3.14159f as little-endian 4-byte float
        expected = 3.1415927410125732
        buf = struct.pack("<f", expected)
        assert abs(_float_le(buf, 0) - expected) < 1e-6


class TestFrameReception:
    """Test 32-byte frame reception, checksum verification and 0x55 recovery."""

    def test_receive_valid_single_frame(self):
        payload = b"S3CamVision"
        frame_bytes = make_raw_i2c_frame(payload)
        bus = MockI2CBus(read_queue=[frame_bytes])
        cam = ESP32_S3_CAM(bus)

        data_len, frag_id, total_frag, recv_payload = cam.receive_frame()
        assert data_len == len(payload)
        assert frag_id == 1
        assert total_frag == 1
        assert recv_payload == payload

    def test_receive_frame_with_0x55_resync(self):
        """Simulate hardware FIFO eating the first 0xAA byte; frame starts with 0x55."""
        payload = b"FaceDetected!"
        normal_frame = make_raw_i2c_frame(payload)
        # Shift left by 1 byte (0xAA eaten, 0x55 becomes first byte)
        shifted_frame = normal_frame[1:] + b"\x00"
        assert shifted_frame[0] == _FRAME_HEADER_L

        bus = MockI2CBus(read_queue=[shifted_frame])
        cam = ESP32_S3_CAM(bus)

        data_len, frag_id, total_frag, recv_payload = cam.receive_frame()
        assert data_len == len(payload)
        assert recv_payload == payload

    def test_checksum_mismatch_raises(self):
        payload = b"CorruptedPayload"
        frame_bytes = bytearray(make_raw_i2c_frame(payload))
        # Corrupt the payload byte
        frame_bytes[8] ^= 0xFF

        bus = MockI2CBus(read_queue=[bytes(frame_bytes)] * 4)
        cam = ESP32_S3_CAM(bus)

        with pytest.raises(ESP32_S3_CAM_Error) as exc_info:
            cam.receive_frame(retry=1)
        assert exc_info.value.code == "invalid_checksum"


class TestPacketAssembly:
    """Test multi-frame packet reassembly."""

    def test_multi_fragment_assembly(self):
        p1 = b"Chunk-1-Data-"
        p2 = b"Chunk-2-Data-"
        p3 = b"Chunk-3-End!"

        f1 = make_raw_i2c_frame(p1, fragment_id=1, total_fragments=3)
        f2 = make_raw_i2c_frame(p2, fragment_id=2, total_fragments=3)
        f3 = make_raw_i2c_frame(p3, fragment_id=3, total_fragments=3)

        bus = MockI2CBus(read_queue=[f1, f2, f3])
        cam = ESP32_S3_CAM(bus)

        packet = cam.receive_packet()
        assert packet == p1 + p2 + p3


class TestVisionTargetParsers:
    """Test higher-level computer vision packet parsers."""

    def test_parse_color_result(self):
        # 1 block: total=1, index=0 (red), x=120, y=100, w=50, h=60
        # Format: count (1B), [index(1B), x(2B), y(2B), w(2B), h(2B)] = 9B
        data = bytearray([1])
        data.extend([0])  # index = 0
        data.extend([120, 0])  # x = 120
        data.extend([100, 0])  # y = 100
        data.extend([50, 0])  # w = 50
        data.extend([60, 0])  # h = 60

        bus = MockI2CBus()
        cam = ESP32_S3_CAM(bus)
        # Mock _request_packet
        cam._request_packet = lambda cmd: bytes(data)

        res = cam.get_color_result()
        assert res["count"] == 1
        assert len(res["blocks"]) == 1
        b0 = res["blocks"][0]
        assert b0["index"] == 0
        assert b0["x"] == 120
        assert b0["y"] == 100
        assert b0["width"] == 50
        assert b0["height"] == 60

    def test_parse_face_detection(self):
        # 1 face: total=1, [index(1B), x(2B), y(2B), w(2B), h(2B)]
        data = bytearray([1])
        data.extend([1])  # index = 1
        data.extend([80, 0])  # x = 80
        data.extend([90, 0])  # y = 90
        data.extend([40, 0])  # w = 40
        data.extend([45, 0])  # h = 45

        bus = MockI2CBus()
        cam = ESP32_S3_CAM(bus)
        cam._request_packet = lambda cmd: bytes(data)

        res = cam.get_face_result()
        assert res["total_count"] == 1
        assert res["parsed_count"] == 1
        item = res["items"][0]
        assert item["x"] == 80
        assert item["y"] == 90
        assert item["width"] == 40
        assert item["height"] == 45

    def test_parse_qrcode_result(self):
        # QR Code format:
        # total (1B)
        # Record: index(1B), x(2B), y(2B), w(2B), h(2B), text_len(2B), text_bytes(text_len B)
        qr_text = "https://github.com/HanerQAQ/ESP32-S3-CAM"
        text_bytes = qr_text.encode("utf-8")
        text_len = len(text_bytes)

        data = bytearray([1])  # total = 1
        data.extend([1])  # index = 1
        data.extend([100, 0])  # x = 100
        data.extend([110, 0])  # y = 110
        data.extend([80, 0])  # w = 80
        data.extend([80, 0])  # h = 80
        data.extend([text_len & 0xFF, (text_len >> 8) & 0xFF])  # text_len
        data.extend(text_bytes)

        bus = MockI2CBus()
        cam = ESP32_S3_CAM(bus)
        cam._request_packet = lambda cmd: bytes(data)

        res = cam.get_qrcode_result()
        assert res["total_count"] == 1
        assert res["parsed_count"] == 1
        item = res["items"][0]
        assert item["text"] == qr_text
        assert item["x"] == 100
        assert item["y"] == 110
        assert item["width"] == 80
        assert item["height"] == 80

    def test_parse_apriltag_3d_pose(self):
        # 1 tag: total=1
        # Record (38B): id(2B), x(2B), y(2B), w(2B), h(2B), rot(4B float),
        #               x_trans(4B), y_trans(4B), z_trans(4B), x_rot(4B), y_rot(4B), z_rot(4B)
        data = bytearray([1])
        data.extend([5, 0])      # id = 5
        data.extend([160, 0])    # x = 160
        data.extend([120, 0])    # y = 120
        data.extend([48, 0])     # w = 48
        data.extend([48, 0])     # h = 48
        data.extend(struct.pack("<f", 45.0))   # rotation
        data.extend(struct.pack("<f", 0.12))   # x_trans
        data.extend(struct.pack("<f", -0.05))  # y_trans
        data.extend(struct.pack("<f", 0.85))   # z_trans
        data.extend(struct.pack("<f", 0.0))    # x_rot
        data.extend(struct.pack("<f", 0.0))    # y_rot
        data.extend(struct.pack("<f", 45.0))   # z_rot

        bus = MockI2CBus()
        cam = ESP32_S3_CAM(bus)
        cam._request_packet = lambda cmd: bytes(data)

        res = cam.get_apriltag_result()
        assert res["total_count"] == 1
        assert res["parsed_count"] == 1
        tag = res["items"][0]
        assert tag["id"] == 5
        assert tag["x"] == 160
        assert tag["y"] == 120
        assert tag["width"] == 48
        assert tag["height"] == 48
        assert abs(tag["z_translation"] - 0.85) < 1e-4

    def test_parse_pose_result_with_keypoints(self):
        # 1 person with 2 keypoints: total=1
        # Header (10B): index(1B), x(2B), y(2B), w(2B), h(2B), kp_count(1B)
        # Keypoints (5B each): index(1B), x(2B), y(2B)
        data = bytearray([1])    # total = 1
        data.extend([0])         # index = 0
        data.extend([100, 0])    # x = 100
        data.extend([80, 0])     # y = 80
        data.extend([60, 0])     # w = 60
        data.extend([120, 0])    # h = 120
        data.extend([2])         # kp_count = 2

        # KP 0 (Nose): index=0, x=130, y=90
        data.extend([0, 130, 0, 90, 0])
        # KP 1 (Left Eye): index=1, x=125, y=85
        data.extend([1, 125, 0, 85, 0])

        bus = MockI2CBus()
        cam = ESP32_S3_CAM(bus)
        cam._request_packet = lambda cmd: bytes(data)

        res = cam.get_pose_result()
        assert res["total_count"] == 1
        assert res["parsed_count"] == 1
        person = res["items"][0]
        assert person["x"] == 100
        assert person["y"] == 80
        assert person["keypoint_count"] == 2
        assert len(person["keypoints"]) == 2
        assert person["keypoints"][0]["x"] == 130
        assert person["keypoints"][1]["y"] == 85

    def test_parse_line_result(self):
        # Line format: detected(1B), start_x(2B), start_y(2B), end_x(2B), end_y(2B),
        #              angle_deg(4B float), offset(4B float) = 17 bytes
        data = bytearray([1])    # detected = 1
        data.extend([120, 0])    # start_x = 120
        data.extend([200, 0])    # start_y = 200
        data.extend([125, 0])    # end_x = 125
        data.extend([20, 0])     # end_y = 20
        data.extend(struct.pack("<f", -2.5))   # angle_deg = -2.5
        data.extend(struct.pack("<f", 5.0))    # offset = 5.0

        bus = MockI2CBus()
        cam = ESP32_S3_CAM(bus)
        cam._request_packet = lambda cmd: bytes(data)

        res = cam.get_line_result()
        assert res["detected"] is True
        assert res["start_x"] == 120
        assert res["start_y"] == 200
        assert abs(res["angle_deg"] - (-2.5)) < 1e-4
        assert abs(res["offset"] - 5.0) < 1e-4

    def test_face_register_and_count(self):
        bus = MockI2CBus()
        cam = ESP32_S3_CAM(bus)

        # 1. Face count response: total = 3
        cam._request_packet = lambda cmd: bytes([3])
        count = cam.get_face_count()
        assert count == 3

        # 2. Face register response: success=1, face_id=2
        cam._request_packet = lambda cmd: bytes([1, 2, 0])
        reg = cam.face_register()
        assert reg["success"] is True
        assert reg["face_id"] == 2

    def test_camera_orientation_controls(self):
        bus = MockI2CBus()
        cam = ESP32_S3_CAM(bus)

        cam._request_packet = lambda cmd: bytes([1])
        assert cam.set_hmirror(True) is True
        assert cam.set_vflip(False) is True

    def test_face_delete_success_and_failure(self):
        bus = MockI2CBus()
        cam = ESP32_S3_CAM(bus)

        sent_cmds = []
        def mock_request(cmd):
            sent_cmds.append(cmd)
            return bytes([1])

        cam._request_packet = mock_request
        assert cam.face_delete(1) is True
        assert '{"command":"face_delete","params":1}' in sent_cmds[0]

        cam._request_packet = lambda cmd: bytes([0])
        assert cam.face_delete(2) is False

        cam._request_packet = lambda cmd: bytes([])
        with pytest.raises(ESP32_S3_CAM_Error):
            cam.face_delete(0)

    def test_module_reset(self):
        bus = MockI2CBus()
        cam = ESP32_S3_CAM(bus)
        cam.module_reset()
        assert len(bus.written_data) == 1
        addr, data = bus.written_data[0]
        assert addr == ESP32_S3_CAM_I2C_ADDR
        assert b'"command":"module_reset"' in data


