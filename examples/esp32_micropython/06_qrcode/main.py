# -*- coding: utf-8 -*-
from machine import Pin, I2C
from time import sleep_ms
import sys
for _p in ('/lib', '..', '../lib'):
    if _p not in sys.path:
        sys.path.append(_p)
try:
    from ESP32_S3_CAM import *
except ImportError:
    from lib.ESP32_S3_CAM import *

# I2C 硬件参数：设备地址 0x52、速率 100kHz
# 说明：标准 ESP32 默认 I2C 引脚为 SDA=21, SCL=22；若使用特定扩展板可修改为 SDA=17, SCL=16
I2C_ADDR = 0x52
I2C_SDA = 21
I2C_SCL = 22
I2C_FREQ_HZ = 100000

i2c = I2C(0, scl=Pin(I2C_SCL), sda=Pin(I2C_SDA), freq=I2C_FREQ_HZ)
llm = ESP32_S3_CAM(i2c, I2C_ADDR)
sleep_ms(500)

def print_qrcode_result(result):
    if result['total_count'] == 0:
        return
    print('qrcode_total=%d parsed=%d' % (result['total_count'], result['parsed_count']))
    for item in result['items']:
        print('  index=%d text_len=%d text=%s' % (
            item['index'], item['text_len'], item['text']))

def main():
    print("ESP32_S3_CAM QR Code Detection Demo (MicroPython)")
    while True:
        try:
            result = llm.get_qrcode_result()
            print_qrcode_result(result)
        except Exception as e:
            print('get_qrcode_result error:', e)
        sleep_ms(50)

if __name__ == '__main__':
    main()



