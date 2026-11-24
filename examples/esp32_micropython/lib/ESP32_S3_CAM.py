import struct

try:
    import time
except ImportError:
    time = None

ESP32_S3_CAM_I2C_ADDR = 0x52
VISION_MODE_COLOR = 0
VISION_MODE_LINE = 1

ESP32_S3_CAM_MAX_COLOR_BLOCKS = 16
MAX_COLOR_BLOCKS = 16
MAX_TARGET_COLORS = 5
HAND_GESTURE_MAX_COUNT = 3
TARGET_DETECTION_MAX_COUNT = 5
YOLO11N_CLASS_MAX_ID = 79
POSE_MAX_COUNT = 3
POSE_KEYPOINT_COUNT = 17
APRILTAG_MAX_COUNT = 3
QRCODE_MAX_COUNT = 3
QRCODE_MAX_TEXT_LEN = 96
FACE_RECOGNITION_MAX_COUNT = 5
_FACE_RECORD_SIZE = 10
HAND_GESTURE_DISLIKE = 9

_FRAME_HEADER_H = 0xAA
_FRAME_HEADER_L = 0x55
_FRAME_HEADER_SIZE = 8
_FRAME_MAX_SIZE = 32
_MAX_FRAME_DATA_SIZE = 23
_PACKET_BUFFER_SIZE = 1024
_MAX_FRAGMENTS = 64
_COLOR_BLOCK_SIZE = 9
_LINE_RESULT_SIZE = 17
_HAND_GESTURE_RECORD_SIZE = 9
_TARGET_DETECTION_RECORD_SIZE = 9
_POSE_RECORD_HEADER_SIZE = 10
_POSE_KEYPOINT_RECORD_SIZE = 5
_APRILTAG_RECORD_SIZE = 38
_QRCODE_RECORD_HEADER_SIZE = 11

_COLOR_CONFIG_COMMAND = '{"command":"get_info","params":"color_config"}'
_COLOR_RESULT_COMMAND = '{"command":"get_info","params":"color_result"}'
_HAND_GESTURE_COMMAND = '{"command":"get_info","params":"hand_gesture"}'
_CAT_DOG_COMMAND = '{"command":"get_info","params":"cat_dog"}'
_CAT_COMMAND = '{"command":"get_info","params":"cat"}'
_DOG_COMMAND = '{"command":"get_info","params":"dog"}'
_PEDESTRIAN_COMMAND = '{"command":"get_info","params":"pedestrian"}'
_YOLO11N_COMMAND = '{"command":"get_info","params":"coco_detect"}'
_FACE_COMMAND = '{"command":"get_info","params":"human"}'
_POSE_COMMAND = '{"command":"get_info","params":"coco_pose"}'
_APRILTAG_COMMAND = '{"command":"get_info","params":"apriltag_recognition"}'
_QRCODE_COMMAND = '{"command":"get_info","params":"qrcode"}'
_FACE_RECOGNITION_COMMAND = '{"command":"get_info","params":"face_recognition"}'
_FACE_REGISTER_COMMAND = '{"command":"face_register"}'
_FACE_COUNT_COMMAND = '{"command":"get_info","params":"face_count"}'
_MODULE_RESET_COMMAND = '{"command":"module_reset"}'



class ESP32_S3_CAM_Error(Exception):
    def __init__(self, code, message):
        super().__init__(message)
        self.code = code
        self.message = message

    def __str__(self):
        return "%s(%s)" % (self.message, self.code)


def _sleep_ms(ms):
    if time is None:
        return
    if hasattr(time, 'sleep_ms'):
        time.sleep_ms(ms)
    else:
        time.sleep(ms / 1000.0)


def _checksum(data):
    value = 0
    for b in data:
        value ^= b
    return value & 0xFF


def _u16_le(data, offset):
    return data[offset] | (data[offset + 1] << 8)


def _float_le(data, offset):
    return struct.unpack('<f', bytes(data[offset:offset + 4]))[0]


class ESP32_S3_CAM:
    """ESP32_S3_CAM I2C 客户端。传入对象需提供 writeto(addr, bytes) 与 readfrom(addr, len)。"""

    def __init__(self, i2c, addr=ESP32_S3_CAM_I2C_ADDR):
        self.i2c = i2c
        self.addr = addr

    def send_json(self, json_text):
        """发送 JSON 文本，严格使用纯 UTF-8 字节，严禁添加末尾 \0 字节。"""
        if not json_text:
            raise ESP32_S3_CAM_Error('invalid_arg', 'JSON command is empty')
        data = json_text.encode('utf-8') if isinstance(json_text, str) else bytes(json_text)
        self.i2c.writeto(self.addr, data)

    def receive_frame(self, retry=3):
        """接收并校验单个 32 字节固定帧。支持首字节丢失容错与从机未就绪重试。"""
        for attempt in range(retry + 1):
            frame = bytes(self.i2c.readfrom(self.addr, _FRAME_MAX_SIZE))
            if len(frame) != _FRAME_MAX_SIZE:
                raise ESP32_S3_CAM_Error('invalid_size', 'I2C frame size is not 32 bytes')

            # 容错与重同步：若首字节 0xAA 偶发丢失（从机硬件 FIFO STOP/START 间隙被前置消耗），帧以 0x55 开头
            if frame[0] == _FRAME_HEADER_L:
                candidate = bytes([_FRAME_HEADER_H]) + frame[:_FRAME_MAX_SIZE - 1]
                c_data_len = _u16_le(candidate, 2)
                if 0 < c_data_len <= _MAX_FRAME_DATA_SIZE:
                    c_payload = candidate[_FRAME_HEADER_SIZE:_FRAME_HEADER_SIZE + c_data_len]
                    if _checksum(c_payload) == candidate[_FRAME_HEADER_SIZE + c_data_len]:
                        frame = candidate

            if frame[0] != _FRAME_HEADER_H or frame[1] != _FRAME_HEADER_L:
                sync_offset = frame.find(bytes([_FRAME_HEADER_H, _FRAME_HEADER_L]))
                if sync_offset > 0:
                    extra_bytes = bytes(self.i2c.readfrom(self.addr, sync_offset))
                    frame = frame[sync_offset:] + extra_bytes
                else:
                    if attempt < retry:
                        _sleep_ms(5)
                        continue
                    raise ESP32_S3_CAM_Error('invalid_response', 'invalid frame header')

            data_len = _u16_le(frame, 2)
            fragment_id = _u16_le(frame, 4)
            total_fragments = _u16_le(frame, 6)
            if data_len == 0 or data_len > _MAX_FRAME_DATA_SIZE:
                if attempt < retry:
                    _sleep_ms(5)
                    continue
                raise ESP32_S3_CAM_Error('invalid_size', 'invalid DATA length')
            if (fragment_id == 0 or total_fragments == 0 or
                    fragment_id > total_fragments or
                    fragment_id > _MAX_FRAGMENTS or total_fragments > _MAX_FRAGMENTS):
                if attempt < retry:
                    _sleep_ms(5)
                    continue
                raise ESP32_S3_CAM_Error('invalid_response', 'invalid fragment id')

            payload = frame[_FRAME_HEADER_SIZE:_FRAME_HEADER_SIZE + data_len]
            if _checksum(payload) != frame[_FRAME_HEADER_SIZE + data_len]:
                if attempt < retry:
                    _sleep_ms(5)
                    continue
                raise ESP32_S3_CAM_Error('invalid_checksum', 'frame checksum mismatch')
            return data_len, fragment_id, total_fragments, payload

    def receive_packet(self):
        """接收并拼装完整数据包。"""
        packet = bytearray()
        expected_fragment_id = 1
        total_fragments = 0
        while True:
            _, fragment_id, frame_total, payload = self.receive_frame()
            _sleep_ms(1)
            if fragment_id != expected_fragment_id:
                raise ESP32_S3_CAM_Error('invalid_response', 'fragment id is not continuous')
            if total_fragments == 0:
                total_fragments = frame_total
            elif frame_total != total_fragments:
                raise ESP32_S3_CAM_Error('invalid_response', 'total fragment count changed')
            packet.extend(payload)
            if len(packet) > _PACKET_BUFFER_SIZE:
                raise ESP32_S3_CAM_Error('invalid_size', 'packet buffer overflow')
            if expected_fragment_id >= total_fragments:
                break
            expected_fragment_id += 1
        return bytes(packet)

    def _request_packet(self, command):
        self.send_json(command)
        _sleep_ms(20)
        return self.receive_packet()

    def get_color_config(self):
        """获取当前视觉配置（颜色识别/巡线）。注册数量由返回包长减 1 计算。"""
        data = self._request_packet(_COLOR_CONFIG_COMMAND)
        if len(data) < 1:
            raise ESP32_S3_CAM_Error('invalid_size', 'color config packet too short')
        registered_color_count = len(data) - 1
        if data[0] > VISION_MODE_LINE or registered_color_count > MAX_TARGET_COLORS:
            raise ESP32_S3_CAM_Error('invalid_response', 'invalid color config')
        targets = list(data[1:])
        return {
            'mode': data[0],
            'registered_color_count': registered_color_count,
            'target_count': registered_color_count,
            'targets': targets,
        }

    def set_vision_mode(self, mode, targets):
        """设置视觉模式与目标颜色列表。协议字段名称固定为 target。"""
        if mode not in (VISION_MODE_COLOR, VISION_MODE_LINE):
            raise ESP32_S3_CAM_Error('invalid_arg', 'invalid vision mode')
        if not targets or len(targets) > MAX_TARGET_COLORS:
            raise ESP32_S3_CAM_Error('invalid_arg', 'invalid target list')
        if mode == VISION_MODE_LINE and len(targets) != 1:
            raise ESP32_S3_CAM_Error('invalid_arg', 'line mode accepts exactly one target color')
        target_text = ','.join([str(int(x)) for x in targets])
        command = '{"command":"vision_setting","params":{"mode":%d,"target":[%s]}}' % (int(mode), target_text)
        data = self._request_packet(command)
        if len(data) < 1:
            raise ESP32_S3_CAM_Error('invalid_size', 'vision setting response too short')
        if data[0] > 1:
            raise ESP32_S3_CAM_Error('invalid_response', 'invalid vision setting response')
        return data[0] == 1

    def get_color_result(self):
        """获取颜色识别结果，具备色块上限截断保护，防止异常返回。"""
        data = self._request_packet(_COLOR_RESULT_COMMAND)
        if len(data) < 1:
            raise ESP32_S3_CAM_Error('invalid_size', 'color result packet too short')
        total = data[0]
        if total == 0:
            return {'count': 0, 'blocks': []}
        available_blocks = (len(data) - 1) // _COLOR_BLOCK_SIZE if len(data) > 1 else 0
        count = min(total, available_blocks, MAX_COLOR_BLOCKS)
        blocks = []
        for i in range(count):
            off = 1 + i * _COLOR_BLOCK_SIZE
            blocks.append({
                'index': data[off],
                'x': _u16_le(data, off + 1),
                'y': _u16_le(data, off + 3),
                'width': _u16_le(data, off + 5),
                'height': _u16_le(data, off + 7)
            })
        return {'count': count, 'blocks': blocks}

    def get_line_result(self):
        """获取巡线结果。"""
        data = self._request_packet(_COLOR_RESULT_COMMAND)
        if len(data) < 1:
            raise ESP32_S3_CAM_Error('invalid_size', 'line result packet too short')
        if data[0] > 1:
            raise ESP32_S3_CAM_Error('invalid_response', 'invalid line detected flag')
        result = {'detected': data[0] == 1}
        if not result['detected']:
            return result
        if len(data) < _LINE_RESULT_SIZE:
            raise ESP32_S3_CAM_Error('invalid_size', 'invalid line result size')
        result.update({
            'start_x': _u16_le(data, 1),
            'start_y': _u16_le(data, 3),
            'end_x': _u16_le(data, 5),
            'end_y': _u16_le(data, 7),
            'angle_deg': _float_le(data, 9),
            'offset': _float_le(data, 13)
        })
        return result

    def get_hand_gesture_result(self):
        """获取手势识别结果。"""
        data = self._request_packet(_HAND_GESTURE_COMMAND)
        if len(data) < 1:
            raise ESP32_S3_CAM_Error('invalid_size', 'hand gesture packet too short')
        total = data[0]
        if total == 0:
            return {'total_count': 0, 'parsed_count': 0, 'items': []}
        if len(data) < 1 + total * _HAND_GESTURE_RECORD_SIZE:
            raise ESP32_S3_CAM_Error('invalid_size', 'invalid hand gesture result size')
        parsed = min(total, HAND_GESTURE_MAX_COUNT)
        items = []
        for i in range(parsed):
            off = 1 + i * _HAND_GESTURE_RECORD_SIZE
            if data[off] > HAND_GESTURE_DISLIKE:
                raise ESP32_S3_CAM_Error('invalid_response', 'invalid hand gesture id')
            items.append({
                'gesture': data[off],
                'x': _u16_le(data, off + 1),
                'y': _u16_le(data, off + 3),
                'width': _u16_le(data, off + 5),
                'height': _u16_le(data, off + 7)
            })
        return {'total_count': total, 'parsed_count': parsed, 'items': items}

    def _parse_target_detection_data(self, data, check_yolo=False):
        if len(data) < 1:
            raise ESP32_S3_CAM_Error('invalid_size', 'target packet too short')
        total = data[0]
        if total == 0:
            return {'total_count': 0, 'parsed_count': 0, 'items': []}
        if len(data) < 1 + total * _TARGET_DETECTION_RECORD_SIZE:
            raise ESP32_S3_CAM_Error('invalid_size', 'invalid target result size')
        parsed = min(total, TARGET_DETECTION_MAX_COUNT)
        items = []
        for i in range(parsed):
            off = 1 + i * _TARGET_DETECTION_RECORD_SIZE
            index = data[off]
            if check_yolo and index > YOLO11N_CLASS_MAX_ID:
                raise ESP32_S3_CAM_Error('invalid_response', 'invalid YOLO11n class id')
            items.append({
                'index': index,
                'x': _u16_le(data, off + 1),
                'y': _u16_le(data, off + 3),
                'width': _u16_le(data, off + 5),
                'height': _u16_le(data, off + 7)
            })
        return {'total_count': total, 'parsed_count': parsed, 'items': items}

    def get_cat_dog_result(self):
        data = self._request_packet(_CAT_DOG_COMMAND)
        return self._parse_target_detection_data(data)

    def get_cat_result(self):
        return self._parse_target_detection_data(self._request_packet(_CAT_COMMAND))

    def get_dog_result(self):
        return self._parse_target_detection_data(self._request_packet(_DOG_COMMAND))

    def get_pedestrian_result(self):
        return self._parse_target_detection_data(self._request_packet(_PEDESTRIAN_COMMAND))

    def get_yolo11n_result(self):
        return self._parse_target_detection_data(self._request_packet(_YOLO11N_COMMAND), True)

    def get_face_result(self):
        return self._parse_target_detection_data(self._request_packet(_FACE_COMMAND))

    def get_pose_result(self):
        data = self._request_packet(_POSE_COMMAND)
        if len(data) < 1:
            raise ESP32_S3_CAM_Error('invalid_size', 'pose packet too short')
        total = data[0]
        if total == 0:
            return {'total_count': 0, 'parsed_count': 0, 'items': []}
        parsed = min(total, POSE_MAX_COUNT)
        items = []
        off = 1
        for i in range(total):
            if len(data) - off < _POSE_RECORD_HEADER_SIZE:
                raise ESP32_S3_CAM_Error('invalid_size', 'invalid pose record header size')
            kp_count = data[off + 9]
            record_size = _POSE_RECORD_HEADER_SIZE + kp_count * _POSE_KEYPOINT_RECORD_SIZE
            if kp_count > POSE_KEYPOINT_COUNT or len(data) - off < record_size:
                raise ESP32_S3_CAM_Error('invalid_size', 'invalid pose record size')
            if i < parsed:
                keypoints = []
                for k in range(kp_count):
                    ko = off + 10 + k * _POSE_KEYPOINT_RECORD_SIZE
                    keypoints.append({
                        'index': data[ko],
                        'x': _u16_le(data, ko + 1),
                        'y': _u16_le(data, ko + 3)
                    })
                items.append({
                    'index': data[off],
                    'x': _u16_le(data, off + 1),
                    'y': _u16_le(data, off + 3),
                    'width': _u16_le(data, off + 5),
                    'height': _u16_le(data, off + 7),
                    'keypoint_count': kp_count,
                    'keypoints': keypoints
                })
            off += record_size
        return {'total_count': total, 'parsed_count': parsed, 'items': items}

    def get_apriltag_result(self):
        data = self._request_packet(_APRILTAG_COMMAND)
        if len(data) < 1:
            raise ESP32_S3_CAM_Error('invalid_size', 'AprilTag packet too short')
        total = data[0]
        if total == 0:
            return {'total_count': 0, 'parsed_count': 0, 'items': []}
        if len(data) < 1 + total * _APRILTAG_RECORD_SIZE:
            raise ESP32_S3_CAM_Error('invalid_size', 'invalid AprilTag result size')
        parsed = min(total, APRILTAG_MAX_COUNT)
        items = []
        for i in range(parsed):
            off = 1 + i * _APRILTAG_RECORD_SIZE
            items.append({
                'id': _u16_le(data, off),
                'x': _u16_le(data, off + 2),
                'y': _u16_le(data, off + 4),
                'width': _u16_le(data, off + 6),
                'height': _u16_le(data, off + 8),
                'rotation': _float_le(data, off + 10),
                'x_translation': _float_le(data, off + 14),
                'y_translation': _float_le(data, off + 18),
                'z_translation': _float_le(data, off + 22),
                'x_rotation': _float_le(data, off + 26),
                'y_rotation': _float_le(data, off + 30),
                'z_rotation': _float_le(data, off + 34)
            })
        return {'total_count': total, 'parsed_count': parsed, 'items': items}

    def get_qrcode_result(self):
        data = self._request_packet(_QRCODE_COMMAND)
        if len(data) < 1:
            raise ESP32_S3_CAM_Error('invalid_size', 'QR code packet too short')
        total = data[0]
        if total == 0:
            return {'total_count': 0, 'parsed_count': 0, 'items': []}
        parsed = min(total, QRCODE_MAX_COUNT)
        items = []
        off = 1
        for i in range(total):
            if len(data) - off < _QRCODE_RECORD_HEADER_SIZE:
                raise ESP32_S3_CAM_Error('invalid_size', 'invalid QR code record header size')
            text_len = _u16_le(data, off + 9)
            record_size = _QRCODE_RECORD_HEADER_SIZE + text_len
            if len(data) - off < record_size:
                raise ESP32_S3_CAM_Error('invalid_size', 'invalid QR code record size')
            if i < parsed:
                text_bytes = data[off + _QRCODE_RECORD_HEADER_SIZE:off + _QRCODE_RECORD_HEADER_SIZE + text_len]
                stored_text = text_bytes[:QRCODE_MAX_TEXT_LEN]
                try:
                    text = stored_text.decode('utf-8')
                except Exception:
                    text = ''.join([chr(b) if 32 <= b <= 126 else '?' for b in stored_text])
                items.append({
                    'index': data[off],
                    'x': _u16_le(data, off + 1),
                    'y': _u16_le(data, off + 3),
                    'width': _u16_le(data, off + 5),
                    'height': _u16_le(data, off + 7),
                    'text_len': text_len,
                    'text': text
                })
            off += record_size
        return {'total_count': total, 'parsed_count': parsed, 'items': items}


    def set_hmirror(self, enabled):
        cmd = '{"command":"vision_setting","params":{"hmirror":%d}}' % (1 if enabled else 0)
        data = self._request_packet(cmd)
        if len(data) < 1:
            raise ESP32_S3_CAM_Error('invalid_size', 'hmirror response too short')
        return data[0] == 1

    def set_vflip(self, enabled):
        cmd = '{"command":"vision_setting","params":{"vflip":%d}}' % (1 if enabled else 0)
        data = self._request_packet(cmd)
        if len(data) < 1:
            raise ESP32_S3_CAM_Error('invalid_size', 'vflip response too short')
        return data[0] == 1

    def module_reset(self):
        self.send_json(_MODULE_RESET_COMMAND)

    def get_face_recognition_result(self):
        data = self._request_packet(_FACE_RECOGNITION_COMMAND)
        if len(data) < 1:
            raise ESP32_S3_CAM_Error('invalid_size', 'face recognition packet too short')
        total = data[0]
        if total == 0:
            return {'total_count': 0, 'parsed_count': 0, 'items': []}
        if len(data) < 1 + total * _FACE_RECORD_SIZE:
            raise ESP32_S3_CAM_Error('invalid_size', 'invalid face recognition payload size')
        parsed = min(total, FACE_RECOGNITION_MAX_COUNT)
        items = []
        for i in range(parsed):
            base = 1 + i * _FACE_RECORD_SIZE
            items.append({
                'id': _u16_le(data, base),
                'face_id': _u16_le(data, base),
                'x': _u16_le(data, base + 2),
                'y': _u16_le(data, base + 4),
                'width': _u16_le(data, base + 6),
                'height': _u16_le(data, base + 8)
            })
        return {'total_count': total, 'parsed_count': parsed, 'items': items}

    def face_register(self):
        data = self._request_packet(_FACE_REGISTER_COMMAND)
        if len(data) < 1:
            raise ESP32_S3_CAM_Error('invalid_size', 'face register response too short')
        success = (data[0] == 1)
        face_id = _u16_le(data, 1) if (success and len(data) >= 3) else 0
        return {'success': success, 'face_id': face_id}

    def face_delete(self, face_id=0):
        cmd = '{"command":"face_delete","params":%d}' % int(face_id)
        data = self._request_packet(cmd)
        if len(data) < 1:
            raise ESP32_S3_CAM_Error('invalid_size', 'face delete response too short')
        return data[0] == 1

    def get_face_count(self):
        data = self._request_packet(_FACE_COUNT_COMMAND)
        if len(data) < 1:
            raise ESP32_S3_CAM_Error('invalid_size', 'face count response too short')
        return data[0]


class RawI2C:
    """
    Linux / Jetson Orin / 树莓派 原始 I2C 驱动类。
    通过 Linux 内核 /dev/i2c-X 的 I2C_RDWR ioctl 实现纯二进制字节直发直收，接口兼容 MicroPython machine.I2C。
    特别针对 NVIDIA Jetson Orin (Orin Nano / NX / AGX Orin) 与 树莓派 40-Pin 扩展引脚优化：
    - 支持总线号智能自动探测（优先探测 Jetson Orin 40-pin 默认的 7，以及 1, 8, 0）。
    - 支持环境变量 ESP32_S3_CAM_I2C_BUS 或 CLI 参数自定义指定。
    - 提供完善的权限提示与未找到设备异常处理。
    - 提供从机地址扫描 (scan) 与存在性探测 (probe) 功能。
    """
    I2C_RDWR = 0x0707
    I2C_M_RD = 0x0001
    DEFAULT_BUS_CANDIDATES = (7, 1, 8, 0)

    def __init__(self, bus=None):
        import os
        self._os = os
        self._fd = None
        self.bus = None

        # 1. 确定 I2C 总线编号
        if bus is not None:
            if isinstance(bus, str) and bus.startswith('/dev/i2c-'):
                bus = int(bus.split('-')[-1])
            self.bus = int(bus)
        else:
            # 优先从环境变量读取
            env_bus = os.environ.get('ESP32_S3_CAM_I2C_BUS')
            if env_bus is not None:
                try:
                    self.bus = int(env_bus)
                except ValueError:
                    pass

            # 智能探测可用总线候选
            if self.bus is None:
                self.bus = self._detect_best_bus()

        # 2. 打开设备节点
        dev_path = '/dev/i2c-%d' % self.bus
        try:
            self._fd = os.open(dev_path, os.O_RDWR)
        except PermissionError:
            raise ESP32_S3_CAM_Error('permission_denied',
                "无法访问 %s：权限不足 (Permission denied)。\n"
                "请将当前用户添加至 i2c 用户组: sudo usermod -aG i2c $USER (需重新登录生效)，\n"
                "或者以 sudo 权限运行当前脚本。" % dev_path)
        except FileNotFoundError:
            raise ESP32_S3_CAM_Error('bus_not_found',
                "未找到 I2C 设备节点 %s！\n"
                "Jetson Orin 40-Pin 引脚 3(SDA) 和 5(SCL) 通常对应 /dev/i2c-7 (部分载板为 /dev/i2c-1)。\n"
                "请运行 'ls /dev/i2c*' 查看当前系统启用的 I2C 总线，并通过 '--bus <N>' 或环境变量 'ESP32_S3_CAM_I2C_BUS' 指定。" % dev_path)
        except Exception as e:
            raise ESP32_S3_CAM_Error('open_failed', "打开 %s 失败: %s" % (dev_path, str(e)))

    @classmethod
    def _detect_best_bus(cls):
        """根据系统内存在的 /dev/i2c-* 设备节点按优先级自动选取 (7 -> 1 -> 8 -> 0)"""
        import os
        for candidate in cls.DEFAULT_BUS_CANDIDATES:
            if os.path.exists('/dev/i2c-%d' % candidate):
                return candidate
        # 若未发现预设候选节点，返回 Jetson Orin 40-pin 推荐默认总线 7
        return 7

    def close(self):
        if self._fd is not None:
            self._os.close(self._fd)
            self._fd = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()

    def _i2c_rdwr(self, addr, flags, payload_or_len):
        import ctypes
        import fcntl

        class I2CMsg(ctypes.Structure):
            _fields_ = [
                ('addr', ctypes.c_uint16),
                ('flags', ctypes.c_uint16),
                ('len', ctypes.c_uint16),
                ('buf', ctypes.POINTER(ctypes.c_uint8)),
            ]

        class I2CRdwrIoctlData(ctypes.Structure):
            _fields_ = [
                ('msgs', ctypes.POINTER(I2CMsg)),
                ('nmsgs', ctypes.c_uint32),
            ]

        if flags & self.I2C_M_RD:
            buf = (ctypes.c_uint8 * int(payload_or_len))()
        else:
            payload = bytes(payload_or_len)
            buf = (ctypes.c_uint8 * len(payload)).from_buffer_copy(payload)

        msg = I2CMsg(addr, flags, len(buf), buf)
        ioctl_data = I2CRdwrIoctlData(ctypes.pointer(msg), 1)
        fcntl.ioctl(self._fd, self.I2C_RDWR, ioctl_data)
        if flags & self.I2C_M_RD:
            return bytes(buf)
        return len(buf)

    def writeto(self, addr, data):
        return self._i2c_rdwr(addr, 0, data)

    def readfrom(self, addr, length):
        return self._i2c_rdwr(addr, self.I2C_M_RD, length)

    def scan(self):
        """扫描当前总线上可应答的 7-bit 从机地址列表 (0x08 ~ 0x77)"""
        found = []
        for addr in range(0x08, 0x78):
            try:
                self._i2c_rdwr(addr, 0, b'')
                found.append(addr)
            except Exception:
                pass
        return found

    def probe(self, addr=ESP32_S3_CAM_I2C_ADDR):
        """探测指定从机地址是否存在 ACK 应答"""
        try:
            self._i2c_rdwr(addr, 0, b'')
            return True
        except Exception:
            return False
