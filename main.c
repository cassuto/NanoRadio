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
event_redirect(char *url)
{
  return 0; /* todo */
}

static int
event_content_type(char *at, int length)
{
  return 0;
}

static int
event_body(char *at, int length)
{
  printf("%.*s", length, at);
  return 0;
}

int main(void)
{
  ws_log_init();

  //current_weather_t weather;
  //query_current_data( city_name, &weather );

  //query_rss("www.people.com.cn", "/rss/politics.xml");
  
  task_audio_init();
  task_audio_open("http.hz.qingting.fm", "/386.mp3", 80);
  
  http_t http;
  int stream_socket = http_request(&http, "nanoradio.github.io", "/api/channel/entry.1209.0", 80, -1, -1);
  http_event_procs_t procs =
    {
      .event_body = event_body,
      .event_redirect = event_redirect,
      .event_content_type = event_content_type
    };
  printf("RC=%d\n", http_read_response(&http, stream_socket, &procs));
  
  
  while(1);
  
  return 0;
}
