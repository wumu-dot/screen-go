#ifndef DISPLAY_DATA_H
#define DISPLAY_DATA_H

#include <stdint.h>
#include <stdbool.h>

#define WEATHER_CLEAR         0
#define WEATHER_PARTLY_CLOUDY 1
#define WEATHER_CLOUDY        2
#define WEATHER_RAIN          3
#define WEATHER_SNOW          4

typedef struct {
    uint16_t year;
    uint8_t  month, day, weekday;
    uint8_t  hour, minute, second;
    int8_t   temperature;
    uint8_t  humidity;
    int8_t   feels_like;
    uint8_t  weather_code;
    char     city[32];
    uint8_t  battery_pct;
    bool     time_valid;
    bool     weather_valid;
    bool     wifi_connected;
} DisplayData;

extern DisplayData g_display_data;
extern SemaphoreHandle_t g_data_sem;
extern QueueHandle_t g_display_queue;
extern volatile uint32_t g_heartbeat_clock;
extern volatile uint32_t g_heartbeat_display;
extern volatile uint32_t g_heartbeat_network;
extern volatile uint32_t g_heartbeat_power;

#endif
