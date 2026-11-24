#include <Arduino.h>
#include <Wire.h>
#include "driver/i2c.h"

#include "ESP32_S3_CAM.h"

// I2C 硬件参数：设备地址 0x52、速率 100kHz
// 说明：标准 ESP32-WROOM-32 默认 I2C 引脚为 SDA=21, SCL=22；若使用特定扩展板可修改为 SDA=17, SCL=16
#define I2C_ADDR 0x52
#define I2C_SDA 21
#define I2C_SCL 22
#define I2C_PORT I2C_NUM_0
#define I2C_FREQ_HZ 100000U
#define I2C_BUFFER_SIZE 1024

static esp32_s3_cam_handle_t s_esp32_s3_cam;

static esp32_s3_cam_err_t app_i2c_write(const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    Wire.beginTransmission(I2C_ADDR);
    size_t written = Wire.write(data, len);
    uint8_t status = Wire.endTransmission(true);
    if (status != 0 || written != len) {
        return ESP32_S3_CAM_ERR_FAIL;
    }
    return ESP32_S3_CAM_OK;
}

static esp32_s3_cam_err_t app_i2c_read(uint8_t *data, size_t len, uint32_t timeout_ms)
{
    size_t received = Wire.requestFrom((uint8_t)I2C_ADDR, (size_t)len, (bool)true);
    if (received != len) {
        return ESP32_S3_CAM_ERR_FAIL;
    }
    for (size_t i = 0; i < len; i++) {
        if (!Wire.available()) {
            return ESP32_S3_CAM_ERR_FAIL;
        }
        data[i] = Wire.read();
    }
    return ESP32_S3_CAM_OK;
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Wire.setPins(I2C_SDA, I2C_SCL);
    Wire.begin();
    Wire.setClock(I2C_FREQ_HZ);

    esp32_s3_cam_init(&s_esp32_s3_cam);
    esp32_s3_cam_register_i2c_callbacks(&s_esp32_s3_cam, app_i2c_write, app_i2c_read);
    Serial.println("ESP32_S3_CAM I2C Master initialized (0x52, 100kHz).");
}

void loop()
{
    Serial.println("=== ESP32-S3 Image Transmission & Camera Setting Demo ===");
    Serial.println("Live Stream URL:   http://192.168.4.1:81/stream");
    Serial.println("Snapshot URL:      http://192.168.4.1/capture");

    bool success = false;
    Serial.println("1. Setting Horizontal Mirror ON...");
    esp32_s3_cam_err_t err = esp32_s3_cam_set_hmirror(&s_esp32_s3_cam, true, &success, 1000);
    Serial.printf("   hmirror ON result: err=%d, success=%d\n", (int)err, success ? 1 : 0);
    delay(1500);

    Serial.println("2. Setting Vertical Flip ON...");
    err = esp32_s3_cam_set_vflip(&s_esp32_s3_cam, true, &success, 1000);
    Serial.printf("   vflip ON result: err=%d, success=%d\n", (int)err, success ? 1 : 0);
    delay(1500);

    Serial.println("3. Restoring Normal Orientation (Mirror OFF, Flip OFF)...");
    esp32_s3_cam_set_hmirror(&s_esp32_s3_cam, false, &success, 1000);
    delay(1500);
    esp32_s3_cam_set_vflip(&s_esp32_s3_cam, false, &success, 1000);
    delay(2000);
}
