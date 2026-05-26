#include "shared/display_data.h"
#include "shared/config.h"
#include "network/wifi.h"
#include "network/http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "network";
extern void clock_set_time(int64_t ts);

static bool get_loc(char *city, size_t sz, float *lat, float *lon)
{
    char *body = http_get(IPAPI_URL);
    if (!body) return false;
    cJSON *r = cJSON_Parse(body);
    bool ok = false;
    if (r) {
        cJSON *ci = cJSON_GetObjectItem(r, "city");
        cJSON *la = cJSON_GetObjectItem(r, "latitude");
        cJSON *lo = cJSON_GetObjectItem(r, "longitude");
        if (ci && cJSON_IsString(ci)) {
            const char *s = ci->valuestring;
            size_t i = 0;
            while (*s && i < sz - 1) {
                if ((unsigned char)*s < 128) city[i++] = *s;
                s++;
            }
            city[i] = '\0';
            if (!city[0]) strncpy(city, DEFAULT_CITY, sz);
        } else strncpy(city, DEFAULT_CITY, sz);
        *lat = (la && cJSON_IsNumber(la)) ? (float)la->valuedouble : DEFAULT_LAT;
        *lon = (lo && cJSON_IsNumber(lo)) ? (float)lo->valuedouble : DEFAULT_LON;
        ok = true;
        cJSON_Delete(r);
    }
    free(body);
    return ok;
}

static bool get_weather(float lat, float lon)
{
    char url[256];
    snprintf(url, sizeof(url),
        "%s?latitude=%.4f&longitude=%.4f&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code",
        WEATHER_API_URL, lat, lon);
    char *body = http_get(url);
    if (!body) return false;
    cJSON *r = cJSON_Parse(body);
    bool ok = false;
    if (r) {
        cJSON *cur = cJSON_GetObjectItem(r, "current");
        if (cur) {
            cJSON *t = cJSON_GetObjectItem(cur, "temperature_2m");
            cJSON *h = cJSON_GetObjectItem(cur, "relative_humidity_2m");
            cJSON *f = cJSON_GetObjectItem(cur, "apparent_temperature");
            cJSON *w = cJSON_GetObjectItem(cur, "weather_code");
            if (xSemaphoreTake(g_data_sem, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (t) g_display_data.temperature = (int8_t)t->valuedouble;
                if (h) g_display_data.humidity    = (uint8_t)h->valuedouble;
                if (f) g_display_data.feels_like   = (int8_t)f->valuedouble;
                if (w) g_display_data.weather_code  = wmo_to_weather_code((int)w->valuedouble);
                g_display_data.weather_valid = true;
                xSemaphoreGive(g_data_sem);
                ok = true;
            }
        }
        cJSON_Delete(r);
    }
    free(body);
    return ok;
}

static void sync_ntp(void)
{
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER);
    esp_netif_sntp_init(&cfg);
    int retry = 0;
    while (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(5000)) != ESP_OK && retry < 3) retry++;
    if (retry < 3) {
        time_t now; time(&now);
        clock_set_time((int64_t)now);
        ESP_LOGI(TAG, "NTP OK: %lld", (long long)now);
    }
    esp_netif_sntp_deinit();
}

void network_task(void *pvParam)
{
    (void)pvParam;
    vTaskDelay(pdMS_TO_TICKS(5000));
    bool first = true;
    while (1) {
        g_display_data.wifi_connected = wifi_is_connected();
        if (!g_display_data.wifi_connected)
            g_display_data.wifi_connected = wifi_connect();

        if (g_display_data.wifi_connected) {
            float lat, lon; char city[32];
            if (get_loc(city, sizeof(city), &lat, &lon)) {
                if (xSemaphoreTake(g_data_sem, pdMS_TO_TICKS(100)) == pdTRUE) {
                    strncpy(g_display_data.city, city, 31);
                    xSemaphoreGive(g_data_sem);
                }
            }
            get_weather(lat, lon);
            if (first) { sync_ntp(); first = false; }
            xTaskNotifyGive((TaskHandle_t)g_display_queue);
        }
        g_heartbeat_network++;
        vTaskDelay(pdMS_TO_TICKS(NETWORK_INTERVAL_S * 1000));
    }
}
