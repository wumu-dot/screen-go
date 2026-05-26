#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H
#include <stdint.h>
char *http_get(const char *url);
uint8_t wmo_to_weather_code(int wmo_code);
#endif
