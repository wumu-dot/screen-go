#include "shared/display_data.h"
#include "shared/config.h"
#include "drivers/oled_ssd1306.h"
#include "fonts/font_8x16.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdio.h>

static void render_status(void)
{
    if (g_display_data.wifi_connected)
        draw_status_icon(0, 0, icon_wifi_on);
    else
        draw_status_icon(0, 0, icon_wifi_off);

    /* 时间 HH:MM */
    char t[8];
    snprintf(t, sizeof(t), "%02d:%02d", g_display_data.hour, g_display_data.minute);
    oled_draw_string(72, 0, t, font_8x16);

    /* 电池% */
    char b[5];
    snprintf(b, sizeof(b), "%d%%", g_display_data.battery_pct);
    oled_draw_string(100, 0, b, font_8x16);
}

static void render_city(void)
{
    /* 默认北京, 简单判断英文城市名 */
    if (g_display_data.city[0] && g_display_data.city[0] >= 'A') {
        oled_draw_string(0, 14, g_display_data.city, font_8x16);
    } else {
        draw_chinese(32, 14, CH_IDX_BEI);
        draw_chinese(48, 14, CH_IDX_JING);
    }
}

static void render_temp(void)
{
    char buf[8];
    if (g_display_data.weather_valid)
        snprintf(buf, sizeof(buf), "%dC", g_display_data.temperature);
    else
        snprintf(buf, sizeof(buf), "--C");
    oled_draw_string(40, 28, buf, font_8x16);
}

static void render_desc(void)
{
    if (!g_display_data.weather_valid) { oled_draw_string(44, 50, "--", font_8x16); return; }
    switch (g_display_data.weather_code) {
    case 0: draw_chinese(48, 48, CH_IDX_CLEAR); break;
    case 1: draw_chinese(32, 48, CH_IDX_PARTLY); draw_chinese(64, 48, CH_IDX_CLOUDY); break;
    case 2: draw_chinese(48, 48, CH_IDX_CLOUDY); break;
    case 3: draw_chinese(48, 48, CH_IDX_RAIN); break;
    case 4: draw_chinese(48, 48, CH_IDX_SNOW); break;
    default: draw_chinese(48, 48, CH_IDX_CLOUDY); break;
    }
}

static void render_bottom(void)
{
    char buf[24];
    if (g_display_data.weather_valid)
        snprintf(buf, sizeof(buf), "H:%02d%% F:%dC", g_display_data.humidity, g_display_data.feels_like);
    else
        snprintf(buf, sizeof(buf), "H:--%% F:--C");
    oled_draw_string(10, 56, buf, font_8x16);
}

void display_task(void *pvParam)
{
    (void)pvParam;
    uint32_t note;
    while (1) {
        xTaskNotifyWait(0, 0xFFFFFFFF, &note, pdMS_TO_TICKS(1000));
        if (xSemaphoreTake(g_data_sem, pdMS_TO_TICKS(10)) == pdTRUE) {
            oled_clear();
            render_status();
            render_city();
            render_temp();
            render_desc();
            render_bottom();
            oled_refresh();
            xSemaphoreGive(g_data_sem);
        }
        g_heartbeat_display++;
    }
}
