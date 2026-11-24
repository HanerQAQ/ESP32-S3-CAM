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

static void print_qrcode_result(const esp32_s3_cam_qrcode_result_t *result)
{
    if (result->total_count == 0) return;
    Serial.print("qrcode_total=");
    Serial.print(result->total_count);
    Serial.print(" parsed=");
    Serial.println(result->parsed_count);

    for (uint8_t i = 0; i < result->parsed_count; i++) {
        const esp32_s3_cam_qrcode_item_t *item = &result->items[i];
        Serial.print("  index=");
        Serial.print(item->index);
        Serial.print(" text_len=");
        Serial.print(item->text_len);
        Serial.print(" text=");
        Serial.println(item->text);
    }
}

void loop()
{
    esp32_s3_cam_qrcode_result_t qrcode_result;
    esp32_s3_cam_err_t ret = esp32_s3_cam_get_qrcode_result(&s_esp32_s3_cam, &qrcode_result, 1000);
    if (ret == ESP32_S3_CAM_OK) {
        if (qrcode_result.total_count == 0) {
            Serial.println("[qrcode] OK, total=0 (No QR code detected)");
        } else {
            print_qrcode_result(&qrcode_result);
        }
    } else {
        Serial.print("esp32_s3_cam_get_qrcode_result failed: ");
        Serial.println((int)ret);
    }
    delay(500);
}
