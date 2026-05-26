#include "shared/display_data.h"
#include "shared/config.h"
#include "drivers/oled_ssd1306.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "main";

DisplayData g_display_data = {0};
SemaphoreHandle_t g_data_sem = NULL;
QueueHandle_t g_display_queue = NULL;
volatile uint32_t g_heartbeat_clock = 0;
volatile uint32_t g_heartbeat_display = 0;
volatile uint32_t g_heartbeat_network = 0;
volatile uint32_t g_heartbeat_power = 0;

extern void display_task(void *);
extern void clock_task(void *);
extern void network_task(void *);
extern void power_task(void *);
extern void watchdog_task(void *);

void app_main(void)
{
    ESP_LOGI(TAG, "Weather Clock starting...");
    nvs_flash_init();
    oled_init();

    g_data_sem = xSemaphoreCreateMutex();
    g_display_queue = xQueueCreate(5, sizeof(uint32_t));
    strncpy(g_display_data.city, DEFAULT_CITY, 31);
    g_display_data.battery_pct = 100;

    xTaskCreate(display_task,  "display",  STACK_DISPLAY,  NULL, PRIO_DISPLAY,  NULL);
    xTaskCreate(clock_task,    "clock",    STACK_CLOCK,    NULL, PRIO_CLOCK,    NULL);
    xTaskCreate(network_task,  "network",  STACK_NETWORK,  NULL, PRIO_NETWORK,  NULL);
    xTaskCreate(power_task,    "power",    STACK_POWER,    NULL, PRIO_POWER,    NULL);
    xTaskCreate(watchdog_task, "watchdog", STACK_WATCHDOG, NULL, PRIO_WATCHDOG, NULL);

    ESP_LOGI(TAG, "5 tasks created. Running...");
}
