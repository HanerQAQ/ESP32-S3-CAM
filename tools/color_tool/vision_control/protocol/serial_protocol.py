from __future__ import annotations

import json
from dataclasses import dataclass
from typing import Any


FRAME_HEADER = b"\xaa\x55"
FUNC_IMAGE_CAPTURE = 0
FUNC_SETTINGS_READ = 1
FUNC_SETTINGS_SAVE = 2
MAX_FRAME_SIZE = 1024
FRAME_OVERHEAD_SIZE = 9
MAX_DATA_SIZE = MAX_FRAME_SIZE - FRAME_OVERHEAD_SIZE
MAX_PAYLOAD_SIZE = MAX_DATA_SIZE - 1


@dataclass(frozen=True)
class ProtocolFrame:
    fragment_id: int
    total_fragments: int
    function: int
    payload: bytes


class ProtocolError(ValueError):
    pass


def xor_checksum(data: bytes) -> int:
    checksum = 0
    for value in data:
        checksum ^= value
    return checksum


def build_frame(
    function: int,
    payload: bytes,
    fragment_id: int = 1,
    total_fragments: int = 1,
) -> bytes:
    if not 0 <= function <= 255:
        raise ProtocolError("协议功能号必须在 0 到 255 之间。")
    data = bytes((function,)) + payload
    if not 1 <= len(data) <= MAX_DATA_SIZE:
        raise ProtocolError(f"协议数据区长度必须在 1 到 {MAX_DATA_SIZE} 字节之间。")
    if fragment_id <= 0 or total_fragments <= 0 or fragment_id > total_fragments:
        raise ProtocolError("协议分片编号无效。")

    return b"".join(
        (
            FRAME_HEADER,
            len(data).to_bytes(2, "little"),
            fragment_id.to_bytes(2, "little"),
            total_fragments.to_bytes(2, "little"),
            data,
            bytes((xor_checksum(data),)),
        )
    )


def build_json_frame(function: int, command: str, params: Any) -> bytes:
    payload = json.dumps({"command": command, "params": params}, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    return build_frame(function, payload)


def build_json_payload(command: str, params: Any) -> bytes:
    return json.dumps({"command": command, "params": params}, ensure_ascii=False, separators=(",", ":")).encode("utf-8")


def build_json_command(command: str, params: Any) -> bytes:
    return build_json_payload(command, params)


def decode_json_payload(payload: bytes) -> dict[str, Any]:
    try:
        value = json.loads(payload.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ProtocolError(f"协议 JSON 数据解析失败：{exc}") from exc
    if not isinstance(value, dict):
        raise ProtocolError("协议 JSON 数据必须是对象。")
    return value


class SerialFrameParser:
    def __init__(self) -> None:
        self._buffer = bytearray()

    def feed(self, data: bytes) -> list[ProtocolFrame]:
        self._buffer.extend(data)
        frames = []

        while True:
            header_index = self._buffer.find(FRAME_HEADER)
            if header_index < 0:
                self._buffer.clear()
                break
            if header_index > 0:
                del self._buffer[:header_index]
            if len(self._buffer) < 9:
                break

            payload_length = int.from_bytes(self._buffer[2:4], "little")
            frame_length = 8 + payload_length + 1
            if payload_length <= 0:
                del self._buffer[0]
                continue
            if frame_length > MAX_FRAME_SIZE:
                del self._buffer[0]
                continue
            if len(self._buffer) < frame_length:
                break

            frame_data = bytes(self._buffer[:frame_length])
            del self._buffer[:frame_length]
            data_area = frame_data[8:-1]
            checksum = frame_data[-1]
            if xor_checksum(data_area) != checksum:
                continue

            fragment_id = int.from_bytes(frame_data[4:6], "little")
            total_fragments = int.from_bytes(frame_data[6:8], "little")
            if fragment_id <= 0 or total_fragments <= 0 or fragment_id > total_fragments:
                continue

            frames.append(
                ProtocolFrame(
                    fragment_id,
                    total_fragments,
                    data_area[0],
                    data_area[1:],
                )
            )
        return frames


class FragmentAssembler:
    def __init__(self) -> None:
        self._function = None
        self._total_fragments = 0
        self._chunks = {}

    def clear(self) -> None:
        self._function = None
        self._total_fragments = 0
        self._chunks.clear()

    def add(self, frame: ProtocolFrame) -> bytes | None:
        if (
            self._function is not None
            and (frame.function != self._function or frame.total_fragments != self._total_fragments)
        ):
            self.clear()
            raise ProtocolError("同一次业务的功能号或分片总数不一致。")

        self._function = frame.function
        self._total_fragments = frame.total_fragments
        self._chunks[frame.fragment_id] = frame.payload

        expected_ids = range(1, frame.total_fragments + 1)
        if not all(fragment_id in self._chunks for fragment_id in expected_ids):
            return None

        payload = b"".join(self._chunks[fragment_id] for fragment_id in expected_ids)
        self.clear()
        return payload
