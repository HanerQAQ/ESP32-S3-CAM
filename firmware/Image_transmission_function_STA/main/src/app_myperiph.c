#include "app_myperiph.h"
#include "app_mywifi.h"
#include "s3_cam_camera.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <unistd.h>

static const char *TAG = "app_myperiph";

/* ESP32-S3 CAM: BOOT strapping pin, active low */
#define BOOT_BUTTON_GPIO       GPIO_NUM_0
#define BOOT_LONG_PRESS_MS     1000
#define BOOT_DEBOUNCE_MS       30

typedef struct {
    gpio_num_t pin;
    int raw;
    int stable;
    int64_t edge_ms;
    int64_t pressed_ms;
    bool long_handled;
} boot_btn_t;

static boot_btn_t s_btn_boot = { .pin = BOOT_BUTTON_GPIO, .raw = 1, .stable = 1 };
static volatile int s_boot_level = 1;
static volatile uint32_t s_boot_press_count = 0;

static void camera_size(uint32_t *w, uint32_t *h)
{
    if (app_camera_get_resolution() == FRAMESIZE_VGA) {
        *w = 640;
        *h = 480;
    } else {
        *w = 320;
        *h = 240;
    }
}

static void boot_enter_provisioning(const char *why)
{
    ESP_LOGI(TAG, "BOOT %s: disconnecting STA and entering AP provisioning", why);
    esp_err_t err = app_mywifi_enter_provisioning();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Provisioning mode request failed: %s", esp_err_to_name(err));
    }
}

static void print_system_info_banner(void)
{
    uint32_t w, h;
    camera_size(&w, &h);
    bool connected = app_mywifi_is_sta_connected();
    const char *web_ip = connected ? app_mywifi_get_sta_ip() : app_mywifi_get_ap_ip();

    printf("\r\n=======================================================\r\n"
           " [ESP32-S3-CAM System Status]\r\n"
           " SoftAP:            %s (SSID: %s, IP: http://%s)\r\n"
           " Config URL:        http://%s/wifi\r\n"
           " STA Wi-Fi State:   %s\r\n"
           " STA Router SSID:   %s\r\n"
           " STA IP Address:    %s\r\n"
           " Camera Web UI:     http://%s/\r\n"
           " MJPEG Stream URL:  http://%s:81/stream\r\n"
           " Snapshot URL:      http://%s/capture\r\n"
           " Camera Resolution: %lu x %lu (%lu frames)\r\n"
           "=======================================================\r\n\r\n",
           app_mywifi_is_ap_active() ? "ON" : "OFF (hold BOOT to provision)",
           app_mywifi_get_ap_ssid(), app_mywifi_get_ap_ip(),
           app_mywifi_is_ap_active() ? app_mywifi_get_ap_ip() : web_ip,
           app_mywifi_get_disconnect_message(),
           app_mywifi_get_sta_ssid(),
           connected ? app_mywifi_get_sta_ip() : "None",
           web_ip, web_ip, web_ip,
           (unsigned long)w, (unsigned long)h,
           (unsigned long)app_camera_get_frame_count());
}

static void boot_button_poll(boot_btn_t *btn)
{
    int64_t now_ms = esp_timer_get_time() / 1000;
    int level = gpio_get_level(btn->pin);
    s_boot_level = level;

    if (level != btn->raw) {
        btn->raw = level;
        btn->edge_ms = now_ms;
    }
    if (btn->stable != btn->raw && now_ms - btn->edge_ms >= BOOT_DEBOUNCE_MS) {
        btn->stable = btn->raw;
        if (btn->stable == 0) {
            btn->pressed_ms = now_ms;
            btn->long_handled = false;
            s_boot_press_count++;
            ESP_LOGI(TAG, "BOOT GPIO%d pressed", btn->pin);
        } else {
            ESP_LOGI(TAG, "BOOT GPIO%d released after %lld ms", btn->pin,
                     (long long)(now_ms - btn->pressed_ms));
            if (!btn->long_handled) {
                printf("key_down!! (short press: status only)\r\n");
                print_system_info_banner();
            }
        }
    }
    if (btn->stable == 0 && !btn->long_handled &&
        now_ms - btn->pressed_ms >= BOOT_LONG_PRESS_MS) {
        btn->long_handled = true;
        printf("key_long_down!! \r\n");
        boot_enter_provisioning("long press");
    }
}

static void boot_button_task(void *arg)
{
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&btn_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BOOT GPIO config failed: %s", esp_err_to_name(err));
    }

    s_boot_level = gpio_get_level(BOOT_BUTTON_GPIO);
    ESP_LOGI(TAG, "BOOT input: GPIO%d=%d (active low); short press=status, 1s hold=provisioning",
             BOOT_BUTTON_GPIO, s_boot_level);

    while (1) {
        boot_button_poll(&s_btn_boot);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void process_uart_command(char *cmd)
{
    while (*cmd == ' ' || *cmd == '\t' || *cmd == '\r' || *cmd == '\n') cmd++;
    int len = strlen(cmd);
    while (len > 0 && (cmd[len - 1] == ' ' || cmd[len - 1] == '\t' ||
                       cmd[len - 1] == '\r' || cmd[len - 1] == '\n')) {
        cmd[--len] = '\0';
    }
    if (len == 0) return;

    if (strcasecmp(cmd, "sta_ip") == 0) {
        if (app_mywifi_is_sta_connected()) {
            printf("sta_ip:%s\r\n", app_mywifi_get_sta_ip());
        } else {
            printf("sta_ip:null\r\n");
        }
    } else if (strcasecmp(cmd, "ap_ip") == 0) {
        printf("ap_ip:%s\r\n", app_mywifi_get_ap_ip());
    } else if (strcasecmp(cmd, "status") == 0) {
        uint32_t w, h;
        camera_size(&w, &h);
        printf("{\"board\":\"ESP32-S3\",\"sta_connected\":%s,\"sta_ip\":\"%s\",\"ap_active\":%s,"
               "\"ap_ip\":\"%s\",\"ap_ssid\":\"%s\",\"w\":%lu,\"h\":%lu,\"frames\":%lu}\r\n",
               app_mywifi_is_sta_connected() ? "true" : "false",
               app_mywifi_get_sta_ip(),
               app_mywifi_is_ap_active() ? "true" : "false",
               app_mywifi_get_ap_ip(),
               app_mywifi_get_ap_ssid(),
               (unsigned long)w, (unsigned long)h,
               (unsigned long)app_camera_get_frame_count());
    } else if (strcasecmp(cmd, "reset_wifi") == 0) {
        printf("Clearing Wi-Fi credentials from NVS and restarting...\r\n");
        app_mywifi_clear_sta_creds();
        vTaskDelay(pdMS_TO_TICKS(300));
        esp_restart();
    } else if (strcasecmp(cmd, "restore_ap") == 0) {
        printf("Restoring SoftAP (STA stays connected)...\r\n");
        app_mywifi_restore_ap();
    } else if (strcasecmp(cmd, "provision_ap") == 0) {
        printf("Entering provisioning mode (STA disconnected, saved creds kept)...\r\n");
        app_mywifi_enter_provisioning();
    } else if (strcasecmp(cmd, "boot") == 0) {
        printf("boot_gpio:%d level:%d presses:%u\r\n",
               BOOT_BUTTON_GPIO, s_boot_level, (unsigned)s_boot_press_count);
    } else if (strcasecmp(cmd, "reboot") == 0) {
        printf("Rebooting device...\r\n");
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_restart();
    } else if (strcasecmp(cmd, "help") == 0) {
        printf("\r\nAvailable UART Commands:\r\n"
               "  sta_ip       - Query Station IP address\r\n"
               "  ap_ip        - Query SoftAP IP address\r\n"
               "  status       - Query current JSON status\r\n"
               "  reset_wifi   - Clear saved Wi-Fi and reboot to AP\r\n"
               "  restore_ap   - Restore SoftAP (STA stays connected)\r\n"
               "  provision_ap - Disconnect STA and enter AP provisioning\r\n"
               "  boot         - Show BOOT input level / press count\r\n"
               "  reboot       - Restart board\r\n"
               "  help         - Show this command list\r\n\r\n");
    } else {
        printf("Unknown command: '%s'. Type 'help' for command list.\r\n", cmd);
    }
}

static void uart_cmd_task(void *arg)
{
    char line_buf[128];
    int line_idx = 0;

    int flags = fcntl(fileno(stdin), F_GETFL);
    fcntl(fileno(stdin), F_SETFL, flags | O_NONBLOCK);

    while (1) {
        int c = fgetc(stdin);
        if (c != EOF) {
            if (c == '\r' || c == '\n') {
                if (line_idx > 0) {
                    line_buf[line_idx] = '\0';
                    process_uart_command(line_buf);
                    line_idx = 0;
                }
            } else if (line_idx < sizeof(line_buf) - 1) {
                line_buf[line_idx++] = (char)c;
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

esp_err_t app_myperiph_init(void)
{
    BaseType_t ret1 = xTaskCreate(boot_button_task, "boot_btn_task", 3072, NULL, 4, NULL);
    BaseType_t ret2 = xTaskCreate(uart_cmd_task, "uart_cmd_task", 3072, NULL, 4, NULL);

    if (ret1 != pdPASS || ret2 != pdPASS) {
        ESP_LOGE(TAG, "Failed to create peripheral task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Peripherals initialized (BOOT GPIO0 + UART0 CLI)");
    return ESP_OK;
}
