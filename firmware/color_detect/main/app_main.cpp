#include "s3_cam_camera.h"

#include "app_mywifi.h"

#include "app_myhttpd.hpp"

#include "app_mymdns.h"



#include "esp_log.h"

#include "driver/spi_common.h"

#include "esp_chip_info.h"

#include "esp_system.h"

#include "esp_flash.h"
#include "esp_rom_sys.h"



#include "my_usart.h"

#include "my_usart1_user.h"

#include "my_user_iic.h"

#include "mykey.h"

#include <cstring>



// AI recognition setup / AI 识别初始化

#include "s3_cam_color_detection.hpp" // Color detection / 颜色识别

#include "color_adjust_protocol.hpp"



static QueueHandle_t xQueueAIFrame = NULL;

static QueueHandle_t xQueueAIUSERFrame = NULL;
extern "C" void app_camera_set_user_queue(QueueHandle_t q);

static QueueHandle_t xQueuemyvirtualKey = NULL;



static const char TAG[] = "main_AI_version";

char Version[] = "AI_V1.5.2";

uint16_t wifi_Mode = 0; // Pure access point mode (192.168.4.1) / 纯 AP 模式（192.168.4.1）



// ==============================================================================

// Camera resolution configuration (supported sizes) / 摄像头分辨率配置（支持的分辨率）:

//   - FRAMESIZE_240X240 (240x240, 1:1 ratio; recommended for color detection AI and high FPS, matches demo) / 正方形画幅，颜色识别推荐分辨率，与 demo 一致

//   - FRAMESIZE_QVGA (320x240, standard 4:3; recommended for high frame rates) / 标准 4:3 画面，适合高帧率

//   - FRAMESIZE_CIF (400x296, wide view with smooth motion) / 宽幅画面，运动更流畅

//   - FRAMESIZE_HVGA (480x320, enhanced image quality and wider view) / 画质更细腻，视野更广

//   - FRAMESIZE_VGA (640x480, high definition with rich detail) / 高清画面，细节丰富

//   - FRAMESIZE_SVGA (800x600, maximum detail for high-resolution streaming) / 细节最丰富，适合高分辨率图传

// ==============================================================================

#define CAMERA_DEFAULT_FRAMESIZE    FRAMESIZE_QVGA

#define CAMERA_DEFAULT_PIXFORMAT    PIXFORMAT_RGB565

#define CAMERA_DEFAULT_FB_COUNT     3



// Current resolution and pixel format, shared with other modules / 当前分辨率和像素格式，供其他模块读取或配置

framesize_t g_camera_framesize = CAMERA_DEFAULT_FRAMESIZE;

pixformat_t g_camera_pixformat = CAMERA_DEFAULT_PIXFORMAT;

extern "C" void app_main(void)

{

        uint8_t ver_data[128] = "\0";



    xQueueAIFrame = xQueueCreate(2, sizeof(camera_fb_t *));

    xQueueAIUSERFrame = xQueueCreate(1, sizeof(camera_fb_t *));

    xQueuemyvirtualKey = xQueueCreate(1, sizeof(int *));



    // Initialize UART0 and UART1 / 初始化 UART0 和 UART1

        My_Uart_Init(xQueuemyvirtualKey);
    // My_Uart1_user_Init(xQueuemyvirtualKey);



    // Initialize the onboard blue LED on IO2 (active low) / 初始化 IO2 板载蓝色 LED（低电平点亮）

    gpio_config_t led_conf = {};

    led_conf.intr_type = GPIO_INTR_DISABLE;

    led_conf.mode = GPIO_MODE_OUTPUT;

    led_conf.pin_bit_mask = (1ULL << GPIO_NUM_2);

    led_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;

    led_conf.pull_up_en = GPIO_PULLUP_DISABLE;

    gpio_config(&led_conf);

    gpio_set_level(GPIO_NUM_2, 0); // Turn on the LED / 点亮 LED



    // Initialize the I2C slave (SDA=IO47, SCL=IO48, address=0x52) / 初始化 I2C 从机（SDA=IO47，SCL=IO48，地址 0x52）

        My_i2c_init(xQueuemyvirtualKey);
        i2c_proto_set_project_type(I2C_PROJ_COLOR);

        app_mywifi_main();

        color_adjust_init();

        my_register_camera(g_camera_pixformat, g_camera_framesize, CAMERA_DEFAULT_FB_COUNT, xQueueAIFrame);
        app_mymdns_main();

        app_camera_set_user_queue(xQueueAIUSERFrame);
    register_color_detection(xQueueAIFrame, xQueuemyvirtualKey, NULL, xQueueAIUSERFrame, false); 
        register_httpd(xQueueAIUSERFrame, NULL, true); 

        Key_Init(xQueuemyvirtualKey);
        



    // Send the version information / 发送版本信息
    sprintf((char*)ver_data, "ESP32-S3-CAM Board Ver:%s (Color Detection I2C:0x52)\r\n", Version);

    Uart_Send_Data(ver_data, strlen((char*)ver_data)); 

}

