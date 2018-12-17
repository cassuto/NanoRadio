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
#define TRACE_UNIT "xapi-nanoradio"

#include "util-logtrace.h"
#include "portable.h"
#include "xapi.h"

static const char *api_host = "nanoradio.github.io"; /* Host domain of online database */

static const char *api_server_list = "/api/server/list.%d";
static const char *api_catalog_root = "/api/catalog/root";
static const char *api_catalog_entry = "/api/catalog/entry.%d.%d";
static const char *api_channel_info = "/api/channel/info.%d";
static const char *api_channel_entry = "/api/channel/entry.%d.%d";
static const char *api_info = "/api/info/%d";
static const char *api_program_info = "/api/program/info.%d.%d";
static const char *api_program_list = "/api/program/list.%d.%d.%d";

#define NANORADIO_API_CHUNK 4 * 1024

static char api_chunk[NANORADIO_API_CHUNK+1];
static int api_chunk_write_pos;
static int api_parsing_pos;

enum CURRENT_API {
  API_CATALOG_ROOT,
  API_CATALOG_LIST
};

static enum CURRENT_API api_current;

static int
event_body(char *at, int length)
{
  if( api_chunk_write_pos >= sizeof api_chunk-1 )
    return 0;
  else
    {
      ws_memcpy( &api_chunk[api_chunk_write_pos], at, length );
      api_chunk_write_pos += length;
    }
  return 0;
}

static int
api_request_chunk(const char *api_file, enum CURRENT_API current)
{
  int rc;
  http_t http;
  http_event_procs_t procs;
  http_callbacks_init(&procs);
  procs.event_body = event_body;
  
  api_chunk_write_pos = 0;
  api_current = current;
  
  if( (rc = http_request(&http, api_host, api_file, 443, -1, -1) ) )
    return rc;
  
  if( (rc = http_read_response(&http, &procs)) )
    return rc;
  
  api_chunk[api_chunk_write_pos] = '\0';
  return 0;
}

int
update_root_catalog(void)
{
  return api_request_chunk(api_catalog_root, API_CATALOG_ROOT);
}

void
radiolist_reset_parser(void)
{
  api_parsing_pos = 0;
}

int
radiolist_items_count(void)
{
  register int count = 0;
  const char *p = api_chunk;
  while( *p )
    {
      if( *p++ == '\n' )
        count++;
    }
  return count;
}

enum rdlst_parser_state
{
  PARSE_INITIAL = 0,
  PARSE_STRING,
  PARSE_NUMBER
};

int
radiolist_parse_token(int line, radiolist_token_t *token)
{
  register char ch;
  const char *line_end = api_chunk + api_parsing_pos;
  enum rdlst_parser_state parser_state = PARSE_INITIAL;
  
  token->type = RADLST_UNKNOWN;

  if( api_parsing_pos >= sizeof api_chunk )
    return 1; /* reach at the termination of current chnk */
  
  while( api_parsing_pos < sizeof api_chunk )
    {
      ch = api_chunk[api_parsing_pos];
      switch( parser_state )
        {
          case PARSE_INITIAL: /* to incidate the next state of FSM */
            switch( ch )
              {
                case '"':
                  token->length = 0;
                  token->type = RADLST_STRING;
                  token->u.string = api_chunk + api_parsing_pos + 1;
                  parser_state = PARSE_STRING;
                  goto parse_next;
                  
                case '\r':
                case '\n':
                case ' ':
                  goto parse_next;
                  break;
                  
                default:
                  if( isdigit(ch) )
                    {
                      token->type = RADLST_NUMBER;
                      token->u.number = 0;
                      parser_state = PARSE_NUMBER;
                      api_parsing_pos--;
                    }
                  else
                    {
                      trace_error(("radiolist_parse_token() unexp token.%d\n", api_parsing_pos));
                      return -WERR_PARSE_DATABASE;
                    }
              }
              
          case PARSE_STRING: /* In parsing of string sequence */
            if( ch == '"' )
              {
                token->length = api_parsing_pos - (token->u.string - api_chunk);
                parser_state = PARSE_INITIAL;
                api_parsing_pos++;
                goto parse_end;
              }
            break;
            
          case PARSE_NUMBER: /* In parsing of number value */
            switch( ch )
              {
                case '\r':
                case '\n':
                case ' ':
                  parser_state = PARSE_INITIAL; /* a number could be terminated by line separator */
                  goto parse_end;
                  
                default:
                  if( isdigit(ch) )
                    {
                      token->u.number *= 10;
                      token->u.number += ch - '0';
                    }
                  else
                    {
                      trace_error(("radiolist_parse_token() Invalid number.\n"));
                      return -WERR_PARSE_DATABASE;
                    }
              }
            break;
        }
parse_next:
      api_parsing_pos++;
    }

parse_end:
  return (parser_state == PARSE_INITIAL) ? 0 : -WERR_PARSE_DATABASE;
}
