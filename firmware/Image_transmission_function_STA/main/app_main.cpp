#include "s3_cam_camera.h"
#include "app_myhttpd.hpp"
#include "network_service.h"
#include "app_myperiph.h"

#include "esp_log.h"
#include "driver/spi_common.h"
#include "esp_chip_info.h"
#include "esp_system.h"
#include "esp_flash.h"
#include "nvs_flash.h"

#include "my_usart.h"
#include "my_user_iic.h"
#include <cstring>

static QueueHandle_t xQueueAIFrame = NULL;
static QueueHandle_t xQueueAIUSERFrame = NULL;
static QueueHandle_t xQueuemyvirtualKey = NULL;

extern "C" void app_camera_set_user_queue(QueueHandle_t q);
static const char TAG[] = "main_AI_version";
char Version[] = "AI_V1.5.2";
uint16_t wifi_Mode = 1; // Station (STA) mode with initial AP provisioning

// ==============================================================================
// Camera resolution configuration / 摄像头分辨率配置。
// Runtime switching (web UI / API) supports two sizes / 运行时可切换的分辨率只有两种:
//   - FRAMESIZE_QVGA (320x240, default; higher frame rate) / 默认，帧率更高
//   - FRAMESIZE_VGA  (640x480, higher detail)              / 细节更丰富
// ==============================================================================
#define CAMERA_DEFAULT_FRAMESIZE    FRAMESIZE_QVGA
#define CAMERA_DEFAULT_PIXFORMAT    PIXFORMAT_RGB565
#define CAMERA_DEFAULT_FB_COUNT     3

static_assert(CAMERA_DEFAULT_FRAMESIZE == FRAMESIZE_QVGA || CAMERA_DEFAULT_FRAMESIZE == FRAMESIZE_VGA,
              "CAMERA_DEFAULT_FRAMESIZE must match a runtime-switchable resolution (QVGA or VGA)");

framesize_t g_camera_framesize = CAMERA_DEFAULT_FRAMESIZE;
pixformat_t g_camera_pixformat = CAMERA_DEFAULT_PIXFORMAT;

extern "C" void app_main(void)
{
    uint8_t ver_data[128] = "\0";

    // NVS is required by the Wi-Fi credential store and the mDNS hostname config
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ret);

    // Depth 1 keeps one latest frame for HTTP consumers, leaving the driver enough
    // framebuffers to keep DMA running even when nobody is streaming.
    xQueueAIFrame = xQueueCreate(1, sizeof(camera_fb_t *));
    xQueueAIUSERFrame = xQueueCreate(1, sizeof(camera_fb_t *));
    xQueuemyvirtualKey = xQueueCreate(1, sizeof(int *));

    // Initialize the onboard blue LED on IO2 (active low)
    gpio_config_t led_conf = {};
    led_conf.intr_type = GPIO_INTR_DISABLE;
    led_conf.mode = GPIO_MODE_OUTPUT;
    led_conf.pin_bit_mask = (1ULL << GPIO_NUM_2);
    led_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    led_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&led_conf);
    gpio_set_level(GPIO_NUM_2, 0); // Turn on the LED

    // Initialize the I2C slave (SDA=IO47, SCL=IO48, address=0x52)
    My_i2c_init(xQueuemyvirtualKey);
    i2c_proto_set_project_type(I2C_PROJ_IMAGE_TRANS);

    // Network (netif + Wi-Fi) first, matching the working Camera_Display reference
    ESP_ERROR_CHECK(network_service_start());

    // Camera after Wi-Fi, before mDNS/HTTP
    my_register_camera(g_camera_pixformat, g_camera_framesize, CAMERA_DEFAULT_FB_COUNT, xQueueAIFrame);
    app_camera_set_user_queue(xQueueAIUSERFrame);

    // Start the image streaming & web provisioning service
    register_httpd(xQueueAIFrame, NULL, true);

    // BOOT button (short press = status, 1s hold = AP provisioning) + UART CLI
    ESP_ERROR_CHECK(app_myperiph_init());

    // Initialize UART0 & send version information
    sprintf((char*)ver_data, "ESP32-S3-CAM Board Ver:%s (Image Transmission STA I2C:0x52)\r\n", Version);
    Uart_Send_Data(ver_data, strlen((char*)ver_data));
}
