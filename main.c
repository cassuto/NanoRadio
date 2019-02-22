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

#include "portable.h"
#include "util-logtrace.h"
#include "xapi.h"
#include "tasks.h"

const char *city_name = "Lijiang";

static int
entry_proc(const char *value, int length, void *opaque)
{
  printf("%.*s\n", length, value);
  return 0;
}

int main(void)
{
  ws_log_init();

  //current_weather_t weather;
  //query_current_data( city_name, &weather );

  //query_rss("www.people.com.cn", "/rss/politics.xml");
  radiolist_time_t radtime;
  radio_update_program(386, 1, 0);
  radio_program(entry_proc, NULL);
  radio_program_start_time(2, &radtime);
  printf("%d %d %d\n", radtime.hour, radtime.min, radtime.sec);
  return 0;
  
  printf("radio_channel_chunkinfo() = %d\n", radio_channel_chunkinfo(1209));
  printf("radio_update_channel() = %d\n", radio_update_channel(1209, 0));
  printf("id = %d\n", radio_channel(&entry_proc, NULL));
  int server_id = radio_server_id(0);
  int stream_id = radio_stream_id(0);
  const char *host, *file;

  printf("ser = %d\n", server_id);
  printf("e = %d\n", stream_id);
  
  printf("g = %d\n", radio_server(server_id, stream_id, &host, &file));
  printf("host = %s\n", host);
  printf("file = %s\n", file);
  
  task_audio_init();
  task_audio_open(host, file, 80);
  while(1);
  
  return 0;
}
