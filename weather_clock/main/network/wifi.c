#include "wifi.h"
#include "shared/config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "wifi";
static EventGroupHandle_t s_evt;
#define WIFI_BIT BIT0

static void handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
        esp_wifi_connect();
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_evt, WIFI_BIT);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_evt, WIFI_BIT);
        ESP_LOGI(TAG, "WiFi connected");
    }
}

bool wifi_connect(void)
{
    nvs_flash_init();
    s_evt = xEventGroupCreate();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t h1, h2;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &handler, NULL, &h1);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &handler, NULL, &h2);

    wifi_config_t wc = {0};
    strncpy((char *)wc.sta.ssid, WIFI_SSID, 32);
    strncpy((char *)wc.sta.password, WIFI_PASSWORD, 64);
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wc);
    esp_wifi_start();

    EventBits_t bits = xEventGroupWaitBits(s_evt, WIFI_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));
    return (bits & WIFI_BIT) != 0;
}

bool wifi_is_connected(void)
{
    if (!s_evt) return false;
    return (xEventGroupGetBits(s_evt) & WIFI_BIT) != 0;
}
