#include "oled_ssd1306.h"
#include "driver/i2c.h"
#include "shared/config.h"
#include <string.h>

uint8_t g_oled_fb[1024];

static const uint8_t OLED_INIT_CMDS[] = {
    0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
    0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
    0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF
};

static esp_err_t i2c_write_cmd(uint8_t cmd)
{
    i2c_cmd_handle_t h = i2c_cmd_link_create();
    i2c_master_start(h);
    i2c_master_write_byte(h, (OLED_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(h, 0x00, true);
    i2c_master_write_byte(h, cmd, true);
    i2c_master_stop(h);
    esp_err_t r = i2c_master_cmd_begin(I2C_MASTER_PORT, h, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(h);
    return r;
}

static esp_err_t i2c_write_data(const uint8_t *data, size_t len)
{
    i2c_cmd_handle_t h = i2c_cmd_link_create();
    i2c_master_start(h);
    i2c_master_write_byte(h, (OLED_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(h, 0x40, true);
    i2c_master_write(h, data, len, true);
    i2c_master_stop(h);
    esp_err_t r = i2c_master_cmd_begin(I2C_MASTER_PORT, h, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(h);
    return r;
}

void oled_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_PORT, &conf);
    i2c_driver_install(I2C_MASTER_PORT, I2C_MODE_MASTER, 0, 0, 0);

    for (size_t i = 0; i < sizeof(OLED_INIT_CMDS); i++)
        i2c_write_cmd(OLED_INIT_CMDS[i]);

    oled_clear();
    oled_refresh();
}

void oled_clear(void) { memset(g_oled_fb, 0, sizeof(g_oled_fb)); }

void oled_refresh(void)
{
    i2c_write_cmd(0x21); i2c_write_cmd(0); i2c_write_cmd(127);
    i2c_write_cmd(0x22); i2c_write_cmd(0); i2c_write_cmd(7);
    i2c_write_data(g_oled_fb, sizeof(g_oled_fb));
}

void oled_set_pixel(uint8_t x, uint8_t y, bool on)
{
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) return;
    uint16_t idx = x + (y / 8) * OLED_WIDTH;
    if (on) g_oled_fb[idx] |= (1 << (y % 8));
    else    g_oled_fb[idx] &= ~(1 << (y % 8));
}

void oled_draw_bitmap(uint8_t x, uint8_t y, const uint8_t *bmp, uint8_t w, uint8_t h)
{
    for (uint8_t dy = 0; dy < h; dy++)
        for (uint8_t dx = 0; dx < w; dx++) {
            uint8_t byte = bmp[(dx / 8) * h + dy];
            oled_set_pixel(x + dx, y + dy, byte & (1 << (dx % 8)));
        }
}

void oled_draw_char(uint8_t x, uint8_t y, char c, const uint8_t (*font)[16])
{
    if (c < 32 || c > 126) return;
    const uint8_t *g = font[c - 32];
    for (uint8_t r = 0; r < 16; r++)
        for (uint8_t col = 0; col < 8; col++)
            oled_set_pixel(x + col, y + r, g[r] & (0x80 >> col));
}

void oled_draw_string(uint8_t x, uint8_t y, const char *str, const uint8_t (*font)[16])
{
    while (*str) { oled_draw_char(x, y, *str, font); x += 8; str++; }
}

void oled_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on)
{
    for (uint8_t dy = 0; dy < h; dy++)
        for (uint8_t dx = 0; dx < w; dx++)
            oled_set_pixel(x + dx, y + dy, on);
}
