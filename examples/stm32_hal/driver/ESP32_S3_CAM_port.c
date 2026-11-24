#include "ESP32_S3_CAM_port.h"
#include "i2c.h"
#include "main.h"
#include <stddef.h>

extern I2C_HandleTypeDef ESP32_S3_CAM_I2C_HANDLE;

void esp32_s3_cam_port_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

uint32_t esp32_s3_cam_port_get_tick_ms(void)
{
    return HAL_GetTick();
}

static esp32_s3_cam_err_t hal_i2c_status_to_esp32_s3_cam(HAL_StatusTypeDef status)
{
    if (status == HAL_OK) {
        return ESP32_S3_CAM_OK;
    }

    if (status == HAL_TIMEOUT) {
        return ESP32_S3_CAM_ERR_TIMEOUT;
    }

    return ESP32_S3_CAM_ERR_FAIL;
}

static void recover_i2c_if_needed(void)
{
    if (HAL_I2C_GetError(&ESP32_S3_CAM_I2C_HANDLE) != HAL_I2C_ERROR_NONE) {
        HAL_I2C_DeInit(&ESP32_S3_CAM_I2C_HANDLE);
        HAL_I2C_Init(&ESP32_S3_CAM_I2C_HANDLE);
    }
}

static bool configure_i2c_clock(uint32_t clock_speed)
{
    if (ESP32_S3_CAM_I2C_HANDLE.Init.ClockSpeed == clock_speed) {
        return true;
    }

    if (HAL_I2C_GetState(&ESP32_S3_CAM_I2C_HANDLE) != HAL_I2C_STATE_READY) {
        return false;
    }

    if (HAL_I2C_DeInit(&ESP32_S3_CAM_I2C_HANDLE) != HAL_OK) {
        return false;
    }

    ESP32_S3_CAM_I2C_HANDLE.Init.ClockSpeed = clock_speed;
    if (HAL_I2C_Init(&ESP32_S3_CAM_I2C_HANDLE) != HAL_OK) {
        return false;
    }

    esp32_s3_cam_port_delay_ms(2);
    return true;
}

bool esp32_s3_cam_port_is_ready(uint32_t timeout_ms)
{
    return HAL_I2C_IsDeviceReady(&ESP32_S3_CAM_I2C_HANDLE,
                                 ESP32_S3_CAM_I2C_ADDRESS_HAL,
                                 3,
                                 timeout_ms) == HAL_OK;
}

bool esp32_s3_cam_port_init(esp32_s3_cam_handle_t *handle)
{
    if (handle == NULL) {
        return false;
    }

    if (esp32_s3_cam_init(handle) != ESP32_S3_CAM_OK) {
        return false;
    }

    if (!configure_i2c_clock(ESP32_S3_CAM_I2C_CLOCK_HZ)) {
        return false;
    }

    return esp32_s3_cam_register_i2c_callbacks(handle,
                                            esp32_s3_cam_port_i2c_write,
                                            esp32_s3_cam_port_i2c_read) == ESP32_S3_CAM_OK;
}

esp32_s3_cam_err_t esp32_s3_cam_port_i2c_write(const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    size_t offset = 0;
    uint32_t timeout = (timeout_ms == 0U) ? HAL_MAX_DELAY : timeout_ms;

    if (data == NULL || len == 0U) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    while (offset < len) {
        size_t chunk_len = len - offset;
        HAL_StatusTypeDef status;

        if (chunk_len > ESP32_S3_CAM_I2C_BUFFER_SIZE) {
            chunk_len = ESP32_S3_CAM_I2C_BUFFER_SIZE;
        }

        status = HAL_I2C_Master_Transmit(&ESP32_S3_CAM_I2C_HANDLE,
                                         ESP32_S3_CAM_I2C_ADDRESS_HAL,
                                         (uint8_t *)&data[offset],
                                         (uint16_t)chunk_len,
                                         timeout);
        if (status != HAL_OK) {
            recover_i2c_if_needed();
            return hal_i2c_status_to_esp32_s3_cam(status);
        }

        offset += chunk_len;
    }

    return ESP32_S3_CAM_OK;
}

esp32_s3_cam_err_t esp32_s3_cam_port_i2c_read(uint8_t *data, size_t len, uint32_t timeout_ms)
{
    HAL_StatusTypeDef status;
    uint32_t timeout = (timeout_ms == 0U) ? HAL_MAX_DELAY : timeout_ms;

    if (data == NULL || len == 0U) {
        return ESP32_S3_CAM_ERR_INVALID_ARG;
    }

    if (len > ESP32_S3_CAM_I2C_BUFFER_SIZE || len > UINT16_MAX) {
        return ESP32_S3_CAM_ERR_INVALID_SIZE;
    }

    status = HAL_I2C_Master_Receive(&ESP32_S3_CAM_I2C_HANDLE,
                                    ESP32_S3_CAM_I2C_ADDRESS_HAL,
                                    data,
                                    (uint16_t)len,
                                    timeout);
    if (status != HAL_OK) {
        recover_i2c_if_needed();
        return hal_i2c_status_to_esp32_s3_cam(status);
    }

    return ESP32_S3_CAM_OK;
}
