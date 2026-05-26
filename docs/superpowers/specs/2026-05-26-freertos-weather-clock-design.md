# FreeRTOS 天气时钟 — 设计文档

## 概述

基于 ESP32 + SSD1306 OLED (128x64) 的便携式天气时钟，运行 FreeRTOS，通过 WiFi 获取天气和 NTP 对时，电池供电，无需任何物理按键。

---

## 硬件清单

| 模块 | 型号 | 接口 |
|------|------|------|
| 主控 | ESP32-DevKitC (或 NodeMCU-32S) | - |
| 显示屏 | SSD1306 0.96寸 OLED (128x64) | I2C (SDA=GPIO21, SCL=GPIO22) |
| 电池 | 3.7V 锂电池 + TP4056 充电模块 | ADC (GPIO34) 经分压电阻 |
| 可选 | 电池升压模块 3.7V→5V 给 ESP32 供电 | - |

---

## 软件架构

### 任务划分 (5个任务)

| 任务 | 优先级 | 栈大小 | 频率 | 职责 |
|------|--------|--------|------|------|
| DisplayTask | 最高 (5) | 2KB | 事件驱动 | 收到队列通知 → 渲染OLED |
| ClockTask | 高 (4) | 1.5KB | 每秒 | 维护时间戳，计算日期/星期 |
| NetworkTask | 中 (3) | 4KB | 每15分钟 | WiFi连接 → IP定位 → 天气 → NTP |
| PowerTask | 低 (2) | 1KB | 每30秒 | ADC读电池电压，算电量百分比 |
| WatchdogTask | 最低 (1) | 1KB | 每秒 | 喂狗，检测各任务心跳 |

### 通信机制

- `DisplayData` 结构体：全局共享数据，`Semaphore` 保护写入
- `Queue`：NetworkTask/PowerTask 写完数据后发通知给 DisplayTask
- 各任务只写自己负责的字段，读由 DisplayTask 统一加锁读取

### 共享数据结构体

```c
typedef struct {
    // 时钟 (ClockTask)
    uint16_t year;
    uint8_t  month, day, weekday;
    uint8_t  hour, minute, second;

    // 天气 (NetworkTask)
    int8_t   temperature;      // ℃
    uint8_t  humidity;         // %
    int8_t   feels_like;       // 体感温度 ℃
    uint8_t  weather_code;     // 0=晴 1=多云 2=雨 3=雪
    char     city[32];

    // 电量 (PowerTask)
    uint8_t  battery_pct;      // 0-100%

    // 状态
    bool     time_valid;
    bool     weather_valid;
    bool     wifi_connected;
} DisplayData;
```

---

## OLED 页面布局 (128x64)

```
┌──────────────────────┐
│  📶              🔋82%│  y=0~10  状态栏 (8px)
│  上海                 │  y=14~24 城市名 (10px)
│                      │
│   23°C               │  y=28~48 温度 (16x16大字)
│   多云                │  y=50~56 天气描述 (8px)
│                      │
│  💧65%   🏃25°       │  y=57~63 湿度+体感 (8px)
└──────────────────────┘
```

- 字体：自定 8x16 ASCII字库 + 天气图标位图 + 城市名汉字位图
- 无完整中文字库,只存用到的几个字 (晴/多/云/雨/雪/上/海/北/京等)

---

## 网络流程

### NetworkTask 状态机

```
WiFi连接 → IP定位(ipapi.co) → 天气请求(Open-Meteo)
    │                              │
    │ 失败5次后等15分钟          成功→写入DisplayData→发队列通知
    │                              │
    └──────────────────────────────┘
                                   │
                              NTP对时 (每天1次)
                                   │
                              vTaskDelay(15分钟)
```

### API

| 用途 | API | 备注 |
|------|-----|------|
| IP定位 | `https://ipapi.co/json/` | 免费，无Key，1000次/天 |
| 天气 | `https://api.open-meteo.com/v1/forecast?latitude=XX&longitude=YY&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code` | 免费，无需Key |
| NTP | `pool.ntp.org` | 标准NTP协议 |

---

## 异常处理

| 异常 | 处理 |
|------|------|
| WiFi连接失败 | 状态栏显示⚠，用本地时间继续走，15分钟后重试 |
| 天气获取失败 | 显示`--°C`，保留上次数据最多30分钟 |
| IP定位失败 | 默认城市"北京"兜底 |
| NTP失败 | 用ESP32内部tick继续走时 |
| 电池过低 | OLED闪烁低电量图标 |
| 任务卡死 | WatchDog重启ESP32 |

### 关键原则

ClockTask 和 DisplayTask 完全独立于 NetworkTask。网络状况不影响时钟精度和屏幕刷新。

---

## 目录结构

```
weather_clock/
├── main/
│   ├── main.c
│   ├── tasks/
│   │   ├── display_task.c
│   │   ├── clock_task.c
│   │   ├── network_task.c
│   │   ├── power_task.c
│   │   └── watchdog_task.c
│   ├── drivers/
│   │   ├── oled_ssd1306.c
│   │   └── i2c.c
│   ├── fonts/
│   │   └── font_8x16.c
│   ├── network/
│   │   ├── wifi.c
│   │   └── http_client.c
│   └── shared/
│       ├── display_data.h
│       └── config.h
└── CMakeLists.txt
```

---

## 移植 & 升级方向

- 后续可引入 LVGL 做更丰富的界面
- 可参考 [OV-Watch](https://github.com/No-Chicken/OV-Watch) 的低功耗设计（STOP模式 ~1mA）
- 可加入 SmartConfig 实现运行时配网
- 可扩展传感器（温湿度、加速度计等）

---

## 约束

- 开发框架：ESP-IDF (内置 FreeRTOS)
- 语言：C
- 固件构建：CMake (ESP-IDF 标准)
- 无物理按键，单页面纯显示
- 电池供电，便携场景
