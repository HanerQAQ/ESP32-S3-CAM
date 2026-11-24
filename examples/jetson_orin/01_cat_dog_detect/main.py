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

# 默认从机地址 0x52
I2C_ADDR = 0x52

def sleep_ms(ms):
    time.sleep(ms / 1000.0)

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

def print_target_detection_result(label, result):
    if result['total_count'] == 0:
        return
    print('%s_total=%d parsed=%d' % (label, result['total_count'], result['parsed_count']))
    for item in result['items']:
        tag = "Cat" if item['index'] == 1 else ("Dog" if item['index'] == 2 else ("Class_%d" % item['index']))
        print('  target_seq=%d (%s) x=%d y=%d w=%d h=%d' % (
            item['index'], tag, item['x'], item['y'], item['width'], item['height']))

def main():
    bus_arg = parse_bus_arg()
    print("=======================================================")
    print("  ESP32_S3_CAM Cat and Dog Detection Demo (Jetson Orin)")
    print("=======================================================")
    print("Jetson Orin 40-Pin Header: Pin 3 (SDA), Pin 5 (SCL), Pin 6 (GND)")

    try:
        with RawI2C(bus_arg) as i2c:
            print("[I2C] Connected to /dev/i2c-%d (Target Addr: 0x%02X)" % (i2c.bus, I2C_ADDR))
            llm = ESP32_S3_CAM(i2c, I2C_ADDR)
            sleep_ms(500)
            while True:
                try:
                    result = llm.get_cat_dog_result()
                    print_target_detection_result('cat_dog', result)
                except ESP32_S3_CAM_Error as e:
                    print('get_cat_dog_result error:', e)
                sleep_ms(20)
    except KeyboardInterrupt:
        print("\n[Exit] User interrupted.")

if __name__ == '__main__':
    main()
