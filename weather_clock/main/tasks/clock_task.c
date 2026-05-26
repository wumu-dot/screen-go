#include "shared/display_data.h"
#include "shared/config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static int64_t s_utc = 0;

void clock_set_time(int64_t ts)
{
    if (xSemaphoreTake(g_data_sem, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_utc = ts;
        g_display_data.time_valid = true;
        xSemaphoreGive(g_data_sem);
        xTaskNotifyGive((TaskHandle_t)g_display_queue);
    }
}

static bool is_leap(uint16_t y) { return (y%4==0 && y%100!=0) || y%400==0; }

static void utc_to_cal(int64_t ts, DisplayData *d)
{
    static const uint8_t md[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    d->second = ts % 60; ts /= 60;
    d->minute = ts % 60; ts /= 60;
    d->hour   = ts % 24; ts /= 24;

    uint16_t y = 1970;
    uint32_t days = (uint32_t)ts;
    while (1) {
        uint16_t diy = is_leap(y) ? 366 : 365;
        if (days < diy) break;
        days -= diy; y++;
    }
    d->year = y;
    uint8_t m;
    for (m = 0; m < 12; m++) {
        uint8_t lim = md[m];
        if (m == 1 && is_leap(y)) lim = 29;
        if (days < lim) break;
        days -= lim;
    }
    d->month = m + 1;
    d->day = (uint8_t)(days + 1);
    d->weekday = (uint8_t)((ts + 4) % 7);
}

void clock_task(void *pvParam)
{
    (void)pvParam;
    TickType_t last = xTaskGetTickCount();
    while (1) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(CLOCK_TICK_MS));
        if (!g_display_data.time_valid) continue;
        if (xSemaphoreTake(g_data_sem, pdMS_TO_TICKS(10)) == pdTRUE) {
            s_utc++;
            utc_to_cal(s_utc, &g_display_data);
            xSemaphoreGive(g_data_sem);
            xTaskNotifyGive((TaskHandle_t)g_display_queue);
        }
        g_heartbeat_clock++;
    }
}
