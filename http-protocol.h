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
#ifndef HTTP_PROTOCOL_H_
#define HTTP_PROTOCOL_H_

typedef struct {
  char buff[1024]; /* a frame of HTTP header */
  int content_lenght;
  int content_read_lenght;
  char chunked;
  int chunked_size;
  int write_pos;
} http_t;

typedef struct {
  int (*event_body)(char *at, int length);
  int (*event_redirect)(char *url);
  int (*event_content_type)(char *at, int length);
} http_event_procs_t;

int http_request(http_t *http, const char *host, const char *file, int port, int start, int end);
int http_read_response(http_t *http, int socket, const http_event_procs_t *event_procs);

int parse_url(char *url, const char **pproto, const char **phost, const char **pfile, int *pport);

#endif
