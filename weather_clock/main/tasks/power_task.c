#include "shared/display_data.h"
#include "shared/config.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

void power_task(void *pvParam)
{
    (void)pvParam;
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
    esp_adc_cal_characteristics_t cal;
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH_BIT_12, 1100, &cal);
    TickType_t last = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(POWER_INTERVAL_S * 1000));
        uint32_t raw = 0;
        for (int i = 0; i < 16; i++) raw += adc1_get_raw(ADC_CHANNEL);
        raw /= 16;
        uint32_t mv = esp_adc_cal_raw_to_voltage(raw, &cal) * VOLTAGE_DIVIDER;
        int pct = (int)((mv - BATTERY_EMPTY_V * 1000) * 100 / ((BATTERY_FULL_V - BATTERY_EMPTY_V) * 1000));
        if (pct < 0) pct = 0; if (pct > 100) pct = 100;

        if (xSemaphoreTake(g_data_sem, pdMS_TO_TICKS(10)) == pdTRUE) {
            g_display_data.battery_pct = (uint8_t)pct;
            xSemaphoreGive(g_data_sem);
            xTaskNotifyGive((TaskHandle_t)g_display_queue);
        }
        g_heartbeat_power++;
    }
}
