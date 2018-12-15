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
#define TRACE_UNIT "xapi"

#include "portable.h"
#include "util-logtrace.h"
#include "tcp-socket.h"

#include "http-protocol.h"

#define HTTP_MIN(x, y) ((x) > (y) ? (y) : (x))

static char
http_isdigit(char ch)
{
  return ((ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F') || (ch >= '0' && ch <= '9'));
}

static int
http_ctn(char ch)
{
  return (ch >= 'a' ? ch - 'a' + 10 : ch >= 'A' ? ch - 'A' + 10 : ch - '0');
}

static int
http_atoi(const char *p, int radix)
{
  int num = 0;
  if( !p ) return 0;
  
  while( http_isdigit(*p) )
    {
      num = num * radix + http_ctn(*p);
      p++;
    }
  return num;
}

static void
http_itoa(char *buff, int size, int num)
{
  char t;
  char *p, *firstdig = buff;
  int pos = 0;
  if( !num ) buff[pos++] = '0';
  else
    while( num && pos < size )
      {
        buff[pos++] = '0' + (num % 10);
        num /= 10;
      }
  p = buff+pos;

  *p-- = '\0';
  do {  /* reverse digitals */
    t = *p;
    *p = *firstdig;
    *firstdig = t;
    --p;

    ++firstdig;
  } while (firstdig < p);
}

/* Parse URL, destroying the original string buffer as well.
 * the url parameter must be a writable buffer, not a constant string stored in readonly data segment.
 */
int
parse_url(char *url, const char **pproto, const char **phost, const char **pfile, int *pport)
{
  char *p;
  while(isspace(*url)) url++;
  
  if( (p = strstr(url, "://")) )
    {
      *p = '\0';
      if(pproto) *pproto = url;
      url = p + 3;
      
      if( (p = strstr(url, ":")) ) /* found a port number specified */
        {
          p++;
          if( http_isdigit(*p) )
            {
              if(pport) *pport = http_atoi(p, 10);
            }
          else
            return -WERR_FAILED;
        }
      else
        {
          if(pport) *pport = 80;
        }
      
      if( (p = strstr(url, "/")) ) /* found a resource path specified */
        {
          *p++ = '\0';
          if(phost) *phost = url;
          if(pfile) *pfile = p;
        }
      
      if(phost) *phost = url;
      
      return 0;
    }
    
  return -WERR_FAILED;
}

/* start/end specified the requested range in bytes. start = -1 when range field is invalid, end = -1 read through */
static int
http_send_request(http_t *http, int socket, const char *host, const char *file, int start, int end)
{
  trace_assert( strlen(host) + strlen(file) < sizeof(http->buff) - 44 - 48 );

  strcpy( http->buff, "GET " );
  strcat( http->buff, file );
  strcat( http->buff, " HTTP/1.1\r\n" );
  strcat( http->buff, "Host: " );
  strcat( http->buff, host );
  if( start < 0 )
    strcat( http->buff, "\r\n" );
  else
    {
      char byte_size[16];
      strcat( http->buff, "\r\nRange: bytes=" );
      http_itoa( byte_size, sizeof byte_size, start );
      strcat( http->buff, byte_size );
      strcat( http->buff, "-" );
      if( end > 0 )
        {
          http_itoa( byte_size, sizeof byte_size, end );
          strcat( http->buff, byte_size );
        }
      strcat( http->buff, "\r\n" );
    }
  strcat( http->buff, "Connection: close\r\n\r\n" );

//printf("header: %s\n", http_buff);

  return ws_socket_send( socket, http->buff, strlen(http->buff), 0 );
}

int
http_request(http_t *http, const char *host, const char *file, int port, int start, int end)
{
  int len;
  int socket = ws_socket_conn(host, port, 1);
  if( socket < 0 ) return socket;

  len = http_send_request(http, socket, host, file, start, end);
  if( len <= 0 ) return WERR_TCP_RECV;

  return socket;
}

static char *
get_http_field(const char *buff, char *field, char **end)
{
  char *pos = strstr( buff, field );
  if( !pos )
    return pos;
  pos += strlen( field ) + 1;
  while( isspace(*pos) )
    pos++;
  if( end )
    {
      if( !(*end = strstr(pos, "\r\n")) )
        return NULL;
    }
  return pos;
}

static int
dummy_event_at_length(char *at, int length)
{ return 0; WS_UNUSED(at); WS_UNUSED(length); }
static int
dummy_event_vstr(char *url)
{ return 0; WS_UNUSED(url); }

void
http_callbacks_init(http_event_procs_t *procs)
{
  procs->event_body = &dummy_event_at_length;
  procs->event_redirect = &dummy_event_vstr;
  procs->event_content_type = &dummy_event_at_length;
}

enum SMCODE
{
  SMCODE_BODY = 0,
  SMCODE_TERMINATE_CHUNK,
  SMCODE_CHUNK_SIZE
};

/* Return 0 if succeeded.
 * Return value < 0 if failed
 * Return HTTP status code (> 0) if a server error occurred.
 */
int
http_read_response(http_t *http, int socket, const http_event_procs_t *event_procs)
{
  int status;
  enum SMCODE smcode;
  int len, pos = 0;
  char *field_value = NULL;
  const char *response = http->buff;
  char *body;
  char *at;
  int block_length = 0;
  char stream_eof = 0;

#if 1
  while( sizeof(http->buff) - pos > 0 && (len = ws_socket_recv( socket, http->buff + pos, sizeof(http->buff) - pos, 0 )) )
    {
      if( len <= 0 ) break;
      pos += len;
    }
  
  if(!pos) return -WERR_TCP_RECV;
  
  while(isspace(*response)) response++;
// printf("response: %s\n", response);
  if( 0 != strncmp(response, "HTTP/1.1", sizeof("HTTP/1.1")-1) )
    return -WERR_HTTP_HEADER;
  response += 8;
  while(isspace(*response)) response++;

  status = http_atoi(response, 10);
  switch( status )
  {
    case 200: /* OK */
    case 206: /* Partial Content */
      break;
    case 301: /* Redirect */
      if( (field_value = get_http_field(http->buff, "Location", &at)) )
        {
          *at = '\0';
          if( (status = event_procs->event_redirect(field_value)) )
            return status;
        }
      return status;
      break;
    default:
      return status;
  }

  http->content_lenght = http_atoi( get_http_field(http->buff, "Content-Length:", &at), 10 );
  field_value = get_http_field(http->buff, "Content-Type", &at);
  event_procs->event_content_type(field_value, (int)(at - field_value));
  
  field_value = get_http_field(http->buff, "Transfer-Encoding", NULL);
  if( field_value && 0 == strncmp(field_value, "chunked", sizeof("chunked")-1) )
    http->chunked = 1;
  else
    http->chunked = 0;
  http->chunked_size = 0;
  
  body = strstr(http->buff, "\r\n\r\n");
  if(!body)
    return -WERR_HTTP_HEADER;
  body += 4;
  
  http->write_pos = 0;
  smcode = SMCODE_BODY;

  if( http->chunked )
    {
      trace_debug(("chunkd transfer\n"));
      smcode = SMCODE_CHUNK_SIZE;
    }

  len = sizeof(http->buff) - (int)(body - http->buff);
  memmove(http->buff, body, len);
#else

  http->write_pos = 0;
  http->content_lenght = 0;
  http->chunked = 1;
  http->chunked_size = 0;
  smcode = SMCODE_CHUNK_SIZE;
  
  FILE *fp = fopen("./test.txt", "r");
  
  len = fread(http->buff, 1, sizeof(http->buff), fp);
#endif
  http->write_pos = len;
  http->content_read_lenght = 0;
  at = http->buff;
  
#if 0
  FILE *fdump = fopen("./dump.txt", "wb");
  fwrite(http->buff, 1, sizeof(http->buff), fdump);
#endif
  for(;;)
    {
      char *p = http->buff, *pend = http->buff + (http->write_pos - 1);
     
      //printf("=========%s\n", http->buff);
      
      while(p <= pend)
        {
          //printf("===%s\n", p);
          switch(*p)
          {
            case '\r':
              if(smcode != SMCODE_BODY)
                {
                  p++;
                  continue;
                }
              break;
            case '\n':
              switch(smcode)
              {
                case SMCODE_CHUNK_SIZE:
                  if( http->chunked_size )
                    {
                      at = ++p;
                      
                      block_length = 0;
                      smcode = SMCODE_BODY; /* to read the body of current chunk */
                      continue;
                    }
                  break;
                  
                case SMCODE_TERMINATE_CHUNK:
                  http->chunked_size = 0;
                  smcode = SMCODE_CHUNK_SIZE;
                  p++;
                  continue;
                  
                default:
                  break;
              }
              break;
          }
          switch(smcode)
          {
            case SMCODE_BODY:
              if(http->chunked)
                {
                  block_length++;
                  if(p == pend || block_length == http->chunked_size )
                    {
                      if( (status = event_procs->event_body(at, block_length)) )
                        return status;
                      
                      if( block_length == http->chunked_size )
                        {
                          smcode = SMCODE_TERMINATE_CHUNK; /* to start a new chunk */
                        }
                      else if( p == pend )
                        {
                          http->chunked_size -= block_length;
                          goto next_sector;
                        }
                    }
                }
              else
                {
                  block_length++;
                  if( p == pend )
                    {
                      if( (status = event_procs->event_body(at, block_length)) )
                        return status;
                      
                      http->content_read_lenght += block_length;
                      goto next_sector;
                    }
                }
              break;
              
            case SMCODE_CHUNK_SIZE:
              if( !http_isdigit(*p) )
                {
                  trace_debug(("no chunk size, ch = %d\n", *p));
                  return -WERR_HTTP_HEADER;
                }
              
              http->chunked_size = http->chunked_size * 16 + http_ctn(*p);
              break;
              
            default:
              break;
          }
          
          p++;
        }
        
next_sector:
      if( stream_eof ) break;
      
      http->write_pos = 0;
      block_length = 0;
      at = http->buff;
      
      /* fill in the buffer */
      while( sizeof(http->buff) > http->write_pos )
      {
#if 1
        len = ws_socket_recv( socket, http->buff + http->write_pos, sizeof(http->buff) - http->write_pos, 0 );
#else
        len = fread(http->buff + http->write_pos, 1, sizeof(http->buff) - http->write_pos, fp);
#endif
#if 0
        fwrite(http->buff + http->write_pos, 1, sizeof(http->buff) - http->write_pos, fdump);
#endif
        if( len <= 0 )
          {
            stream_eof = 1;
            break;
          }
        http->write_pos += len;
      }

    };
  
  if(http->content_lenght && http->content_lenght != http->content_read_lenght)
    {
      return -WERR_TCP_RECV;
    }
  
  return 0;
}
