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

def main():
    print("=== ESP32-S3 Image Transmission & Camera Setting Demo (MicroPython) ===")
    print("Camera Stream URL:   http://192.168.4.1:81/stream")
    print("Camera Snapshot URL: http://192.168.4.1/capture")

    while True:
        print("Setting Horizontal Mirror ON...")
        llm.set_hmirror(True)
        sleep_ms(2000)

        print("Setting Vertical Flip ON...")
        llm.set_vflip(True)
        sleep_ms(2000)

        print("Restoring Normal Orientation (Mirror OFF, Flip OFF)...")
        llm.set_hmirror(False)
        sleep_ms(100)
        llm.set_vflip(False)
        sleep_ms(3000)

if __name__ == '__main__':
    main()
