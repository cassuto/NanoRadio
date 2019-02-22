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


typedef enum
{
  RADLST_UNKNOWN = 0,
  RADLST_STRING,
  RADLST_NUMBER,
  RADLST_TIME
} radiolist_type_t;

typedef struct
{
  int hour; /* bitfield here if the size of other values in union is less than 24 bytes */
  int min;
  int sec;
} radiolist_time_t;

typedef struct
{
  radiolist_type_t type;
  int length;
  union
  {
    int number;
    radiolist_time_t time;
    const char *string;
  } u;
} radiolist_token_t;

typedef int (*pfn_radio_entry_s)(const char *value, int length, void *opaque);

int NC_P(radiolist_entries_count)(void);

void NC_P(radiolist_reset_parser)(void);
void NC_P(radiolist_parser_goto)(int index);
int NC_P(radiolist_parse_token)(radiolist_token_t *token);

int NC_P(radio_update_root_catalog)(void);
int NC_P(radio_root_catalog)(pfn_radio_entry_s entry_callback, void *opaque);
int NC_P(radio_root_catalog_id)(int index);

int NC_P(radio_update_catalog)(int catelog_id);
int NC_P(radio_catalog)(pfn_radio_entry_s entry_callback, void *opaque);
int NC_P(radio_catalog_id)(int index);

int NC_P(radio_channel_chunkinfo)(int channel_id);
int NC_P(radio_update_channel)(int channel_id, int chunk_id);
int NC_P(radio_channel)(pfn_radio_entry_s entry_callback, void *opaque);

int NC_P(radio_server)(int server_id, int stream_id, const char **host, const char **file);

int NC_P(radio_program_chunkinfo)(int channel_id, int day_id);
int NC_P(radio_update_program)(int channel_id, int day_id, int chunk_id);
int NC_P(radio_program)(pfn_radio_entry_s entry_callback, void *opaque);
int NC_P(radio_program_start_time)(int index, radiolist_time_t *radtime);
int NC_P(radio_program_end_time)(int index, radiolist_time_t *radtime);

#endif
