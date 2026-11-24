#ifndef ESP32_S3_CAM_PORT_H_
#define ESP32_S3_CAM_PORT_H_

#include "ESP32_S3_CAM.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configurable STM32 HAL I2C Handle
 * 
 * Default is hi2c2. If your STM32 board uses hi2c1 (e.g. PB6/PB7),
 * define ESP32_S3_CAM_I2C_HANDLE as hi2c1 in your project compiler definitions
 * or override it here.
 */
#ifndef ESP32_S3_CAM_I2C_HANDLE
#define ESP32_S3_CAM_I2C_HANDLE hi2c2
#endif

#define ESP32_S3_CAM_I2C_ADDRESS       (0x52U)
#define ESP32_S3_CAM_I2C_ADDRESS_HAL   (ESP32_S3_CAM_I2C_ADDRESS << 1)
#define ESP32_S3_CAM_I2C_CLOCK_HZ      (100000U)
#define ESP32_S3_CAM_I2C_BUFFER_SIZE   (1024U)

void esp32_s3_cam_port_delay_ms(uint32_t ms);
uint32_t esp32_s3_cam_port_get_tick_ms(void);

bool esp32_s3_cam_port_init(esp32_s3_cam_handle_t *handle);
bool esp32_s3_cam_port_is_ready(uint32_t timeout_ms);

esp32_s3_cam_err_t esp32_s3_cam_port_i2c_write(const uint8_t *data, size_t len, uint32_t timeout_ms);
esp32_s3_cam_err_t esp32_s3_cam_port_i2c_read(uint8_t *data, size_t len, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
