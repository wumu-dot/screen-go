#include "shared/display_data.h"
#include "shared/config.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "wdog";
static uint32_t s_clk, s_dpy, s_net, s_pwr;

void watchdog_task(void *pvParam)
{
    (void)pvParam;
    esp_task_wdt_add(NULL);
    ESP_LOGI(TAG, "WDT timeout=%ds", WDT_TIMEOUT_S);
    while (1) {
        if (g_heartbeat_clock   != s_clk) s_clk = g_heartbeat_clock;
        else ESP_LOGW(TAG, "ClockTask stall!");
        if (g_heartbeat_display != s_dpy) s_dpy = g_heartbeat_display;
        else ESP_LOGW(TAG, "DisplayTask stall!");
        if (g_heartbeat_network != s_net) s_net = g_heartbeat_network;
        else ESP_LOGW(TAG, "NetworkTask stall!");
        if (g_heartbeat_power   != s_pwr) s_pwr = g_heartbeat_power;
        else ESP_LOGW(TAG, "PowerTask stall!");
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
