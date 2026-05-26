#ifndef FONT_8X16_H
#define FONT_8X16_H

#include <stdint.h>

extern const uint8_t font_8x16[95][16];
extern const uint8_t font_chinese_16x16[][32];
#define CHINESE_CHAR_COUNT 10

#define CH_IDX_CLEAR   0
#define CH_IDX_CLOUDY  1
#define CH_IDX_RAIN    2
#define CH_IDX_SNOW    3
#define CH_IDX_PARTLY  4
#define CH_IDX_SHANG   5
#define CH_IDX_HAI     6
#define CH_IDX_BEI     7
#define CH_IDX_JING    8
#define CH_IDX_DEGREE  9

extern const uint8_t icon_sun[32];
extern const uint8_t icon_partly_cloudy[32];
extern const uint8_t icon_cloud[32];
extern const uint8_t icon_rain[32];
extern const uint8_t icon_snow[32];
extern const uint8_t icon_wifi_on[16];
extern const uint8_t icon_wifi_off[16];
extern const uint8_t icon_battery[16];

void draw_chinese(uint8_t x, uint8_t y, uint8_t char_idx);
void draw_weather_icon(uint8_t x, uint8_t y, uint8_t weather_code);
void draw_status_icon(uint8_t x, uint8_t y, const uint8_t *icon);

#endif
