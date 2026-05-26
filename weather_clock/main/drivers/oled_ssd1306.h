#ifndef OLED_SSD1306_H
#define OLED_SSD1306_H

#include <stdint.h>
#include <stdbool.h>

void oled_init(void);
void oled_clear(void);
void oled_refresh(void);
void oled_set_pixel(uint8_t x, uint8_t y, bool on);
void oled_draw_bitmap(uint8_t x, uint8_t y, const uint8_t *bmp, uint8_t w, uint8_t h);
void oled_draw_char(uint8_t x, uint8_t y, char c, const uint8_t (*font)[16]);
void oled_draw_string(uint8_t x, uint8_t y, const char *str, const uint8_t (*font)[16]);
void oled_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on);

extern uint8_t g_oled_fb[1024];

#endif
