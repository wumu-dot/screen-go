#include "http_client.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "http";
#define BUF_SIZE 2048
static char s_buf[BUF_SIZE];
static size_t s_len;

static esp_err_t handler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA && s_len + evt->data_len < BUF_SIZE) {
        memcpy(s_buf + s_len, evt->data, evt->data_len);
        s_len += evt->data_len;
        s_buf[s_len] = '\0';
    }
    return ESP_OK;
}

char *http_get(const char *url)
{
    s_len = 0; s_buf[0] = '\0';
    esp_http_client_config_t cfg = {
        .url = url, .event_handler = handler, .timeout_ms = 10000,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_perform(c);
    char *r = NULL;
    if (err == ESP_OK && esp_http_client_get_status_code(c) == 200)
        r = strdup(s_buf);
    ESP_LOGI(TAG, "GET %s → %d", url, r ? 200 : -1);
    esp_http_client_cleanup(c);
    return r;
}

uint8_t wmo_to_weather_code(int wmo)
{
    if (wmo <= 1)  return 0;
    if (wmo == 2)  return 1;
    if (wmo == 3)  return 2;
    if (wmo <= 68) return 3;
    return 4;
}
