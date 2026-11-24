# -*- coding: utf-8 -*-
import os
import sys
import time

try:
    from esp32_s3_cam import ESP32_S3_CAM, ESP32_S3_CAM_Error, RawI2C
except ImportError:
    _SDK_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), "../..", "python_sdk"))
    if _SDK_PATH not in sys.path:
        sys.path.insert(0, _SDK_PATH)
    from esp32_s3_cam import ESP32_S3_CAM, ESP32_S3_CAM_Error, RawI2C

# I2C 从机地址 0x52
I2C_ADDR = 0x52


def parse_bus_arg():
    for i, arg in enumerate(sys.argv[1:], 1):
        if arg in ('--bus', '-b') and i < len(sys.argv) - 1:
            try:
                return int(sys.argv[i + 1])
            except ValueError:
                pass
        elif arg.isdigit():
            return int(arg)
    return None

def sleep_ms(ms):
    time.sleep(ms / 1000.0)

COLOR_NAMES = {1: "Red", 2: "Yellow", 3: "Green", 4: "Blue", 5: "Purple"}

def print_color_result(result):
    if result['count'] == 0:
        return
    print('color_count=%d' % result['count'])
    for block in result['blocks']:
        color_name = COLOR_NAMES.get(block['index'], "Unknown")
        print('  color_id=%d (%s) x=%d y=%d w=%d h=%d' % (
            block['index'], color_name, block['x'], block['y'], block['width'], block['height']))

def main():
    print("ESP32_S3_CAM Color Detection Demo")
    bus_arg = parse_bus_arg()
    with RawI2C(bus_arg) as i2c:
        print('[I2C] Connected to /dev/i2c-%d (Target Addr: 0x%02X)' % (i2c.bus, I2C_ADDR))
        llm = ESP32_S3_CAM(i2c, I2C_ADDR)
        sleep_ms(500)
        while True:
            try:
                result = llm.get_color_result()
                print_color_result(result)
            except Exception as e:
                print('get_color_result error:', e)
            sleep_ms(20)

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print('\n[Exit] User interrupted.')
