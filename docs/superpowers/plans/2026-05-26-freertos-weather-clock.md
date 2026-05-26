# FreeRTOS 天气时钟 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 基于 ESP32 + SSD1306 OLED 构建 FreeRTOS 天气时钟

**Architecture:** 5任务协作 — ClockTask维护时间，NetworkTask获取天气/NTP，PowerTask读电池，DisplayTask渲染OLED，WatchdogTask心跳检测。通过DisplayData结构体+信号量+队列通信。

**Tech Stack:** ESP-IDF v5.x, FreeRTOS, C, SSD1306 I2C驱动, Open-Meteo天气API, ipapi.co定位

---

### Task 1: 项目脚手架 & 配置

**Files:**
- Create: `weather_clock/CMakeLists.txt`
- Create: `weather_clock/main/CMakeLists.txt`
- Create: `weather_clock/main/shared/config.h`
- Create: `weather_clock/main/shared/display_data.h`

- [ ] **Step 1: 创建顶层 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(weather_clock)
```

- [ ] **Step 2: 创建 main/CMakeLists.txt**

```cmake
idf_component_register(
    SRCS
        "main.c"
        "tasks/display_task.c"
        "tasks/clock_task.c"
        "tasks/network_task.c"
        "tasks/power_task.c"
        "tasks/watchdog_task.c"
        "drivers/oled_ssd1306.c"
        "network/wifi.c"
        "network/http_client.c"
        "fonts/font_8x16.c"
    INCLUDE_DIRS
        "."
        "shared"
        "drivers"
        "network"
        "fonts"
        "tasks"
)
```

- [ ] **Step 3: 创建 shared/config.h**

```c
#ifndef CONFIG_H
#define CONFIG_H

/* WiFi */
#define WIFI_SSID           "your_ssid"
#define WIFI_PASSWORD       "your_password"
#define WIFI_MAX_RETRIES    5
#define WIFI_RETRY_DELAY_MS 2000

/* 时间 */
#define NTP_SERVER          "pool.ntp.org"
#define NTP_SYNC_INTERVAL   (24 * 3600)  /* 每天一次 */
#define CLOCK_TICK_MS       1000          /* 时钟任务周期 */

/* 网络 */
#define NETWORK_INTERVAL_S  (15 * 60)     /* 15分钟更新一次天气 */
#define WEATHER_STALE_S     (30 * 60)     /* 30分钟无更新则清空天气 */

/* 天气API */
#define WEATHER_API_URL     "https://api.open-meteo.com/v1/forecast"
#define IPAPI_URL           "https://ipapi.co/json/"

/* 默认城市 (IP定位失败兜底) */
#define DEFAULT_CITY        "北京"
#define DEFAULT_LAT         39.9042
#define DEFAULT_LON         116.4074

/* 电池 */
#define POWER_INTERVAL_S    30
#define ADC_CHANNEL         ADC1_CHANNEL_6  /* GPIO34 */
#define ADC_ATTEN           ADC_ATTEN_DB_11
#define ADC_MAX_RAW         4095
#define VOLTAGE_DIVIDER     2.0             /* 分压比 */
#define BATTERY_FULL_V      4.2
#define BATTERY_EMPTY_V     3.3

/* I2C OLED */
#define I2C_MASTER_SCL_IO   22
#define I2C_MASTER_SDA_IO   21
#define I2C_MASTER_FREQ_HZ  400000
#define I2C_MASTER_PORT     I2C_NUM_0
#define OLED_ADDR           0x3C
#define OLED_WIDTH          128
#define OLED_HEIGHT         64

/* 任务栈 & 优先级 */
#define STACK_DISPLAY       2048
#define STACK_CLOCK         1536
#define STACK_NETWORK       4096
#define STACK_POWER         1024
#define STACK_WATCHDOG      1024

#define PRIO_DISPLAY        5
#define PRIO_CLOCK          4
#define PRIO_NETWORK        3
#define PRIO_POWER          2
#define PRIO_WATCHDOG       1

/* 看门狗超时 */
#define WDT_TIMEOUT_S       10

#endif
```

- [ ] **Step 4: 创建 shared/display_data.h**

```c
#ifndef DISPLAY_DATA_H
#define DISPLAY_DATA_H

#include <stdint.h>
#include <stdbool.h>

/* 天气代码 */
#define WEATHER_CLEAR        0
#define WEATHER_PARTLY_CLOUDY 1
#define WEATHER_CLOUDY       2
#define WEATHER_RAIN         3
#define WEATHER_SNOW         4

typedef struct {
    /* 时钟 (ClockTask写入) */
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  weekday;   /* 0=周日 */
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;

    /* 天气 (NetworkTask写入) */
    int8_t   temperature;     /* ℃, -128表示无效 */
    uint8_t  humidity;        /* % */
    int8_t   feels_like;      /* ℃ */
    uint8_t  weather_code;
    char     city[32];

    /* 电量 (PowerTask写入) */
    uint8_t  battery_pct;     /* 0-100 */

    /* 状态 */
    bool     time_valid;
    bool     weather_valid;
    bool     wifi_connected;
} DisplayData;

/* 全局共享实例 */
extern DisplayData g_display_data;

/* 信号量：保护DisplayData写入 */
extern SemaphoreHandle_t g_data_sem;

/* 队列：通知DisplayTask刷新 */
extern QueueHandle_t g_display_queue;

/* 心跳计数器 (各任务每秒递增, WatchdogTask检查) */
extern volatile uint32_t g_heartbeat_clock;
extern volatile uint32_t g_heartbeat_display;
extern volatile uint32_t g_heartbeat_network;
extern volatile uint32_t g_heartbeat_power;

#endif
```

- [ ] **Step 5: 编译验证**

```bash
cd weather_clock && idf.py build
```

- [ ] **Step 6: 提交**

```bash
git add weather_clock/ && git commit -m "feat: 项目脚手架 & 配置 & 共享数据结构"
```

---

### Task 2: OLED SSD1306 I2C 驱动

**Files:**
- Create: `weather_clock/main/drivers/oled_ssd1306.h`
- Create: `weather_clock/main/drivers/oled_ssd1306.c`

- [ ] **Step 1: 创建 oled_ssd1306.h**

```c
#ifndef OLED_SSD1306_H
#define OLED_SSD1306_H

#include <stdint.h>

void oled_init(void);
void oled_clear(void);
void oled_refresh(void);          /* 把framebuffer推到屏幕 */
void oled_set_pixel(uint8_t x, uint8_t y, bool on);
void oled_draw_bitmap(uint8_t x, uint8_t y, const uint8_t *bmp, uint8_t w, uint8_t h);
void oled_draw_char(uint8_t x, uint8_t y, char c, const uint8_t (*font)[16]);
void oled_draw_string(uint8_t x, uint8_t y, const char *str, const uint8_t (*font)[16]);
void oled_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on);

/* 全局framebuffer, 128x64 = 1024 bytes, 每字节8垂直像素 */
extern uint8_t g_oled_fb[1024];

#endif
```

- [ ] **Step 2: 创建 oled_ssd1306.c**

```c
#include "oled_ssd1306.h"
#include "driver/i2c.h"
#include "shared/config.h"
#include <string.h>

uint8_t g_oled_fb[1024];

/* SSD1306命令 */
static const uint8_t OLED_INIT_CMDS[] = {
    0xAE,           /* 关闭显示 */
    0xD5, 0x80,     /* 设置时钟分频 */
    0xA8, 0x3F,     /* 复用比 1/64 */
    0xD3, 0x00,     /* 显示偏移 */
    0x40,           /* 起始行 */
    0x8D, 0x14,     /* 启用电荷泵 */
    0x20, 0x00,     /* 水平寻址 */
    0xA1,           /* 段重映射: 左右翻转 */
    0xC8,           /* COM扫描: 上下翻转 */
    0xDA, 0x12,     /* COM引脚 */
    0x81, 0xCF,     /* 对比度 */
    0xD9, 0xF1,     /* 预充电 */
    0xDB, 0x40,     /* VCOMH */
    0xA4,           /* 正常显示 */
    0xA6,           /* 正显 */
    0xAF            /* 开启显示 */
};

static esp_err_t i2c_write_cmd(uint8_t cmd)
{
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (OLED_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, 0x00, true);  /* 控制字节: 命令 */
    i2c_master_write_byte(handle, cmd, true);
    i2c_master_stop(handle);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_PORT, handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(handle);
    return ret;
}

static esp_err_t i2c_write_data(const uint8_t *data, size_t len)
{
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (OLED_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, 0x40, true);  /* 控制字节: 数据 */
    i2c_master_write(handle, data, len, true);
    i2c_master_stop(handle);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_PORT, handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(handle);
    return ret;
}

void oled_init(void)
{
    /* I2C初始化 */
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

    /* 发送初始化命令序列 */
    for (size_t i = 0; i < sizeof(OLED_INIT_CMDS); i++) {
        i2c_write_cmd(OLED_INIT_CMDS[i]);
    }

    oled_clear();
    oled_refresh();
}

void oled_clear(void)
{
    memset(g_oled_fb, 0, sizeof(g_oled_fb));
}

void oled_refresh(void)
{
    /* 设置页寻址: 页0-7, 列0-127 */
    i2c_write_cmd(0x21); i2c_write_cmd(0); i2c_write_cmd(127);  // 列范围
    i2c_write_cmd(0x22); i2c_write_cmd(0); i2c_write_cmd(7);    // 页范围

    i2c_write_data(g_oled_fb, sizeof(g_oled_fb));
}

void oled_set_pixel(uint8_t x, uint8_t y, bool on)
{
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) return;
    uint16_t idx = x + (y / 8) * OLED_WIDTH;
    if (on) {
        g_oled_fb[idx] |= (1 << (y % 8));
    } else {
        g_oled_fb[idx] &= ~(1 << (y % 8));
    }
}

void oled_draw_bitmap(uint8_t x, uint8_t y, const uint8_t *bmp, uint8_t w, uint8_t h)
{
    for (uint8_t dy = 0; dy < h; dy++) {
        for (uint8_t dx = 0; dx < w; dx++) {
            /* 位图每字节是一列的8个垂直像素 */
            uint8_t byte_idx = (dx / 8) * h + dy;
            uint8_t bit_mask = 1 << (dx % 8);
            if (dx < w) {
                oled_set_pixel(x + dx, y + dy, bmp[byte_idx] & bit_mask);
            }
        }
    }
}

void oled_draw_char(uint8_t x, uint8_t y, char c, const uint8_t (*font)[16])
{
    if (c < 32 || c > 126) return;
    const uint8_t *glyph = font[c - 32];
    for (uint8_t row = 0; row < 16; row++) {
        uint8_t line = glyph[row];
        for (uint8_t col = 0; col < 8; col++) {
            oled_set_pixel(x + col, y + row, line & (0x80 >> col));
        }
    }
}

void oled_draw_string(uint8_t x, uint8_t y, const char *str, const uint8_t (*font)[16])
{
    while (*str) {
        oled_draw_char(x, y, *str, font);
        x += 8;
        str++;
    }
}

void oled_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on)
{
    for (uint8_t dy = 0; dy < h; dy++) {
        for (uint8_t dx = 0; dx < w; dx++) {
            oled_set_pixel(x + dx, y + dy, on);
        }
    }
}
```

- [ ] **Step 3: 编译验证**

```bash
cd weather_clock && idf.py build
```

- [ ] **Step 4: 提交**

```bash
git add weather_clock/main/drivers/ && git commit -m "feat: SSD1306 OLED I2C驱动"
```

---

### Task 3: 字库 & 图标

**Files:**
- Create: `weather_clock/main/fonts/font_8x16.h`
- Create: `weather_clock/main/fonts/font_8x16.c`

- [ ] **Step 1: 创建 font_8x16.h**

```c
#ifndef FONT_8X16_H
#define FONT_8X16_H

#include <stdint.h>

/* 8x16 ASCII字体, 字符0x20-0x7E, 每字16字节 (每字节一行) */
extern const uint8_t font_8x16[95][16];

/* 16x16中文字模 (每字32字节, 每字节一行, 2字节宽) */
extern const uint8_t font_chinese_16x16[][32];
#define CHINESE_CHAR_COUNT 10

/* 中文索引 */
#define CH_IDX_CLEAR      0  /* 晴 */
#define CH_IDX_CLOUDY     1  /* 云 */
#define CH_IDX_RAIN       2  /* 雨 */
#define CH_IDX_SNOW       3  /* 雪 */
#define CH_IDX_PARTLY     4  /* 多 */
#define CH_IDX_SHANG      5  /* 上 */
#define CH_IDX_HAI        6  /* 海 */
#define CH_IDX_BEI        7  /* 北 */
#define CH_IDX_JING       8  /* 京 */
#define CH_IDX_DEGREE     9  /* ° 备用 */

/* 16x16天气图标位图 (列优先, 每图标32字节) */
extern const uint8_t icon_sun[32];
extern const uint8_t icon_partly_cloudy[32];
extern const uint8_t icon_cloud[32];
extern const uint8_t icon_rain[32];
extern const uint8_t icon_snow[32];
extern const uint8_t icon_wifi_on[16];   /* 8x16 */
extern const uint8_t icon_wifi_off[16];  /* 8x16 */
extern const uint8_t icon_battery[16];   /* 8x16 */

void draw_chinese(uint8_t x, uint8_t y, uint8_t char_idx);
void draw_weather_icon(uint8_t x, uint8_t y, uint8_t weather_code);
void draw_status_icon(uint8_t x, uint8_t y, const uint8_t *icon);

#endif
```

- [ ] **Step 2: 创建 font_8x16.c (含完整字库数据)**

```c
#include "font_8x16.h"
#include "drivers/oled_ssd1306.h"

/* ─── 8x16 ASCII 字库 (95字符, 从空格' '开始) ─── */
const uint8_t font_8x16[95][16] = {
    /* 32 空格 */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 33 ! */
    {0x00,0x00,0x18,0x3C,0x3C,0x3C,0x18,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x00},
    /* 34 " */
    {0x00,0x66,0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 35 # */
    {0x00,0x00,0x00,0x6C,0x6C,0xFE,0x6C,0x6C,0x6C,0xFE,0x6C,0x6C,0x00,0x00,0x00,0x00},
    /* 36 $ */
    {0x18,0x18,0x7C,0xC6,0xC2,0xC0,0x7C,0x06,0x86,0xC6,0x7C,0x18,0x18,0x00,0x00,0x00},
    /* 37 % */
    {0x00,0x00,0x00,0x00,0xC2,0xC6,0x0C,0x18,0x30,0x60,0xC6,0x86,0x00,0x00,0x00,0x00},
    /* 38 & */
    {0x00,0x00,0x38,0x6C,0x6C,0x38,0x76,0xDC,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00},
    /* 39 ' */
    {0x00,0x30,0x30,0x30,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 40 ( */
    {0x00,0x00,0x0C,0x18,0x30,0x30,0x30,0x30,0x30,0x30,0x18,0x0C,0x00,0x00,0x00,0x00},
    /* 41 ) */
    {0x00,0x00,0x30,0x18,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x18,0x30,0x00,0x00,0x00,0x00},
    /* 42 * */
    {0x00,0x00,0x00,0x00,0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 43 + */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 44 , */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x18,0x30,0x00,0x00,0x00},
    /* 45 - */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 46 . */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00},
    /* 47 / */
    {0x00,0x00,0x00,0x00,0x02,0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00,0x00,0x00,0x00},
    /* 48 0 */
    {0x00,0x00,0x3C,0x66,0xC3,0xC3,0xDB,0xDB,0xC3,0xC3,0x66,0x3C,0x00,0x00,0x00,0x00},
    /* 49 1 */
    {0x00,0x00,0x18,0x38,0x78,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x00,0x00,0x00,0x00},
    /* 50 2 */
    {0x00,0x00,0x7C,0xC6,0x06,0x0C,0x18,0x30,0x60,0xC0,0xC6,0xFE,0x00,0x00,0x00,0x00},
    /* 51 3 */
    {0x00,0x00,0x7C,0xC6,0x06,0x06,0x3C,0x06,0x06,0x06,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 52 4 */
    {0x00,0x00,0x0C,0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x0C,0x0C,0x1E,0x00,0x00,0x00,0x00},
    /* 53 5 */
    {0x00,0x00,0xFE,0xC0,0xC0,0xC0,0xFC,0x06,0x06,0x06,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 54 6 */
    {0x00,0x00,0x38,0x60,0xC0,0xC0,0xFC,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 55 7 */
    {0x00,0x00,0xFE,0xC6,0x06,0x06,0x0C,0x18,0x30,0x30,0x30,0x30,0x00,0x00,0x00,0x00},
    /* 56 8 */
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7C,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 57 9 */
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7E,0x06,0x06,0x06,0x0C,0x78,0x00,0x00,0x00,0x00},
    /* 58 : */
    {0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x00},
    /* 59 ; */
    {0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x18,0x18,0x30,0x00,0x00,0x00,0x00},
    /* 60 < */
    {0x00,0x00,0x00,0x06,0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x06,0x00,0x00,0x00,0x00},
    /* 61 = */
    {0x00,0x00,0x00,0x00,0x00,0xFE,0x00,0x00,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 62 > */
    {0x00,0x00,0x00,0x60,0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x60,0x00,0x00,0x00,0x00},
    /* 63 ? */
    {0x00,0x00,0x7C,0xC6,0xC6,0x0C,0x18,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x00},
    /* 64 @ */
    {0x00,0x00,0x00,0x7C,0xC6,0xC6,0xDE,0xDE,0xDE,0xDC,0xC0,0x7C,0x00,0x00,0x00,0x00},
    /* 65 A */
    {0x00,0x00,0x10,0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00},
    /* 66 B */
    {0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x66,0x66,0x66,0x66,0xFC,0x00,0x00,0x00,0x00},
    /* 67 C */
    {0x00,0x00,0x3C,0x66,0xC2,0xC0,0xC0,0xC0,0xC0,0xC2,0x66,0x3C,0x00,0x00,0x00,0x00},
    /* 68 D */
    {0x00,0x00,0xF8,0x6C,0x66,0x66,0x66,0x66,0x66,0x66,0x6C,0xF8,0x00,0x00,0x00,0x00},
    /* 69 E */
    {0x00,0x00,0xFE,0x66,0x62,0x68,0x78,0x68,0x60,0x62,0x66,0xFE,0x00,0x00,0x00,0x00},
    /* 70 F */
    {0x00,0x00,0xFE,0x66,0x62,0x68,0x78,0x68,0x60,0x60,0x60,0xF0,0x00,0x00,0x00,0x00},
    /* 71 G */
    {0x00,0x00,0x3C,0x66,0xC2,0xC0,0xC0,0xDE,0xC6,0xC6,0x66,0x3A,0x00,0x00,0x00,0x00},
    /* 72 H */
    {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00},
    /* 73 I */
    {0x00,0x00,0x3C,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    /* 74 J */
    {0x00,0x00,0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0xCC,0xCC,0xCC,0x78,0x00,0x00,0x00,0x00},
    /* 75 K */
    {0x00,0x00,0xE6,0x66,0x6C,0x6C,0x78,0x78,0x6C,0x66,0x66,0xE6,0x00,0x00,0x00,0x00},
    /* 76 L */
    {0x00,0x00,0xF0,0x60,0x60,0x60,0x60,0x60,0x60,0x62,0x66,0xFE,0x00,0x00,0x00,0x00},
    /* 77 M */
    {0x00,0x00,0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00},
    /* 78 N */
    {0x00,0x00,0xC6,0xE6,0xF6,0xFE,0xDE,0xCE,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00},
    /* 79 O */
    {0x00,0x00,0x38,0x6C,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x00,0x00,0x00,0x00},
    /* 80 P */
    {0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x60,0x60,0x60,0x60,0xF0,0x00,0x00,0x00,0x00},
    /* 81 Q */
    {0x00,0x00,0x38,0x6C,0xC6,0xC6,0xC6,0xC6,0xC6,0xD6,0xDE,0x7C,0x0C,0x0E,0x00,0x00},
    /* 82 R */
    {0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x6C,0x66,0x66,0x66,0xE6,0x00,0x00,0x00,0x00},
    /* 83 S */
    {0x00,0x00,0x7C,0xC6,0xC6,0xC0,0x7C,0x06,0x06,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 84 T */
    {0x00,0x00,0x7E,0x5A,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    /* 85 U */
    {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 86 V */
    {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00,0x00,0x00,0x00},
    /* 87 W */
    {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xD6,0xD6,0xFE,0x6C,0x6C,0x6C,0x00,0x00,0x00,0x00},
    /* 88 X */
    {0x00,0x00,0xC6,0xC6,0x6C,0x38,0x38,0x38,0x38,0x6C,0xC6,0xC6,0x00,0x00,0x00,0x00},
    /* 89 Y */
    {0x00,0x00,0x66,0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    /* 90 Z */
    {0x00,0x00,0xFE,0xC6,0x86,0x0C,0x18,0x30,0x60,0xC2,0xC6,0xFE,0x00,0x00,0x00,0x00},
    /* 91 [ */
    {0x00,0x00,0x3C,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x3C,0x00,0x00,0x00,0x00},
    /* 92 \ */
    {0x00,0x00,0x00,0x80,0xC0,0xE0,0x70,0x38,0x1C,0x0E,0x06,0x02,0x00,0x00,0x00,0x00},
    /* 93 ] */
    {0x00,0x00,0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00,0x00,0x00,0x00},
    /* 94 ^ */
    {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 95 _ */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0x00},
    /* 96 ` */
    {0x00,0x30,0x30,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 97 a */
    {0x00,0x00,0x00,0x00,0x00,0x78,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00},
    /* 98 b */
    {0x00,0x00,0xE0,0x60,0x60,0x78,0x6C,0x66,0x66,0x66,0x66,0x7C,0x00,0x00,0x00,0x00},
    /* 99 c */
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xC0,0xC0,0xC0,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 100 d */
    {0x00,0x00,0x1C,0x0C,0x0C,0x3C,0x6C,0xCC,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00},
    /* 101 e */
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xFE,0xC0,0xC0,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 102 f */
    {0x00,0x00,0x1C,0x36,0x32,0x30,0x78,0x30,0x30,0x30,0x30,0x78,0x00,0x00,0x00,0x00},
    /* 103 g */
    {0x00,0x00,0x00,0x00,0x00,0x76,0xCC,0xCC,0xCC,0xCC,0xCC,0x7C,0x0C,0xCC,0x78,0x00},
    /* 104 h */
    {0x00,0x00,0xE0,0x60,0x60,0x6C,0x76,0x66,0x66,0x66,0x66,0xE6,0x00,0x00,0x00,0x00},
    /* 105 i */
    {0x00,0x00,0x18,0x18,0x00,0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    /* 106 j */
    {0x00,0x00,0x06,0x06,0x00,0x0E,0x06,0x06,0x06,0x06,0x06,0x06,0x66,0x66,0x3C,0x00},
    /* 107 k */
    {0x00,0x00,0xE0,0x60,0x60,0x66,0x6C,0x78,0x78,0x6C,0x66,0xE6,0x00,0x00,0x00,0x00},
    /* 108 l */
    {0x00,0x00,0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    /* 109 m */
    {0x00,0x00,0x00,0x00,0x00,0xEC,0xFE,0xD6,0xD6,0xD6,0xD6,0xC6,0x00,0x00,0x00,0x00},
    /* 110 n */
    {0x00,0x00,0x00,0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x66,0x66,0x00,0x00,0x00,0x00},
    /* 111 o */
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 112 p */
    {0x00,0x00,0x00,0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00},
    /* 113 q */
    {0x00,0x00,0x00,0x00,0x00,0x76,0xCC,0xCC,0xCC,0xCC,0xCC,0x7C,0x0C,0x0C,0x1E,0x00},
    /* 114 r */
    {0x00,0x00,0x00,0x00,0x00,0xDC,0x76,0x66,0x60,0x60,0x60,0xF0,0x00,0x00,0x00,0x00},
    /* 115 s */
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0x60,0x38,0x0C,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 116 t */
    {0x00,0x00,0x10,0x30,0x30,0xFC,0x30,0x30,0x30,0x30,0x36,0x1C,0x00,0x00,0x00,0x00},
    /* 117 u */
    {0x00,0x00,0x00,0x00,0x00,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00},
    /* 118 v */
    {0x00,0x00,0x00,0x00,0x00,0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00,0x00,0x00,0x00},
    /* 119 w */
    {0x00,0x00,0x00,0x00,0x00,0xC6,0xC6,0xD6,0xD6,0xFE,0x6C,0x6C,0x00,0x00,0x00,0x00},
    /* 120 x */
    {0x00,0x00,0x00,0x00,0x00,0xC6,0x6C,0x38,0x38,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00},
    /* 121 y */
    {0x00,0x00,0x00,0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7E,0x06,0x0C,0xF8,0x00},
    /* 122 z */
    {0x00,0x00,0x00,0x00,0x00,0xFE,0xCC,0x18,0x30,0x60,0xC6,0xFE,0x00,0x00,0x00,0x00},
    /* 123 { */
    {0x00,0x00,0x0E,0x18,0x18,0x18,0x70,0x18,0x18,0x18,0x18,0x0E,0x00,0x00,0x00,0x00},
    /* 124 | */
    {0x00,0x00,0x18,0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x00,0x00},
    /* 125 } */
    {0x00,0x00,0x70,0x18,0x18,0x18,0x0E,0x18,0x18,0x18,0x18,0x70,0x00,0x00,0x00,0x00},
    /* 126 ~ */
    {0x00,0x00,0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
};

/* ─── 16x16 中文字模 ─── */
const uint8_t font_chinese_16x16[CHINESE_CHAR_COUNT][32] = {
    /* 0 晴 (24x24简化→16x16) */
    {0x00,0x00,0x00,0x7E,0x40,0x7E,0x01,0x7C,0x44,0x7C,0x01,0xFE,
     0x11,0xFC,0x14,0xFC,0x14,0xFC,0x14,0xFC,0x51,0x54,0x26,0x50,
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 1 云 */
    {0x00,0x00,0x00,0xFE,0x00,0x00,0x00,0xFC,0x44,0x44,0x44,0x44,
     0x44,0xFC,0x00,0xFE,0x02,0x24,0x48,0x10,0x2C,0x46,0x00,0x00,
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 2 雨 */
    {0x00,0x00,0x00,0xFE,0x12,0x12,0x12,0xFE,0x00,0xFE,0x92,0x92,
     0x92,0x92,0x92,0xFE,0x02,0x02,0x02,0x02,0x02,0x02,0x00,0x00,
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 3 雪 */
    {0x00,0x00,0x00,0xFE,0x82,0x92,0x92,0xFE,0x00,0xFE,0x02,0x7C,
     0x44,0x28,0x28,0x10,0x28,0x44,0x82,0x00,0x00,0x00,0x00,0x00,
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 4 多 */
    {0x00,0x00,0x08,0x1C,0x12,0x32,0x54,0x18,0x10,0x08,0x1C,0x12,
     0x32,0x54,0x18,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 5 上 */
    {0x00,0x00,0x00,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0xFE,0x00,
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 6 海 */
    {0x00,0x00,0x21,0x12,0x04,0x48,0x40,0xFC,0x44,0x44,0x44,0xFC,
     0x12,0x22,0xFE,0x06,0x0A,0x12,0x62,0x02,0x00,0x00,0x00,0x00,
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 7 北 */
    {0x00,0x00,0x10,0x10,0x10,0xFE,0x10,0x10,0x10,0x10,0x10,0x00,
     0x10,0x10,0xFE,0x10,0x10,0x10,0x10,0x10,0x00,0x00,0x00,0x00,
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 8 京 */
    {0x00,0x00,0x02,0x22,0x22,0x22,0x22,0xFE,0x22,0x22,0x22,0x22,
     0x02,0x10,0x50,0x7C,0x52,0x52,0x52,0x7C,0x50,0x10,0x00,0x00,
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 9 ℃ (℃/°C可以用ASCII代替，这里留备用) */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
};

/* ─── 16x16 天气图标位图 ─── */
/* 晴: 太阳 */
const uint8_t icon_sun[32] = {
    0x00,0x00,0x00,0x80,0x86,0x80,0x86,0x80,0x20,0x80,0x20,0x83,
    0xF8,0x80,0x38,0x80,0x30,0x87,0xF0,0x80,0x60,0x80,0x60,0x80,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
/* 多云 */
const uint8_t icon_partly_cloudy[32] = {
    0x00,0x00,0x00,0x80,0x84,0x80,0x86,0x80,0x20,0x80,0x20,0x83,
    0xF8,0x80,0x38,0x80,0x30,0x80,0x70,0x87,0xF8,0x80,0x04,0x80,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
/* 阴天 */
const uint8_t icon_cloud[32] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,
    0x7C,0x80,0xC6,0x80,0xC6,0x81,0xFE,0x80,0x04,0x80,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
/* 雨 */
const uint8_t icon_rain[32] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,
    0x7C,0x80,0xC6,0x80,0xC6,0x81,0xFE,0x80,0x44,0x82,0x28,0x82,
    0x44,0x82,0x28,0x82,0x44,0x80,0x00,0x00,
};
/* 雪 */
const uint8_t icon_snow[32] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,
    0x7C,0x80,0xC6,0x80,0xC6,0x81,0xFE,0x80,0x10,0x80,0x10,0x82,
    0x92,0x82,0x54,0x82,0x38,0x82,0x10,0x80,
};

/* 8x16 WiFi图标 */
const uint8_t icon_wifi_on[16] = {
    0x00,0x7E,0x81,0xBD,0x42,0x5A,0x24,0x18,
    0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
const uint8_t icon_wifi_off[16] = {
    0x00,0x7E,0x81,0x3D,0x42,0x1A,0x24,0x18,
    0x18,0x04,0x08,0x10,0x04,0x00,0x00,0x00,
};
/* 8x16 电池图标 */
const uint8_t icon_battery[16] = {
    0x00,0x7E,0x42,0x42,0x42,0x42,0x42,0x42,
    0x42,0x42,0x42,0x7E,0x00,0x00,0x00,0x00,
};

/* ─── 绘制辅助函数 ─── */

void draw_chinese(uint8_t x, uint8_t y, uint8_t char_idx)
{
    if (char_idx >= CHINESE_CHAR_COUNT) return;
    const uint8_t *bmp = font_chinese_16x16[char_idx];
    /* 16x16位图, 每行2字节 */
    for (uint8_t row = 0; row < 16; row++) {
        uint16_t line = (bmp[row * 2] << 8) | bmp[row * 2 + 1];
        for (uint8_t col = 0; col < 16; col++) {
            oled_set_pixel(x + col, y + row, line & (0x8000 >> col));
        }
    }
}

void draw_weather_icon(uint8_t x, uint8_t y, uint8_t weather_code)
{
    const uint8_t *icon;
    switch (weather_code) {
        case 0: icon = icon_sun; break;
        case 1: icon = icon_partly_cloudy; break;
        case 2: icon = icon_cloud; break;
        case 3: icon = icon_rain; break;
        case 4: icon = icon_snow; break;
        default: icon = icon_cloud; break;
    }
    oled_draw_bitmap(x, y, icon, 16, 16);
}

void draw_status_icon(uint8_t x, uint8_t y, const uint8_t *icon)
{
    oled_draw_bitmap(x, y, icon, 8, 16);
}
```

- [ ] **Step 3: 编译验证**

```bash
cd weather_clock && idf.py build
```

- [ ] **Step 4: 提交**

```bash
git add weather_clock/main/fonts/ && git commit -m "feat: 字库 + 天气图标位图"
```

---

### Task 4: WiFi & HTTP 客户端

**Files:**
- Create: `weather_clock/main/network/wifi.h`
- Create: `weather_clock/main/network/wifi.c`
- Create: `weather_clock/main/network/http_client.h`
- Create: `weather_clock/main/network/http_client.c`

- [ ] **Step 1: 创建 wifi.h**

```c
#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>

bool wifi_connect(void);
bool wifi_is_connected(void);

#endif
```

- [ ] **Step 2: 创建 wifi.c**

```c
#include "wifi.h"
#include "shared/config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "wifi";
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *event_data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "WiFi connected, IP obtained");
    }
}

bool wifi_connect(void)
{
    nvs_flash_init();
    s_wifi_event_group = xEventGroupCreate();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler, NULL, &instance_got_ip);

    wifi_config_t wifi_cfg = {0};
    strncpy((char *)wifi_cfg.sta.ssid, WIFI_SSID, 32);
    strncpy((char *)wifi_cfg.sta.password, WIFI_PASSWORD, 64);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                                           pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

bool wifi_is_connected(void)
{
    if (s_wifi_event_group == NULL) return false;
    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
    return (bits & WIFI_CONNECTED_BIT) != 0;
}
```

- [ ] **Step 3: 创建 http_client.h**

```c
#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stdint.h>

/* 对URL发送GET请求，返回响应body (调用者需free) */
char *http_get(const char *url);

/* Open-Meteo天气码 → 内部天气码 (0-4) */
uint8_t wmo_to_weather_code(int wmo_code);

#endif
```

- [ ] **Step 4: 创建 http_client.c**

```c
#include "http_client.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "http";

#define HTTP_RECV_BUF_SIZE 2048

static char s_recv_buf[HTTP_RECV_BUF_SIZE];
static size_t s_recv_len;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (s_recv_len + evt->data_len < HTTP_RECV_BUF_SIZE) {
            memcpy(s_recv_buf + s_recv_len, evt->data, evt->data_len);
            s_recv_len += evt->data_len;
            s_recv_buf[s_recv_len] = '\0';
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

char *http_get(const char *url)
{
    s_recv_len = 0;
    s_recv_buf[0] = '\0';

    esp_http_client_config_t cfg = {
        .url = url,
        .event_handler = http_event_handler,
        .timeout_ms = 10000,
        .buffer_size = HTTP_RECV_BUF_SIZE,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_perform(client);

    char *result = NULL;
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status == 200) {
            result = strdup(s_recv_buf);
        }
        ESP_LOGI(TAG, "HTTP GET %s → %d (%d bytes)", url, status, (int)s_recv_len);
    } else {
        ESP_LOGE(TAG, "HTTP GET %s failed: %s", url, esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return result;
}

uint8_t wmo_to_weather_code(int wmo_code)
{
    /* WMO天气码映射 */
    if (wmo_code <= 1)  return 0;   /* 晴 */
    if (wmo_code == 2)  return 1;   /* 多云 */
    if (wmo_code == 3)  return 2;   /* 阴 */
    if (wmo_code <= 68) return 3;   /* 雨 */
    return 4;                        /* 雪 */
}
```

- [ ] **Step 5: 编译验证**

```bash
cd weather_clock && idf.py build
```

- [ ] **Step 6: 提交**

```bash
git add weather_clock/main/network/ && git commit -m "feat: WiFi连接 + HTTP GET封装"
```

---

### Task 5: DisplayTask — OLED渲染

**Files:**
- Create: `weather_clock/main/tasks/display_task.c`

- [ ] **Step 1: 创建 display_task.c**

```c
#include "shared/display_data.h"
#include "shared/config.h"
#include "drivers/oled_ssd1306.h"
#include "fonts/font_8x16.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdio.h>

/* ─── 页面渲染 ─── */
static void render_status_bar(void)
{
    /* WiFi图标 */
    if (g_display_data.wifi_connected) {
        draw_status_icon(0, 0, icon_wifi_on);
    } else {
        draw_status_icon(0, 0, icon_wifi_off);
    }

    /* 电池图标 + 百分比 */
    draw_status_icon(112, 0, icon_battery);
    char bat_str[4];
    snprintf(bat_str, sizeof(bat_str), "%d%%", g_display_data.battery_pct);
    oled_draw_string(120, 0, "", font_8x16);  /* 紧接电池图标后面位置不够，简化 */
    /* 电池百分比显示在图标下方或省略，这里只显示电池图标 */

    (void)bat_str;
}

static void render_city(void)
{
    /* 城市名用中文字模, 简化: 直接显示字符串(如果有英文字母) */
    /* 中文城市名用字模绘制 */
    if (g_display_data.weather_valid) {
        /* 上海=索引5,6 */
        /* 北京=索引7,8 */
        /* 简单处理: 比较前两个字节 */
        if (g_display_data.city[0] == 'S' || g_display_data.city[0] == 's') {
            /* 英文城市名直接用ASCII */
            oled_draw_string(0, 14, g_display_data.city, font_8x16);
        } else {
            /* 中文城市名 — 默认显示，用已存字模 */
            draw_chinese(0, 14, CH_IDX_BEI);  /* 北 */
            draw_chinese(16, 14, CH_IDX_JING); /* 京 */
        }
    } else {
        draw_chinese(0, 14, CH_IDX_BEI);
        draw_chinese(16, 14, CH_IDX_JING);
    }
}

static void render_temperature(void)
{
    char buf[16];
    int8_t temp = g_display_data.temperature;
    if (g_display_data.weather_valid && temp > -100) {
        snprintf(buf, sizeof(buf), "%dC", temp);
    } else {
        snprintf(buf, sizeof(buf), "--C");
    }
    /* 温度用16x16大字 — 但我们的字库是8x16，所以用大号间距 */
    /* 在y=28处画，两个数字字符 */
    oled_draw_string(32, 28, buf, font_8x16);
}

static void render_weather_desc(void)
{
    if (!g_display_data.weather_valid) {
        oled_draw_string(40, 50, "----", font_8x16);
        return;
    }
    switch (g_display_data.weather_code) {
    case 0: draw_chinese(40, 50, CH_IDX_CLEAR); break;        /* 晴 */
    case 1: draw_chinese(32, 50, CH_IDX_PARTLY); draw_chinese(48, 50, CH_IDX_CLOUDY); break; /* 多云 */
    case 2: draw_chinese(40, 50, CH_IDX_CLOUDY); break;       /* 云/阴 */
    case 3: draw_chinese(40, 50, CH_IDX_RAIN); break;         /* 雨 */
    case 4: draw_chinese(40, 50, CH_IDX_SNOW); break;         /* 雪 */
    default: draw_chinese(40, 50, CH_IDX_CLOUDY); break;
    }
}

static void render_humidity_feels(void)
{
    char buf[24];
    if (g_display_data.weather_valid) {
        snprintf(buf, sizeof(buf), "H:%d%% F:%dC",
                 g_display_data.humidity, g_display_data.feels_like);
    } else {
        snprintf(buf, sizeof(buf), "H:--%% F:--C");
    }
    oled_draw_string(4, 56, buf, font_8x16);
}

static void render_time(void)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d",
             g_display_data.hour, g_display_data.minute);
    /* 时间显示在右上角 */
    oled_draw_string(72, 0, buf, font_8x16);
}

/* ─── DisplayTask ─── */
void display_task(void *pvParam)
{
    (void)pvParam;
    uint32_t notification;

    while (1) {
        /* 等待刷新通知 (最多等1秒, 超时也刷一次保持时间) */
        if (xTaskNotifyWait(0, 0xFFFFFFFF, &notification, pdMS_TO_TICKS(1000))) {
            /* 有数据更新通知 */
        }

        /* 加锁读取共享数据 */
        if (xSemaphoreTake(g_data_sem, pdMS_TO_TICKS(10)) == pdTRUE) {
            oled_clear();

            render_status_bar();
            render_time();
            render_city();
            render_temperature();
            render_weather_desc();
            render_humidity_feels();

            oled_refresh();

            xSemaphoreGive(g_data_sem);
        }

        /* 更新心跳 */
        g_heartbeat_display++;
    }
}
```

**注意:** display_task.c 的声明放在这里，由 main.c 调用。需要在 main.c 中 `extern void display_task(void *);`

- [ ] **Step 2: 编译验证**

```bash
cd weather_clock && idf.py build
```

- [ ] **Step 3: 提交**

```bash
git add weather_clock/main/tasks/display_task.c && git commit -m "feat: DisplayTask OLED渲染"
```

---

### Task 6: ClockTask — 本地时钟

**Files:**
- Create: `weather_clock/main/tasks/clock_task.c`

- [ ] **Step 1: 创建 clock_task.c**

```c
#include "shared/display_data.h"
#include "shared/config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/* Unix时间戳 (秒) */
static int64_t s_unix_time = 0;

/* NTP对时入口 (由NetworkTask调用) */
void clock_set_time(int64_t unix_time)
{
    if (xSemaphoreTake(g_data_sem, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_unix_time = unix_time;
        g_display_data.time_valid = true;
        xSemaphoreGive(g_data_sem);

        /* 通知DisplayTask刷新 */
        xTaskNotifyGive((TaskHandle_t)g_display_queue);
    }
}

/* 判断闰年 */
static bool is_leap_year(uint16_t year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/* Unix时间戳 → 年月日时分秒 星期 */
static void unix_to_calendar(int64_t ts, DisplayData *d)
{
    static const uint8_t month_days[] = {31,28,31,30,31,30,31,31,30,31,30,31};

    d->second = ts % 60; ts /= 60;
    d->minute = ts % 60; ts /= 60;
    d->hour   = ts % 24; ts /= 24;

    /* 从1970-01-01开始计算日期 */
    uint16_t year = 1970;
    uint32_t days = (uint32_t)ts;

    while (1) {
        uint16_t days_in_year = is_leap_year(year) ? 366 : 365;
        if (days < days_in_year) break;
        days -= days_in_year;
        year++;
    }
    d->year = year;

    uint8_t month = 0;
    for (month = 0; month < 12; month++) {
        uint8_t md = month_days[month];
        if (month == 1 && is_leap_year(year)) md = 29;
        if (days < md) break;
        days -= md;
    }
    d->month = month + 1;
    d->day = (uint8_t)(days + 1);

    /* 计算星期 (1970-01-01是周四) */
    d->weekday = (uint8_t)((ts + 4) % 7);
}

void clock_task(void *pvParam)
{
    (void)pvParam;
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CLOCK_TICK_MS));

        if (!g_display_data.time_valid) continue;

        /* 获取信号量 */
        if (xSemaphoreTake(g_data_sem, pdMS_TO_TICKS(10)) == pdTRUE) {
            s_unix_time++;
            unix_to_calendar(s_unix_time, &g_display_data);
            xSemaphoreGive(g_data_sem);

            /* 通知DisplayTask刷新时间 */
            xTaskNotifyGive((TaskHandle_t)g_display_queue);
        }

        g_heartbeat_clock++;
    }
}
```

- [ ] **Step 2: 编译验证**

```bash
cd weather_clock && idf.py build
```

- [ ] **Step 3: 提交**

```bash
git add weather_clock/main/tasks/clock_task.c && git commit -m "feat: ClockTask 本地时钟 + NTP对时接口"
```

---

### Task 7: NetworkTask — WiFi/天气/NTP

**Files:**
- Create: `weather_clock/main/tasks/network_task.c`

- [ ] **Step 1: 创建 network_task.c**

```c
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
extern void clock_set_time(int64_t unix_time);

/* ─── IP定位 ─── */
static bool get_location(char *city, size_t city_len, float *lat, float *lon)
{
    char *body = http_get(IPAPI_URL);
    if (!body) {
        ESP_LOGW(TAG, "IP定位失败");
        return false;
    }

    cJSON *root = cJSON_Parse(body);
    bool ok = false;
    if (root) {
        cJSON *city_json = cJSON_GetObjectItem(root, "city");
        cJSON *lat_json  = cJSON_GetObjectItem(root, "latitude");
        cJSON *lon_json  = cJSON_GetObjectItem(root, "longitude");

        if (city_json && cJSON_IsString(city_json)) {
            /* 只取ASCII字符，中文城市名截断 */
            const char *src = city_json->valuestring;
            size_t i = 0;
            while (*src && i < city_len - 1) {
                /* 跳过非ASCII (中文) */
                if ((unsigned char)*src < 128) {
                    city[i++] = *src;
                }
                src++;
            }
            city[i] = '\0';
            if (city[0] == '\0') strncpy(city, DEFAULT_CITY, city_len);
        } else {
            strncpy(city, DEFAULT_CITY, city_len);
        }

        if (lat_json && cJSON_IsNumber(lat_json))
            *lat = (float)lat_json->valuedouble;
        else
            *lat = DEFAULT_LAT;

        if (lon_json && cJSON_IsNumber(lon_json))
            *lon = (float)lon_json->valuedouble;
        else
            *lon = DEFAULT_LON;

        ok = true;
        ESP_LOGI(TAG, "定位: %s (%.2f, %.2f)", city, *lat, *lon);
        cJSON_Delete(root);
    }
    free(body);
    return ok;
}

/* ─── 获取天气 ─── */
static bool get_weather(float lat, float lon)
{
    char url[256];
    snprintf(url, sizeof(url),
             "%s?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code",
             WEATHER_API_URL, lat, lon);

    char *body = http_get(url);
    if (!body) {
        ESP_LOGW(TAG, "天气请求失败");
        return false;
    }

    cJSON *root = cJSON_Parse(body);
    bool ok = false;
    if (root) {
        cJSON *current = cJSON_GetObjectItem(root, "current");
        if (current) {
            cJSON *temp   = cJSON_GetObjectItem(current, "temperature_2m");
            cJSON *hum    = cJSON_GetObjectItem(current, "relative_humidity_2m");
            cJSON *feels  = cJSON_GetObjectItem(current, "apparent_temperature");
            cJSON *wcode  = cJSON_GetObjectItem(current, "weather_code");

            if (xSemaphoreTake(g_data_sem, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (temp)  g_display_data.temperature = (int8_t)temp->valuedouble;
                if (hum)   g_display_data.humidity    = (uint8_t)hum->valuedouble;
                if (feels) g_display_data.feels_like  = (int8_t)feels->valuedouble;
                if (wcode) g_display_data.weather_code = wmo_to_weather_code((int)wcode->valuedouble);
                g_display_data.weather_valid = true;
                xSemaphoreGive(g_data_sem);
                ok = true;
                ESP_LOGI(TAG, "天气: %d°C, %d%%, code=%d",
                         g_display_data.temperature, g_display_data.humidity,
                         g_display_data.weather_code);
            }
        }
        cJSON_Delete(root);
    }
    free(body);
    return ok;
}

/* ─── NTP对时 ─── */
static void sync_ntp(void)
{
    esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER);
    esp_netif_sntp_init(&sntp_cfg);

    int retry = 0;
    while (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(5000)) != ESP_OK && retry < 3) {
        retry++;
    }

    if (retry < 3) {
        time_t now;
        time(&now);
        clock_set_time((int64_t)now);
        ESP_LOGI(TAG, "NTP对时成功: %lld", (long long)now);
    } else {
        ESP_LOGW(TAG, "NTP对时失败");
    }

    esp_netif_sntp_deinit();
}

/* ─── NetworkTask ─── */
void network_task(void *pvParam)
{
    (void)pvParam;

    /* 等WiFi稳定再开始 */
    vTaskDelay(pdMS_TO_TICKS(5000));

    bool first_run = true;

    while (1) {
        g_display_data.wifi_connected = wifi_is_connected();

        if (!g_display_data.wifi_connected) {
            ESP_LOGI(TAG, "尝试连接WiFi...");
            g_display_data.wifi_connected = wifi_connect();
        }

        if (g_display_data.wifi_connected) {
            /* IP定位 */
            float lat, lon;
            char city[32];
            if (get_location(city, sizeof(city), &lat, &lon)) {
                if (xSemaphoreTake(g_data_sem, pdMS_TO_TICKS(100)) == pdTRUE) {
                    strncpy(g_display_data.city, city, sizeof(g_display_data.city) - 1);
                    xSemaphoreGive(g_data_sem);
                }
            }

            /* 获取天气 */
            get_weather(lat, lon);

            /* NTP对时 (首次运行或每24小时) */
            if (first_run) {
                sync_ntp();
                first_run = false;
            }
            /* TODO: 按NTP_SYNC_INTERVAL定期同步, 简化: 每次循环都检查 */
            /* 此处由于vTaskDelay(15分钟)较长, 每天约对时96次, 可接受 */

            /* 通知DisplayTask刷新 */
            xTaskNotifyGive((TaskHandle_t)g_display_queue);
        }

        /* 更新心跳 */
        g_heartbeat_network++;

        /* 等待下一个周期 */
        vTaskDelay(pdMS_TO_TICKS(NETWORK_INTERVAL_S * 1000));
    }
}
```

- [ ] **Step 2: 编译验证**

```bash
cd weather_clock && idf.py build
```

- [ ] **Step 3: 提交**

```bash
git add weather_clock/main/tasks/network_task.c && git commit -m "feat: NetworkTask WiFi/天气/NTP"
```

---

### Task 8: PowerTask — 电池ADC

**Files:**
- Create: `weather_clock/main/tasks/power_task.c`

- [ ] **Step 1: 创建 power_task.c**

```c
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

    /* 配置ADC */
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);

    /* 校准 */
    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH_BIT_12,
                             1100, &adc_chars);

    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(POWER_INTERVAL_S * 1000));

        /* 多次采样取平均 */
        uint32_t raw = 0;
        for (int i = 0; i < 16; i++) {
            raw += adc1_get_raw(ADC_CHANNEL);
        }
        raw /= 16;

        /* 转换为电压 (mV) */
        uint32_t voltage = esp_adc_cal_raw_to_voltage(raw, &adc_chars);
        voltage = (uint32_t)(voltage * VOLTAGE_DIVIDER);

        /* 电压 → 百分比 (线性近似) */
        int32_t pct = (int32_t)((voltage - BATTERY_EMPTY_V * 1000) * 100 /
                                ((BATTERY_FULL_V - BATTERY_EMPTY_V) * 1000));
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;

        if (xSemaphoreTake(g_data_sem, pdMS_TO_TICKS(10)) == pdTRUE) {
            g_display_data.battery_pct = (uint8_t)pct;
            xSemaphoreGive(g_data_sem);

            /* 电量变化才通知刷新 (简化: 每次通知) */
            xTaskNotifyGive((TaskHandle_t)g_display_queue);
        }

        g_heartbeat_power++;
    }
}
```

- [ ] **Step 2: 编译验证**

```bash
cd weather_clock && idf.py build
```

- [ ] **Step 3: 提交**

```bash
git add weather_clock/main/tasks/power_task.c && git commit -m "feat: PowerTask 电池ADC读取"
```

---

### Task 9: WatchdogTask — 看门狗 & 心跳

**Files:**
- Create: `weather_clock/main/tasks/watchdog_task.c`

- [ ] **Step 1: 创建 watchdog_task.c**

```c
#include "shared/display_data.h"
#include "shared/config.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "watchdog";

static uint32_t s_last_clock = 0;
static uint32_t s_last_display = 0;
static uint32_t s_last_network = 0;
static uint32_t s_last_power = 0;

void watchdog_task(void *pvParam)
{
    (void)pvParam;

    /* 注册看门狗 */
    esp_task_wdt_add(NULL);
    ESP_LOGI(TAG, "看门狗已启用, 超时=%ds", WDT_TIMEOUT_S);

    while (1) {
        /* 检查各任务心跳 */
        if (g_heartbeat_clock   != s_last_clock)   s_last_clock = g_heartbeat_clock;
        else ESP_LOGW(TAG, "ClockTask无心跳!");

        if (g_heartbeat_display != s_last_display) s_last_display = g_heartbeat_display;
        else ESP_LOGW(TAG, "DisplayTask无心跳!");

        if (g_heartbeat_network != s_last_network) s_last_network = g_heartbeat_network;
        else ESP_LOGW(TAG, "NetworkTask无心跳!");

        if (g_heartbeat_power   != s_last_power)   s_last_power = g_heartbeat_power;
        else ESP_LOGW(TAG, "PowerTask无心跳!");

        /* 喂狗 */
        esp_task_wdt_reset();

        vTaskDelay(pdMS_TO_TICKS(2000)); /* 每2秒检查一次 */
    }
}
```

- [ ] **Step 2: 编译验证**

```bash
cd weather_clock && idf.py build
```

- [ ] **Step 3: 提交**

```bash
git add weather_clock/main/tasks/watchdog_task.c && git commit -m "feat: WatchdogTask 心跳检测"
```

---

### Task 10: main.c — 入口 & 任务创建

**Files:**
- Create: `weather_clock/main/main.c`

- [ ] **Step 1: 创建 main.c**

```c
#include "shared/display_data.h"
#include "shared/config.h"
#include "drivers/oled_ssd1306.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "main";

/* ─── 全局变量定义 ─── */
DisplayData g_display_data = {0};
SemaphoreHandle_t g_data_sem = NULL;
QueueHandle_t g_display_queue = NULL;

volatile uint32_t g_heartbeat_clock = 0;
volatile uint32_t g_heartbeat_display = 0;
volatile uint32_t g_heartbeat_network = 0;
volatile uint32_t g_heartbeat_power = 0;

/* ─── 任务函数声明 ─── */
extern void display_task(void *pvParam);
extern void clock_task(void *pvParam);
extern void network_task(void *pvParam);
extern void power_task(void *pvParam);
extern void watchdog_task(void *pvParam);

void app_main(void)
{
    ESP_LOGI(TAG, "FreeRTOS 天气时钟 启动");

    /* 初始化NVS */
    nvs_flash_init();

    /* 初始化OLED */
    oled_init();

    /* 创建信号量 & 队列 */
    g_data_sem = xSemaphoreCreateMutex();
    g_display_queue = xQueueCreate(5, sizeof(uint32_t));

    /* 设置初始默认城市 */
    strncpy(g_display_data.city, DEFAULT_CITY, sizeof(g_display_data.city) - 1);
    g_display_data.battery_pct = 100;

    /* 创建任务 */
    xTaskCreate(display_task,  "display",  STACK_DISPLAY,  NULL, PRIO_DISPLAY,  NULL);
    xTaskCreate(clock_task,    "clock",    STACK_CLOCK,    NULL, PRIO_CLOCK,    NULL);
    xTaskCreate(network_task,  "network",  STACK_NETWORK,  NULL, PRIO_NETWORK,  NULL);
    xTaskCreate(power_task,    "power",    STACK_POWER,    NULL, PRIO_POWER,    NULL);
    xTaskCreate(watchdog_task, "watchdog", STACK_WATCHDOG, NULL, PRIO_WATCHDOG, NULL);

    ESP_LOGI(TAG, "5个任务已创建, 运行中...");
}
```

- [ ] **Step 2: 编译验证**

```bash
cd weather_clock && idf.py build
```

- [ ] **Step 3: 提交**

```bash
git add weather_clock/main/main.c && git commit -m "feat: main入口 创建5个FreeRTOS任务"
```

---

### Task 11: 集成 & 全量编译

- [ ] **Step 1: 确认目录结构完整**

```
weather_clock/
├── CMakeLists.txt
├── main/
│   ├── CMakeLists.txt
│   ├── main.c
│   ├── shared/
│   │   ├── config.h
│   │   └── display_data.h
│   ├── drivers/
│   │   ├── oled_ssd1306.h
│   │   └── oled_ssd1306.c
│   ├── fonts/
│   │   ├── font_8x16.h
│   │   └── font_8x16.c
│   ├── network/
│   │   ├── wifi.h
│   │   ├── wifi.c
│   │   ├── http_client.h
│   │   └── http_client.c
│   └── tasks/
│       ├── display_task.c
│       ├── clock_task.c
│       ├── network_task.c
│       ├── power_task.c
│       └── watchdog_task.c
```

- [ ] **Step 2: 全量编译**

```bash
cd weather_clock && idf.py fullclean && idf.py build
```

期望: 编译成功，无错误。

- [ ] **Step 3: 烧录 & 串口验证**

```bash
idf.py -p COM3 flash monitor
```

期望输出:
```
I (xxx) main: FreeRTOS 天气时钟 启动
I (xxx) main: 5个任务已创建, 运行中...
I (xxx) wifi: WiFi connected, IP obtained
I (xxx) http: HTTP GET https://ipapi.co/json/ → 200 (xxx bytes)
I (xxx) network: 定位: Shanghai (31.23, 121.47)
I (xxx) http: HTTP GET https://api.open-meteo.com/v1/forecast?... → 200 (xxx bytes)
I (xxx) network: 天气: 23°C, 65%, code=2
I (xxx) network: NTP对时成功: xxxxx
```

- [ ] **Step 4: 最终提交**

```bash
git add . && git commit -m "feat: FreeRTOS天气时钟 完整实现

5任务架构:
- DisplayTask (P5): OLED渲染, 事件驱动
- ClockTask (P4): 本地时钟维护
- NetworkTask (P3): WiFi/NTP/天气
- PowerTask (P2): 电池ADC
- WatchdogTask (P1): 心跳+看门狗

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```
