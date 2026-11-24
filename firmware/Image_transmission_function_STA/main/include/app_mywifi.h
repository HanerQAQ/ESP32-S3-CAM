#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "sdkconfig.h"

// Default SoftAP settings. The SSID is suffixed with the last 3 MAC bytes:
// ESP32-S3-CAM-XXXXXX (uppercase hex).
#define AP_SSID_PREFIX       "ESP32-S3-CAM-"
#define AP_PASSWD_DEFAULT    ""
#define AP_IP_DEFAULT        "192.168.5.1"
#ifndef CONFIG_EXAMPLE_WIFI_AP_CHANNEL
#define CONFIG_EXAMPLE_WIFI_AP_CHANNEL 1
#endif
#define AP_CHANNEL_DEFAULT   CONFIG_EXAMPLE_WIFI_AP_CHANNEL
#define AP_CHANNEL_2G_DEFAULT 1

// Initialize Wi-Fi: STA-first with SoftAP provisioning on demand / on failure
void app_mywifi_main(void);

// Returns true if STA has successfully connected to router and obtained IP
bool app_mywifi_is_sta_connected(void);

// Returns true if STA is currently in the middle of connecting
bool app_mywifi_is_sta_connecting(void);

// Returns true while the provisioning / fallback SoftAP is enabled
bool app_mywifi_is_ap_active(void);

// Get allocated STA IP address string (e.g. "192.168.1.105")
const char* app_mywifi_get_sta_ip(void);

// Get current connected SSID
const char* app_mywifi_get_sta_ssid(void);

// Get SoftAP IP address string ("192.168.5.1")
const char* app_mywifi_get_ap_ip(void);

// Get SoftAP SSID string ("ESP32-S3-CAM-XXXXXX", last 3 AP MAC bytes)
const char* app_mywifi_get_ap_ssid(void);

// Get last disconnect reason code
int app_mywifi_get_disconnect_reason(void);

// Get human-readable disconnect / status message
const char* app_mywifi_get_disconnect_message(void);

// Trigger STA connection with given SSID and Password
esp_err_t app_mywifi_connect_sta(const char *ssid, const char *passwd);

// Connect using a specific scanned BSSID/channel
esp_err_t app_mywifi_connect_sta_ex(const char *ssid, const char *passwd,
                                    const uint8_t *bssid, uint8_t channel);

// Notify Wi-Fi manager that /connect/status delivered the connected STA IP to the client
void app_mywifi_notify_status_delivered(void);

// Restore SoftAP provisioning mode (e.g. BOOT long-press while in STA-only mode)
esp_err_t app_mywifi_restore_ap(void);

// Pre-align the provisioning AP to the channel of the SSID the user just
// selected, so the SoftAP restart happens while the password is being typed.
esp_err_t app_mywifi_prepare_ap_channel(uint8_t channel);

// Disconnect STA and enter AP-only provisioning; NVS credentials are kept for
// the one-click reconnect card in the provisioning page.
esp_err_t app_mywifi_enter_provisioning(void);

// Device hostname reported via DHCP (router client list).
const char *app_mywifi_get_device_hostname(void);

// Scan nearby Wi-Fi APs and return a newly allocated JSON string
// Caller is responsible for free()-ing the returned string
char* app_mywifi_scan_json(void);
void app_mywifi_scan_trigger(void);
bool app_mywifi_scan_running(void);

// NVS helpers
esp_err_t app_mywifi_save_sta_creds(const char *ssid, const char *passwd);
esp_err_t app_mywifi_load_sta_creds(char *ssid, size_t ssid_len, char *passwd, size_t passwd_len);
esp_err_t app_mywifi_clear_sta_creds(void);

#ifdef __cplusplus
}
#endif
