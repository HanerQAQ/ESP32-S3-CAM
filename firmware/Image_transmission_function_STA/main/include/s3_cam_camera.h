#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_camera.h"

#if CONFIG_CAMERA_MODULE_WROVER_KIT
#define CAMERA_MODULE_NAME "Wrover Kit"
#define CAMERA_PIN_PWDN -1
#define CAMERA_PIN_RESET -1
#define CAMERA_PIN_XCLK 21
#define CAMERA_PIN_SIOD 26
#define CAMERA_PIN_SIOC 27

#define CAMERA_PIN_D7 35
#define CAMERA_PIN_D6 34
#define CAMERA_PIN_D5 39
#define CAMERA_PIN_D4 36
#define CAMERA_PIN_D3 19
#define CAMERA_PIN_D2 18
#define CAMERA_PIN_D1 5
#define CAMERA_PIN_D0 4
#define CAMERA_PIN_VSYNC 25
#define CAMERA_PIN_HREF 23
#define CAMERA_PIN_PCLK 22

#elif CONFIG_CAMERA_MODULE_ESP_EYE
#define CAMERA_MODULE_NAME "ESP-EYE"
#define CAMERA_PIN_PWDN -1
#define CAMERA_PIN_RESET -1
#define CAMERA_PIN_XCLK 4
#define CAMERA_PIN_SIOD 18
#define CAMERA_PIN_SIOC 23

#define CAMERA_PIN_D7 36
#define CAMERA_PIN_D6 37
#define CAMERA_PIN_D5 38
#define CAMERA_PIN_D4 39
#define CAMERA_PIN_D3 35
#define CAMERA_PIN_D2 14
#define CAMERA_PIN_D1 13
#define CAMERA_PIN_D0 34
#define CAMERA_PIN_VSYNC 5
#define CAMERA_PIN_HREF 27
#define CAMERA_PIN_PCLK 25

#elif CONFIG_CAMERA_MODULE_ESP_S2_KALUGA
#define CAMERA_MODULE_NAME "ESP-S2-KALUGA"
#define CAMERA_PIN_PWDN -1
#define CAMERA_PIN_RESET -1
#define CAMERA_PIN_XCLK 1
#define CAMERA_PIN_SIOD 8
#define CAMERA_PIN_SIOC 7

#define CAMERA_PIN_D7 38
#define CAMERA_PIN_D6 21
#define CAMERA_PIN_D5 40
#define CAMERA_PIN_D4 39
#define CAMERA_PIN_D3 42
#define CAMERA_PIN_D2 41
#define CAMERA_PIN_D1 37
#define CAMERA_PIN_D0 36
#define CAMERA_PIN_VSYNC 2
#define CAMERA_PIN_HREF 3
#define CAMERA_PIN_PCLK 33

#elif CONFIG_CAMERA_MODULE_ESP_S3_EYE
// ESP-S3-EYE native pin configuration / ESP-S3-EYE 原生引脚配置
#define CAMERA_MODULE_NAME "ESP-S3-EYE"
#define CAMERA_PIN_PWDN -1
#define CAMERA_PIN_RESET -1

#define CAMERA_PIN_VSYNC 6
#define CAMERA_PIN_HREF 7
#define CAMERA_PIN_PCLK 13
#define CAMERA_PIN_XCLK 15

#define CAMERA_PIN_SIOD 4
#define CAMERA_PIN_SIOC 5

#define CAMERA_PIN_D0 11
#define CAMERA_PIN_D1 9
#define CAMERA_PIN_D2 8
#define CAMERA_PIN_D3 10
#define CAMERA_PIN_D4 12
#define CAMERA_PIN_D5 18
#define CAMERA_PIN_D6 17
#define CAMERA_PIN_D7 16

#elif CONFIG_CAMERA_MODULE_ESP_S3_CAM
// Pin assignments follow the 4.SCH_S3-Cam V1.0 schematic and demo/camera_setting.h / 引脚配置依据 4.SCH_S3-Cam V1.0 原理图和 demo/camera_setting.h 修正
// PWDN通过R23(10k)硬件下拉到AGND(-1), RESET通过R22(10k)硬件上拉到DOVDD2.8V(-1)
// D0–D7 connect to DVP_Y2–DVP_Y9 (IO11, IO9, IO8, IO10, IO12, IO18, IO17, IO16) / D0–D7 分别连接到 DVP_Y2–DVP_Y9（IO11、IO9、IO8、IO10、IO12、IO18、IO17、IO16）
#define CAMERA_MODULE_NAME "ESP32-S3-CAM"
#define CAMERA_PIN_PWDN -1
#define CAMERA_PIN_RESET -1

#define CAMERA_PIN_VSYNC 6
#define CAMERA_PIN_HREF 7
#define CAMERA_PIN_PCLK 13
#define CAMERA_PIN_XCLK 15

#define CAMERA_PIN_SIOD 4
#define CAMERA_PIN_SIOC 5

#define CAMERA_PIN_D0 11
#define CAMERA_PIN_D1 9
#define CAMERA_PIN_D2 8
#define CAMERA_PIN_D3 10
#define CAMERA_PIN_D4 12
#define CAMERA_PIN_D5 18
#define CAMERA_PIN_D6 17
#define CAMERA_PIN_D7 16

#elif CONFIG_CAMERA_MODULE_ESP32_CAM_BOARD
#define CAMERA_MODULE_NAME "ESP-DEVCAM"
#define CAMERA_PIN_PWDN 32
#define CAMERA_PIN_RESET 33

#define CAMERA_PIN_XCLK 4
#define CAMERA_PIN_SIOD 18
#define CAMERA_PIN_SIOC 23

#define CAMERA_PIN_D7 36
#define CAMERA_PIN_D6 19
#define CAMERA_PIN_D5 21
#define CAMERA_PIN_D4 39
#define CAMERA_PIN_D3 35
#define CAMERA_PIN_D2 14
#define CAMERA_PIN_D1 13
#define CAMERA_PIN_D0 34
#define CAMERA_PIN_VSYNC 5
#define CAMERA_PIN_HREF 27
#define CAMERA_PIN_PCLK 25

#elif CONFIG_CAMERA_MODULE_M5STACK_PSRAM
#define CAMERA_MODULE_NAME "M5STACK-PSRAM"
#define CAMERA_PIN_PWDN -1
#define CAMERA_PIN_RESET 15

#define CAMERA_PIN_XCLK 27
#define CAMERA_PIN_SIOD 25
#define CAMERA_PIN_SIOC 23

#define CAMERA_PIN_D7 19
#define CAMERA_PIN_D6 36
#define CAMERA_PIN_D5 18
#define CAMERA_PIN_D4 39
#define CAMERA_PIN_D3 5
#define CAMERA_PIN_D2 34
#define CAMERA_PIN_D1 35
#define CAMERA_PIN_D0 32
#define CAMERA_PIN_VSYNC 22
#define CAMERA_PIN_HREF 26
#define CAMERA_PIN_PCLK 21

#elif CONFIG_CAMERA_MODULE_M5STACK_WIDE
#define CAMERA_MODULE_NAME "M5STACK-WIDE"
#define CAMERA_PIN_PWDN -1
#define CAMERA_PIN_RESET 15
#define CAMERA_PIN_XCLK 27
#define CAMERA_PIN_SIOD 22
#define CAMERA_PIN_SIOC 23

#define CAMERA_PIN_D7 19
#define CAMERA_PIN_D6 36
#define CAMERA_PIN_D5 18
#define CAMERA_PIN_D4 39
#define CAMERA_PIN_D3 5
#define CAMERA_PIN_D2 34
#define CAMERA_PIN_D1 35
#define CAMERA_PIN_D0 32
#define CAMERA_PIN_VSYNC 25
#define CAMERA_PIN_HREF 26
#define CAMERA_PIN_PCLK 21

#elif CONFIG_CAMERA_MODULE_AI_THINKER
#define CAMERA_MODULE_NAME "AI-THINKER"
#define CAMERA_PIN_PWDN 32
#define CAMERA_PIN_RESET -1
#define CAMERA_PIN_XCLK 0
#define CAMERA_PIN_SIOD 26
#define CAMERA_PIN_SIOC 27

#define CAMERA_PIN_D7 35
#define CAMERA_PIN_D6 34
#define CAMERA_PIN_D5 39
#define CAMERA_PIN_D4 36
#define CAMERA_PIN_D3 21
#define CAMERA_PIN_D2 19
#define CAMERA_PIN_D1 18
#define CAMERA_PIN_D0 5
#define CAMERA_PIN_VSYNC 25
#define CAMERA_PIN_HREF 23
#define CAMERA_PIN_PCLK 22

#elif CONFIG_CAMERA_MODULE_CUSTOM
#define CAMERA_MODULE_NAME "CUSTOM"
#define CAMERA_PIN_PWDN CONFIG_CAMERA_PIN_PWDN
#define CAMERA_PIN_RESET CONFIG_CAMERA_PIN_RESET
#define CAMERA_PIN_XCLK CONFIG_CAMERA_PIN_XCLK
#define CAMERA_PIN_SIOD CONFIG_CAMERA_PIN_SIOD
#define CAMERA_PIN_SIOC CONFIG_CAMERA_PIN_SIOC

#define CAMERA_PIN_D7 CONFIG_CAMERA_PIN_Y9
#define CAMERA_PIN_D6 CONFIG_CAMERA_PIN_Y8
#define CAMERA_PIN_D5 CONFIG_CAMERA_PIN_Y7
#define CAMERA_PIN_D4 CONFIG_CAMERA_PIN_Y6
#define CAMERA_PIN_D3 CONFIG_CAMERA_PIN_Y5
#define CAMERA_PIN_D2 CONFIG_CAMERA_PIN_Y4
#define CAMERA_PIN_D1 CONFIG_CAMERA_PIN_Y3
#define CAMERA_PIN_D0 CONFIG_CAMERA_PIN_Y2
#define CAMERA_PIN_VSYNC CONFIG_CAMERA_PIN_VSYNC
#define CAMERA_PIN_HREF CONFIG_CAMERA_PIN_HREF
#define CAMERA_PIN_PCLK CONFIG_CAMERA_PIN_PCLK
#endif


// Schematic LED1 (blue LED) is connected to IO2 / 原理图中的 LED1（蓝色 LED）连接到 IO2
#ifndef LED_GPIO_NUM
#define LED_GPIO_NUM 2
#endif
#ifndef BOARD_LED_PIN
#define BOARD_LED_PIN 2
#endif

#define XCLK_FREQ_HZ 20000000  // XCLK is set to 20 MHz (matching demo, sensor limit: 24 MHz) / XCLK 设置为 20 MHz（参考 demo 优化，传感器上限为 24 MHz）

#ifdef __cplusplus
extern "C"
{
#endif
    /**
     * @brief Initialize the camera / 初始化摄像头
     * 
     * @param pixformat    Supported pixel format / 支持的像素格式
     *                     - PIXFORMAT_RGB565
     *                     - PIXFORMAT_YUV422
     *                     - PIXFORMAT_GRAYSC
     *                     - PIXFORMAT_JPEG
     *                     - PIXFORMAT_RGB888
     *                     - PIXFORMAT_RAW
     *                     - PIXFORMAT_RGB444
     *                     - PIXFORMAT_RGB555
     * @param frame_size   Supported frame size / 支持的帧尺寸
     *                     - FRAMESIZE_96X96,    // 96x96 pixels / 96×96 像素
     *                     - FRAMESIZE_QQVGA,    // 160x120 pixels / 160×120 像素
     *                     - FRAMESIZE_QCIF,     // 176x144 pixels / 176×144 像素
     *                     - FRAMESIZE_HQVGA,    // 240x176 pixels / 240×176 像素
     *                     - FRAMESIZE_240X240,  // 240x240 pixels / 240×240 像素
     *                     - FRAMESIZE_QVGA,     // 320x240 pixels / 320×240 像素
     *                     - FRAMESIZE_CIF,      // 400x296 pixels / 400×296 像素
     *                     - FRAMESIZE_HVGA,     // 480x320 pixels / 480×320 像素
     *                     - FRAMESIZE_VGA,      // 640x480 pixels / 640×480 像素
     *                     - FRAMESIZE_SVGA,     // 800x600 pixels / 800×600 像素
     *                     - FRAMESIZE_XGA,      // 1024x768 pixels / 1024×768 像素
     *                     - FRAMESIZE_HD,       // 1280x720 pixels / 1280×720 像素
     *                     - FRAMESIZE_SXGA,     // 1280x1024 pixels / 1280×1024 像素
     *                     - FRAMESIZE_UXGA,     // 1600x1200 pixels / 1600×1200 像素
     *                     - FRAMESIZE_FHD,      // 1920x1080 pixels / 1920×1080 像素
     *                     - FRAMESIZE_P_HD,     // 720x1280 pixels / 720×1280 像素
     *                     - FRAMESIZE_P_3MP,    // 864x1536 pixels / 864×1536 像素
     *                     - FRAMESIZE_QXGA,     // 2048x1536 pixels / 2048×1536 像素
     *                     - FRAMESIZE_QHD,      // 2560x1440 pixels / 2560×1440 像素
     *                     - FRAMESIZE_WQXGA,    // 2560x1600 pixels / 2560×1600 像素
     *                     - FRAMESIZE_P_FHD,    // 1080x1920 pixels / 1080×1920 像素
     *                     - FRAMESIZE_QSXGA,    // 2560x1920 pixels / 2560×1920 像素
     * @param fb Frame buffer / 帧缓冲区_count     Number of frame buffers to be allocated. If more than one, then each frame will be acquired (double speed)
     */
    void my_register_camera(const pixformat_t pixel_fromat,
                         const framesize_t frame_size,
                         const uint8_t fb_count,
                         const QueueHandle_t frame_o);

    esp_err_t app_camera_set_resolution(framesize_t frame_size);
    framesize_t app_camera_get_resolution(void);

    // Serialize frame consumption with resolution switching / 帧使用与分辨率切换互斥
    void app_camera_frame_lock(void);
    void app_camera_frame_unlock(void);

    // Request on-demand capture; the capture task pauses while nobody consumes frames
    void app_camera_capture_acquire(void);
    void app_camera_capture_release(void);

    // Reinitialize the camera before serving a request if no frame arrived recently
    esp_err_t app_camera_ensure_ready(void);

    esp_err_t app_camera_set_quality(uint8_t quality);
    uint8_t app_camera_get_quality(void);
    uint32_t app_camera_get_frame_count(void);

#ifdef __cplusplus
}
#endif
