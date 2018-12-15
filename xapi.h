/*
 *  NanoRadio (Open source IoT hardware)
 *
 *  This project is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License(GPL)
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This project is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#ifndef XAPI_H_
#define XAPI_H_

#include "http-protocol.h"

typedef struct
{
  int id;
  float temp;
  float temp_min;
  float temp_max;
  float pressure;
  float humidity;
  float sea_level;
  float grnd_level;
  float wind_speed;
  float wind_deg;
  float sunrise;
  float sunset;
} current_weather_t;


int query_current_data(const char *city, current_weather_t *data);
int query_rss(const char *host, const char *file);

#endif
