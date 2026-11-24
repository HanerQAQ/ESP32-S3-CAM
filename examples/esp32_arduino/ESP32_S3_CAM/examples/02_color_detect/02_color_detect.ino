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

static const char *get_color_name(uint8_t idx)
{
    switch (idx) {
        case 1: return "Red";
        case 2: return "Yellow";
        case 3: return "Green";
        case 4: return "Blue";
        case 5: return "Purple";
        default: return "Unknown";
    }
}

static void print_color_result(const esp32_s3_cam_color_result_t *result)
{
    if (result->count == 0) return;
    Serial.print("color_count=");
    Serial.println(result->count);
    for (uint8_t i = 0; i < result->count; i++) {
        const esp32_s3_cam_color_block_t *block = &result->blocks[i];
        Serial.print("  color_id=");
        Serial.print(block->index);
        Serial.print(" (");
        Serial.print(get_color_name(block->index));
        Serial.print(") x=");
        Serial.print(block->x);
        Serial.print(" y=");
        Serial.print(block->y);
        Serial.print(" w=");
        Serial.print(block->width);
        Serial.print(" h=");
        Serial.println(block->height);
    }
}

void loop()
{
    esp32_s3_cam_color_result_t color_result;
    esp32_s3_cam_err_t ret = esp32_s3_cam_get_color_result(&s_esp32_s3_cam, &color_result, 1000);
    if (ret == ESP32_S3_CAM_OK) {
        if (color_result.count == 0) {
            Serial.println("[color_detect] OK, count=0 (No color detected)");
        } else {
            print_color_result(&color_result);
        }
    } else {
        Serial.print("esp32_s3_cam_get_color_result failed: ");
        Serial.println((int)ret);
    }
    delay(500);
}
