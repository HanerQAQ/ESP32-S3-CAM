/**
 * @file 01_cat_dog_detect_app.c
 * @brief STM32 HAL example for ESP32-S3-CAM visual module (Cat & Dog Detection).
 *
 * Demonstrates communicating with ESP32-S3-CAM over I2C (slave address 0x52, 100 kHz)
 * to retrieve and process vision results.
 *
 * Usage:
 *   - Bare-metal: Call app_init() once in main(), then call app_loop() inside while(1).
 *   - FreeRTOS / CMSIS-RTOS: Spawn a thread targeting app_task_entry(NULL).
 */

#include "ESP32_S3_CAM.h"
#include "ESP32_S3_CAM_port.h"

#if defined(STM32F4) || defined(STM32F1) || defined(STM32H7) || defined(STM32G4)
#include "main.h"
#endif

#ifdef HAL_I2C_MODULE_ENABLED
extern I2C_HandleTypeDef ESP32_S3_CAM_I2C_HANDLE;
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define ESP32_S3_CAM_DEMO_POLL_PERIOD_MS    200U

static esp32_s3_cam_handle_t s_esp32_s3_cam;
static bool s_esp32_s3_cam_ready = false;


static void app_delay_ms(uint32_t ms)
{
    esp32_s3_cam_port_delay_ms(ms);
}

// 扫描 I2C 总线上的从机设备并打印应答地址
static void print_i2c_scan(void)
{
    uint8_t found_count = 0;

    printf("I2C scan start\r\n");
    for (uint8_t address = 0x03; address <= 0x77; address++) {
#ifdef HAL_I2C_MODULE_ENABLED
        if (HAL_I2C_IsDeviceReady(&ESP32_S3_CAM_I2C_HANDLE, (uint16_t)(address << 1), 1, 10) == HAL_OK) {
            printf("I2C device found at 0x%02X\r\n", address);
            found_count++;
        }
#endif
    }

    if (found_count == 0U) {
        printf("I2C scan result: no device found (check pull-ups and wiring)\r\n");
    }
}

static void print_target_detection_result(const char *label,
                                          const esp32_s3_cam_target_detection_result_t *result)
{
    if (result->total_count == 0U) return;
    printf("%s_total=%u parsed=%u\r\n", label, result->total_count, result->parsed_count);
    for (uint8_t i = 0; i < result->parsed_count; i++) {
        const esp32_s3_cam_target_detection_item_t *item = &result->items[i];
        printf("  index=%u x=%u y=%u w=%u h=%u\r\n",
               item->index,
               item->x,
               item->y,
               item->width,
               item->height);
    }
}

static void run_enabled_esp32_s3_cam_demo(void)
{
    esp32_s3_cam_target_detection_result_t cat_dog_result;
    esp32_s3_cam_err_t ret = esp32_s3_cam_get_cat_dog_result(&s_esp32_s3_cam, &cat_dog_result, 1000);
    if (ret == ESP32_S3_CAM_OK) {
        print_target_detection_result("cat_dog", &cat_dog_result);
    } else {
        printf("esp32_s3_cam_get_cat_dog_result failed: %d\r\n", (int)ret);
    }
}

// 初始化 ESP32-S3-CAM 模组并验证连通性
void app_init(void)
{
    printf("======================================================\r\n");
    printf("  ESP32-S3-CAM STM32 HAL Demo: Cat & Dog Detection\r\n");
    printf("  Target I2C Address: 0x%02X (100 kHz)\r\n", ESP32_S3_CAM_I2C_ADDRESS);
    printf("======================================================\r\n");

    print_i2c_scan();

    s_esp32_s3_cam_ready = esp32_s3_cam_port_init(&s_esp32_s3_cam) &&
                           esp32_s3_cam_port_is_ready(100);
    printf("ESP32_S3_CAM init result: %s\r\n", s_esp32_s3_cam_ready ? "OK" : "FAIL");

}

// 周期性轮询与视觉结果读取（裸机主循环或 RTOS 线程主体）
void app_loop(void)
{
    static uint32_t last_retry_ms = 0;

    if (!s_esp32_s3_cam_ready) {
        uint32_t now = esp32_s3_cam_port_get_tick_ms();
        if ((now - last_retry_ms) >= 5000U) {
            last_retry_ms = now;
            s_esp32_s3_cam_ready = esp32_s3_cam_port_init(&s_esp32_s3_cam) &&
                                   esp32_s3_cam_port_is_ready(100);
            printf("ESP32_S3_CAM retry result: %s\r\n", s_esp32_s3_cam_ready ? "OK" : "FAIL");
        }
        app_delay_ms(100);
        return;
    }

    run_enabled_esp32_s3_cam_demo();
    app_delay_ms(ESP32_S3_CAM_DEMO_POLL_PERIOD_MS);
}

// FreeRTOS / CMSIS-RTOS2 任务入口（如使用多任务框架）
void app_task_entry(void *argument)
{
    (void)argument;
    app_init();
    for (;;) {
        app_loop();
    }
}
