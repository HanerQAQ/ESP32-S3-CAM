#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_myperipherals.h"
#include "esp_code_scanner.h"
#include "fb_gfx.h"
#include "my_usart.h"
#include "my_usart1_user.h"
#include "my_user_iic.h"

static QueueHandle_t xQueueFrameI = NULL;
static QueueHandle_t xQueueEvent = NULL;
static QueueHandle_t xQueueResult = NULL;
static QueueHandle_t xQueueFrameO = NULL;

static const char *TAG = "APP_CODE_SCANNER";

static void rgb_print(camera_fb_t *fb, uint32_t color, const char *str)
{
    int x = ((int)fb->width - (int)(strlen(str) * 14)) / 2;
    if (x < 2) x = 2;
    fb_gfx_print(fb, x, 10, color, str);
}

static int rgb_printf(camera_fb_t *fb, uint32_t color, const char *format, ...)
{
    char loc_buf[64];
    char *temp = loc_buf;
    int len;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    len = vsnprintf(loc_buf, sizeof(loc_buf), format, arg);
    va_end(copy);
    if (len >= sizeof(loc_buf))
    {
        temp = (char *)malloc(len + 1);
        if (temp == NULL)
        {
            return 0;
        }
    }
    vsnprintf(temp, len + 1, format, arg);
    va_end(arg);
    rgb_print(fb, color, temp);
    if (len > 64)
    {
        free(temp);
    }
    return len;
}

static void decode_task(void *arg)
{
    camera_fb_t *fb = NULL;
    int64_t time1, time2;
    uint8_t decode_massage[64] = {'\0'};
    char last_decoded_text[64] = {'\0'};
    int display_decay_count = 0;

    while (1)
    {
        if (xQueueReceive(xQueueFrameI, &fb, portMAX_DELAY))
        {
            // Drop queued old frames so processing uses the latest image / 清除队列中的旧帧，确保处理最新画面
            camera_fb_t *latest_fb = NULL;
            while (xQueueReceive(xQueueFrameI, &latest_fb, 0) == pdTRUE)
            {
                esp_camera_fb_return(fb);
                fb = latest_fb;
            }

            time1 = esp_timer_get_time();
            esp_image_scanner_t *esp_scn = esp_code_scanner_create();
            if (esp_scn) {
                esp_code_scanner_config_t config = {ESP_CODE_SCANNER_MODE_FAST, ESP_CODE_SCANNER_IMAGE_RGB565, fb->width, fb->height};
                esp_code_scanner_set_config(esp_scn, config);
                int decoded_num = esp_code_scanner_scan_image(esp_scn, fb->buf);

                if (decoded_num) {
                    esp_code_scanner_symbol_t result = esp_code_scanner_result(esp_scn);
                    time2 = esp_timer_get_time();

                    ESP_LOGI(TAG, "Decode time in %lld ms.", (time2 - time1) / 1000);
                    ESP_LOGI(TAG, "Decoded %s symbol \"%s\"\n", result.type_name, result.data);

                    strncpy(last_decoded_text, result.data, sizeof(last_decoded_text) - 1);
                    last_decoded_text[sizeof(last_decoded_text) - 1] = '\0';
                    display_decay_count = 15; // Keep the result visible for about 0.5–1 second to prevent a one-frame flicker / 保持结果显示约 0.5–1 秒，避免单帧闪烁

                    snprintf((char*)decode_massage, sizeof(decode_massage), "$%s#", result.data);
                    Uart_Send_Data(decode_massage, strlen((char*)decode_massage)); 
                    Uart1_Send_Data(decode_massage, strlen((char*)decode_massage));

                    i2c_proto_set_qr_result(fb->width / 4, fb->height / 4, fb->width / 2, fb->height / 2, result.data, (uint16_t)strlen(result.data));
                }
                esp_code_scanner_destroy(esp_scn);
            }

            if (display_decay_count > 0)
            {
                rgb_printf(fb, 0x07E0, "%s", last_decoded_text); // Draw the result in green on the image / 在图像上用绿色绘制识别结果
                display_decay_count--;
            }
            else
            {
                i2c_proto_clear_qr_result();
            }
            
            if (xQueueFrameO)
            {
                if (xQueueSend(xQueueFrameO, &fb, 0) != pdTRUE)
                {
                    camera_fb_t *old_frame = NULL;
                    if (xQueueReceive(xQueueFrameO, &old_frame, 0) == pdTRUE)
                    {
                        esp_camera_fb_return(old_frame);
                        xQueueSend(xQueueFrameO, &fb, 0);
                    }
                    else
                    {
                        esp_camera_fb_return(fb);
                    }
                }
            }
            else
            {
                esp_camera_fb_return(fb);
            }
        }
    }
}


void regsitig_decode(   const QueueHandle_t frame_i,
                        const QueueHandle_t event,
                        const QueueHandle_t result,
                        const QueueHandle_t frame_o)
{

    xQueueFrameI = frame_i;
    xQueueFrameO = frame_o;
    xQueueEvent = event;
    xQueueResult = result;

    // Run on Core 1 (priority 4) to leave Core 0 for Wi-Fi and TCP/IP processing and avoid send-buffer error 11 / 绑定到 Core 1（优先级 4），让 Core 0 专门处理 Wi-Fi 和 TCP/IP，避免发送缓冲区满及 error 11
    xTaskCreatePinnedToCore(decode_task, TAG, 8 * 1024, NULL, 4, NULL, 1);
}
