#include "s3_cam_camera.h"
#include "my_usart.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "s3_cam_camera";

static QueueHandle_t xQueueFrameO = NULL;
static SemaphoreHandle_t s_reconfig_mutex = NULL;
static volatile bool s_pause_capture = false;
static volatile bool s_is_paused = false;
static TaskHandle_t s_camera_task_handle = NULL;

static pixformat_t s_pixel_format = PIXFORMAT_RGB565;
static framesize_t s_current_framesize = FRAMESIZE_QVGA;
static uint8_t s_current_quality = 80;
static uint8_t s_fb_count = 3;
static QueueHandle_t s_user_queue = NULL;

void app_camera_set_user_queue(QueueHandle_t q)
{
    s_user_queue = q;
}

static void drain_queue(QueueHandle_t q)
{
    if (!q) return;
    camera_fb_t *fb = NULL;
    while (xQueueReceive(q, &fb, 0) == pdTRUE) {
        if (fb) esp_camera_fb_return(fb);
    }
}

static void task_process_handler(void *arg)
{
    while (true)
    {
        if (s_pause_capture)
        {
            s_is_paused = true;
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        s_is_paused = false;

        if (!xQueueFrameO)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        camera_fb_t *frame = esp_camera_fb_get();
        if (frame)
        {
            if (s_pause_capture)
            {
                // Discard and return immediately if paused while blocked on fb_get
                esp_camera_fb_return(frame);
                continue;
            }

            if (xQueueSend(xQueueFrameO, &frame, 0) != pdTRUE)
            {
                camera_fb_t *old_frame = NULL;
                if (xQueueReceive(xQueueFrameO, &old_frame, 0) == pdTRUE)
                {
                    esp_camera_fb_return(old_frame);
                    xQueueSend(xQueueFrameO, &frame, 0);
                }
                else
                {
                    esp_camera_fb_return(frame);
                }
            }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

static void fill_camera_config(camera_config_t *config,
                               const pixformat_t pixel_format,
                               const framesize_t frame_size,
                               const uint8_t fb_count)
{
    memset(config, 0, sizeof(camera_config_t));
    config->ledc_channel = LEDC_CHANNEL_0;
    config->ledc_timer = LEDC_TIMER_0;
    config->pin_d0 = CAMERA_PIN_D0;
    config->pin_d1 = CAMERA_PIN_D1;
    config->pin_d2 = CAMERA_PIN_D2;
    config->pin_d3 = CAMERA_PIN_D3;
    config->pin_d4 = CAMERA_PIN_D4;
    config->pin_d5 = CAMERA_PIN_D5;
    config->pin_d6 = CAMERA_PIN_D6;
    config->pin_d7 = CAMERA_PIN_D7;
    config->pin_xclk = CAMERA_PIN_XCLK;
    config->pin_pclk = CAMERA_PIN_PCLK;
    config->pin_vsync = CAMERA_PIN_VSYNC;
    config->pin_href = CAMERA_PIN_HREF;
    config->pin_sccb_sda = CAMERA_PIN_SIOD;
    config->pin_sscb_sda = CAMERA_PIN_SIOD;
    config->pin_sccb_scl = CAMERA_PIN_SIOC;
    config->pin_sscb_scl = CAMERA_PIN_SIOC;
    config->pin_pwdn = CAMERA_PIN_PWDN;
    config->pin_reset = CAMERA_PIN_RESET;
    config->xclk_freq_hz = XCLK_FREQ_HZ;
    config->pixel_format = pixel_format;
    config->frame_size = frame_size;
    config->jpeg_quality = 12;
    // Double-buffering in PSRAM (300KB for QVGA, 1.2MB for VGA) provides maximum stability & lowest latency
    config->fb_count = fb_count;
    config->fb_location = CAMERA_FB_IN_PSRAM;
    config->grab_mode = CAMERA_GRAB_LATEST;
}

static void apply_sensor_settings(void)
{
    sensor_t *s = esp_camera_sensor_get();
    if (!s) return;

    if (s) {
        s->status.quality = s_current_quality;
    }

    if (s->id.PID == OV3660_PID || s->id.PID == OV2640_PID) {
        s->set_vflip(s, 1);
    } else if (s->id.PID == GC0308_PID) {
        s->set_hmirror(s, 0);
    } else if (s->id.PID == GC032A_PID) {
        s->set_vflip(s, 1);
    }

    if (s->id.PID == OV3660_PID) {
        s->set_brightness(s, 1);
        s->set_saturation(s, -2);
    }

    if (s->id.PID == GC2145_PID && s->set_reg) {
        // Completely disable all GC2145 hardware edge sharpening & edge algorithms for direct native output
        // 1. Page 0: Disable Edge Enhancement in Block_enable1 and Block_enable3
        s->set_reg(s, 0xfe, 0xff, 0x00); // Switch to Page 0
        s->set_reg(s, 0x80, 0x10, 0x00); // P0:0x80 bit[4] EE_en = 0 (Hardware Edge Enhancement OFF)
        s->set_reg(s, 0x82, 0x40, 0x00); // P0:0x82 bit[6] auto_EE = 0 (Auto Edge Enhancement OFF)
        s->set_reg(s, 0x87, 0x01, 0x00); // P0:0x87 bit[0] auto_edge_effect = 0

        // 2. Page 2: Completely zero out INTPEE edge modes, edge effect gains and scalers
        s->set_reg(s, 0xfe, 0xff, 0x02); // Switch to Page 2 (ISP: INTPEE & DNDD)
        s->set_reg(s, 0x90, 0xff, 0x00); // P2:0x90 EEINTP mode 1 = 0 (All edge modes, HP3, LP disabled)
        s->set_reg(s, 0x91, 0xff, 0x00); // P2:0x91 EEINTP mode 2 = 0
        s->set_reg(s, 0x96, 0xff, 0x00); // P2:0x96 edge level = 0
        s->set_reg(s, 0x97, 0xff, 0x00); // P2:0x97 Edge1/Edge2 effect = 0 (Zero edge gain)
        s->set_reg(s, 0x98, 0xff, 0x00); // P2:0x98 Edge pos/neg ratio = 0
        s->set_reg(s, 0x9d, 0xff, 0x00); // P2:0x9d Edge effect scaler = 0
        s->set_reg(s, 0xfe, 0xff, 0x00); // Return to Page 0
    }
}

void my_register_camera(const pixformat_t pixel_fromat,
                        const framesize_t frame_size,
                        const uint8_t fb_count,
                        const QueueHandle_t frame_o)
{
    ESP_LOGI(TAG, "Camera module is %s", CAMERA_MODULE_NAME);

    if (!s_reconfig_mutex) {
        s_reconfig_mutex = xSemaphoreCreateMutex();
    }

#if CONFIG_CAMERA_MODULE_ESP_EYE || CONFIG_CAMERA_MODULE_ESP32_CAM_BOARD
    gpio_config_t conf;
    conf.mode = GPIO_MODE_INPUT;
    conf.pull_up_en = GPIO_PULLUP_ENABLE;
    conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    conf.intr_type = GPIO_INTR_DISABLE;
    conf.pin_bit_mask = 1LL << 13;
    gpio_config(&conf);
    conf.pin_bit_mask = 1LL << 14;
    gpio_config(&conf);
#endif

    s_pixel_format = pixel_fromat;
    s_current_framesize = frame_size;
    s_fb_count = fb_count;
    xQueueFrameO = frame_o;

    camera_config_t config;
    fill_camera_config(&config, pixel_fromat, frame_size, fb_count);

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return;
    }

    apply_sensor_settings();

    xTaskCreatePinnedToCore(task_process_handler, TAG, 4 * 1024, NULL, 6, &s_camera_task_handle, 1);
}

esp_err_t app_camera_set_resolution(framesize_t frame_size)
{
    if (frame_size != FRAMESIZE_QVGA && frame_size != FRAMESIZE_VGA) {
        ESP_LOGE(TAG, "Unsupported framesize: %d (only QVGA 320x240 and VGA 640x480 supported)", (int)frame_size);
        return ESP_ERR_INVALID_ARG;
    }

    if (frame_size == s_current_framesize) {
        ESP_LOGI(TAG, "Camera resolution already %d, nothing to do", (int)frame_size);
        return ESP_OK;
    }

    if (!s_reconfig_mutex) {
        s_reconfig_mutex = xSemaphoreCreateMutex();
    }
    if (xSemaphoreTake(s_reconfig_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire reconfig mutex");
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "Switching camera resolution from %d to %d...", (int)s_current_framesize, (int)frame_size);

    framesize_t old_size = s_current_framesize;

    // 1. Pause the capture task (never vTaskDelete it: it may hold a frame or be blocked inside the driver)
    s_pause_capture = true;
    for (int i = 0; i < 100 && !s_is_paused; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // 2. Drain all downstream queues, give in-flight consumers (AI, HTTP JPEG encode) time to finish, drain again
    for (int round = 0; round < 2; round++) {
        drain_queue(xQueueFrameO);
        drain_queue(s_user_queue);
        vTaskDelay(pdMS_TO_TICKS(round == 0 ? 350 : 50));
    }

    // 3. Deinitialize old driver (frees DMA and old PSRAM framebuffers)
    esp_camera_deinit();
    vTaskDelay(pdMS_TO_TICKS(50));

    // 4. Reinitialize with the new resolution; retry, then roll back (with retries) so we never end up black
    camera_config_t config;
    esp_err_t err = ESP_FAIL;
    for (int i = 0; i < 3 && err != ESP_OK; i++) {
        fill_camera_config(&config, s_pixel_format, frame_size, s_fb_count);
        err = esp_camera_init(&config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_camera_init failed at resolution %d (0x%x), retry %d", (int)frame_size, err, i);
            esp_camera_deinit();
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    if (err != ESP_OK) {
        frame_size = old_size;
        for (int i = 0; i < 5 && err != ESP_OK; i++) {
            fill_camera_config(&config, s_pixel_format, old_size, s_fb_count);
            err = esp_camera_init(&config);
            if (err != ESP_OK) {
                esp_camera_deinit();
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Camera rollback failed, restarting to recover");
            esp_restart();
        }
    }

    apply_sensor_settings();
    s_current_framesize = frame_size;

    // 5. Resume capture
    s_pause_capture = false;

    ESP_LOGI(TAG, "Camera resolution successfully switched to %d (%s)",
             (int)frame_size, (frame_size == FRAMESIZE_VGA ? "640x480" : "320x240"));

    xSemaphoreGive(s_reconfig_mutex);
    return ESP_OK;
}

framesize_t app_camera_get_resolution(void)
{
    return s_current_framesize;
}

esp_err_t app_camera_set_quality(uint8_t quality)
{
    if (quality < 1) quality = 1;
    if (quality > 100) quality = 100;
    s_current_quality = quality;
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        if (s->set_quality) {
            s->set_quality(s, quality);
        }
        s->status.quality = quality;
    }
    ESP_LOGI(TAG, "Camera quality set to %u", (unsigned)quality);
    return ESP_OK;
}

uint8_t app_camera_get_quality(void)
{
    return s_current_quality;
}
