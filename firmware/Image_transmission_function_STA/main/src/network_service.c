#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "app_mywifi.h"
#include "network_service.h"

static const char *TAG = "network_service";

esp_err_t network_service_start(void)
{
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "Failed to initialize TCP/IP stack");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "Failed to create event loop");

    /* STA steady state with on-demand SoftAP provisioning. mDNS/NetBIOS are
     * intentionally not started: the device is reached by its STA IP. */
    app_mywifi_main();
    return ESP_OK;
}
