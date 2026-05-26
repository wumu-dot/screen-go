#ifndef CONFIG_H
#define CONFIG_H

#define WIFI_SSID           "your_ssid"
#define WIFI_PASSWORD       "your_password"
#define WIFI_MAX_RETRIES    5
#define WIFI_RETRY_DELAY_MS 2000

#define NTP_SERVER          "pool.ntp.org"
#define NTP_SYNC_INTERVAL   (24 * 3600)
#define CLOCK_TICK_MS       1000

#define NETWORK_INTERVAL_S  (15 * 60)
#define WEATHER_STALE_S     (30 * 60)

#define WEATHER_API_URL     "https://api.open-meteo.com/v1/forecast"
#define IPAPI_URL           "https://ipapi.co/json/"

#define DEFAULT_CITY        "Beijing"
#define DEFAULT_LAT         39.9042
#define DEFAULT_LON         116.4074

#define POWER_INTERVAL_S    30
#define ADC_CHANNEL         ADC1_CHANNEL_6
#define ADC_ATTEN           ADC_ATTEN_DB_11
#define VOLTAGE_DIVIDER     2.0
#define BATTERY_FULL_V      4.2
#define BATTERY_EMPTY_V     3.3

#define I2C_MASTER_SCL_IO   22
#define I2C_MASTER_SDA_IO   21
#define I2C_MASTER_FREQ_HZ  400000
#define I2C_MASTER_PORT     I2C_NUM_0
#define OLED_ADDR           0x3C
#define OLED_WIDTH          128
#define OLED_HEIGHT         64

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

#define WDT_TIMEOUT_S       10

#endif
