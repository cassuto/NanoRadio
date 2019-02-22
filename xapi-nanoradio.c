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
static const char *api_catalog_entry = "/api/catalog/entry.%d";
static const char *api_channel_info = "/api/channel/info.%d";
static const char *api_channel_entry = "/api/channel/entry.%d.%d";
/* static const char *api_info = "/api/info/%d"; */
static const char *api_program_info = "/api/program/info.%d.%d";
static const char *api_program_list = "/api/program/list.%d.%d.%d";

#define NANORADIO_API_CHUNK 4 //4 * 1024

static char api_chunk[NANORADIO_API_CHUNK+1];
static char api_buff[4/*512*/];
static int api_chunk_write_pos;
static int api_parsing_pos;

enum CURRENT_API {
  API_CATALOG_ROOT,
  API_CATALOG_ENTRY,
  API_CHANNEL_INFO,
  API_CHANNEL_ENTRY,
  API_SERVER_LIST,
  API_PROGRAM_INFO,
  API_PROGRAM_LIST
};

static enum CURRENT_API api_current;

static int
NC_P(event_body)(char *at, int length)
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
NC_P(api_request_chunk)(const char *api_file, enum CURRENT_API current)
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

void
NC_P(radiolist_reset_parser)(void)
{
  api_parsing_pos = 0;
}

void
NC_P(radiolist_parser_goto)(int index)
{
  const char *p = api_chunk, *offset = api_chunk;
  radiolist_reset_parser();
  index++;
  while( *p )
    {
      if( *p++ == '\n' )
        {
          index--;
          if( index )
            offset = p;
          else
            {
              api_parsing_pos = (int)(offset - api_chunk);
              return;
            }
        }
    }
}

int
NC_P(radiolist_entries_count)(void)
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
NC_P(radiolist_parse_token)(radiolist_token_t *token)
{
  register char ch;
  enum rdlst_parser_state parser_state = PARSE_INITIAL;
  char parsetime = 0;
  
  token->type = RADLST_UNKNOWN;

  if( api_parsing_pos >= api_chunk_write_pos )
    return 1; /* reach at the termination of current chnk */
  
  while( api_parsing_pos < api_chunk_write_pos )
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
                      trace_error(("radiolist_parse_token() unexp token. (%d)\n", ch));
                      return -WERR_PARSE_DATABASE;
                    }
              }
            break;
              
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
                  
                case ':':
                  if( !parsetime )
                    {
                      token->type = RADLST_TIME;
                      token->u.time.hour = token->u.number;
                    }
                  parsetime++;
                  break;
                  
                default:
                  if( isdigit(ch) )
                    {
                      int *dst = NULL;
                      switch( parsetime )
                        {
                          case 0:
                            dst = &token->u.number;
                            break;
                            
                          case 1:
                            dst = &token->u.time.min;
                            break;
                            
                          case 2:
                            dst = &token->u.time.sec;
                            break;
                            
                          default:
                            trace_error(("radiolist_parse_token() Invalid time.\n"));
                            return -WERR_PARSE_DATABASE;
                        }
                      
                      *dst *= 10;
                      *dst += ch - '0';
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

#define CALL_PARSE_TOKEN(rc) \
  { int _rc = (rc); \
    if(_rc == 1) \
      break; \
    else if(_rc) \
      return _rc; \
  }

int
NC_P(radio_update_root_catalog)(void)
{
  return api_request_chunk(api_catalog_root, API_CATALOG_ROOT);
}

/* required update_root_catalog() */
int
NC_P(radio_root_catalog)(pfn_radio_entry_s entry_callback, void *opaque)
{
  int rc;
  radiolist_token_t token;
  if( api_current != API_CATALOG_ROOT ) return -WERR_FAILED;

  radiolist_reset_parser();
  for(;;)
    {
      CALL_PARSE_TOKEN( radiolist_parse_token(&token) ); /* <catalog_name> field */
      if( token.type == RADLST_STRING )
        {
          if( (rc = entry_callback(token.u.string, token.length, opaque)) )
            return rc;
        }
    }
  return 0;
}

/* required update_root_catalog() */
int
NC_P(radio_root_catalog_id)(int index)
{
  int rc;
  radiolist_token_t token;
  if( api_current != API_CATALOG_ROOT ) return -WERR_FAILED;

  radiolist_parser_goto(index);

  if( (rc = radiolist_parse_token(&token)) ) /* skip <catalog_name> field */
    return rc;
  if( (rc = radiolist_parse_token(&token)) ) /* <catalog_id> field */
    return rc;
  if( token.type == RADLST_NUMBER )
    return token.u.number;
  return -WERR_FAILED;
}

int
NC_P(radio_update_catalog)(int catalog_id)
{
  if( snprintf(api_buff, sizeof api_buff, api_catalog_entry, catalog_id) < 0 )
    return -WERR_BUFFER_OVERFLOW;
  return api_request_chunk(api_buff, API_CATALOG_ENTRY);
}

/* required update_catalog() */
int
NC_P(radio_catalog)(pfn_radio_entry_s entry_callback, void *opaque)
{
  int rc;
  radiolist_token_t token;
  if( api_current != API_CATALOG_ROOT ) return -WERR_FAILED;

  radiolist_reset_parser();
  for(;;)
    {
      CALL_PARSE_TOKEN( radiolist_parse_token(&token) ) /* <catalog_id> field */
      CALL_PARSE_TOKEN( radiolist_parse_token(&token) ) /* <catalog_name> field */
      if( token.type == RADLST_STRING )
        {
          if( (rc = entry_callback(token.u.string, token.length, opaque)) )
            return rc;
        }
      else
        return -WERR_PARSE_DATABASE;
    }
  return 0;
}

/* required update_catalog() */
int
NC_P(radio_catalog_id)(int index)
{
  int rc;
  radiolist_token_t token;
  if( api_current != API_CATALOG_ROOT ) return -WERR_FAILED;

  radiolist_parser_goto(index);

  if( (rc = radiolist_parse_token(&token)) ) /* <_id> field */
    return rc;
  if( token.type == RADLST_NUMBER )
    return token.u.number;
  return -WERR_PARSE_DATABASE;
}

/* return < 0 if failed, otherwise the number of chunks */
int
NC_P(radio_channel_chunkinfo)(int channel_id)
{
  int rc;
  radiolist_token_t token;

  if( snprintf(api_buff, sizeof api_buff, api_channel_info, channel_id) < 0 )
    return -WERR_BUFFER_OVERFLOW;
  if( (rc = api_request_chunk(api_buff, API_CHANNEL_INFO)) )
    return rc;

  radiolist_reset_parser();
  if( (rc = radiolist_parse_token(&token)) ) /* <chunk_count> field */
    return rc;
  if( token.type == RADLST_NUMBER )
    {
      return token.u.number;
    }
  return -WERR_PARSE_DATABASE;
}

int
NC_P(radio_update_channel)(int channel_id, int chunk_id)
{
  int rc;
  if( snprintf(api_buff, sizeof api_buff, api_channel_entry, channel_id, chunk_id) < 0 )
    return -WERR_BUFFER_OVERFLOW;
  if( (rc = api_request_chunk(api_buff, API_CHANNEL_ENTRY)) )
    return rc;
  return 0;
}

/* required radio_update_channel() */
int
NC_P(radio_channel)(pfn_radio_entry_s entry_callback, void *opaque)
{
  int rc;
  radiolist_token_t token;
  if( api_current != API_CHANNEL_ENTRY ) return -WERR_FAILED;

  radiolist_reset_parser();
  for(;;)
    {
      CALL_PARSE_TOKEN( radiolist_parse_token(&token) ) /* skip <server_id> field */
      CALL_PARSE_TOKEN( radiolist_parse_token(&token) ) /* skip <stream_id> field */
      CALL_PARSE_TOKEN( radiolist_parse_token(&token) ) /* skip <url> field */
      CALL_PARSE_TOKEN( radiolist_parse_token(&token) ) /* <channel_name> field */
      
      if( token.type == RADLST_STRING )
        {
          if( (rc = entry_callback(token.u.string, token.length, opaque)) )
            return rc;
        }
      else
        return -WERR_PARSE_DATABASE;
    }
  return 0;
}

/* required radio_update_channel() */
int
NC_P(radio_server_id)(int index)
{
  int rc;
  radiolist_token_t token;
  if( api_current != API_CHANNEL_ENTRY ) return -WERR_FAILED;

  radiolist_parser_goto(index);

  if( (rc = radiolist_parse_token(&token)) ) /* <server_id> field */
    return rc;
  if( token.type == RADLST_NUMBER )
    return token.u.number;
  return -WERR_PARSE_DATABASE;
}

/* required radio_update_channel() */
int
NC_P(radio_stream_id)(int index)
{
  int rc;
  radiolist_token_t token;
  if( api_current != API_CHANNEL_ENTRY ) return -WERR_FAILED;

  radiolist_parser_goto(index);

  if( (rc = radiolist_parse_token(&token)) ) /* skip <server_id> field */
    return rc;
  if( (rc = radiolist_parse_token(&token)) ) /* <stream_id> field */
    return rc;
  if( token.type == RADLST_NUMBER )
    return token.u.number;
  return -WERR_PARSE_DATABASE;
}

static int
NC_P(apply_url_pattern)(char *buf, int buffsize, int stream_id)
{
  while( *buf )
    {
      if( *buf == '$' && strncmp(buf, "$id$", 4) == 0 )
        {
          char t;
          char *offset = buf;
          char *p = buf;
          char *lastdigit, *lastbuff = buf + buffsize;
          int slicelen, cnt = 0;
          int num = stream_id;
          while( *(++buf) != '$' ); /* point out the end position of the pattern block closed by a pair of '$' */
          
          if( !num )
            cnt = 1;
          else
            while( num ) { cnt++; num /= 10; }
          
          if( (lastdigit = p + cnt) + (slicelen = (int)(buf + 1 - offset) + 1) >= lastbuff ) /* validate if there is enough room for the replacement */
            return -1;
          memmove(lastdigit, buf + 1, slicelen);
          
          num = stream_id;
          if( !num ) *p++ = '0';
          else
            {
              while( num && p <= lastdigit )
                {
                  *p++ = '0' + (num % 10);
                  num /= 10;
                }
              if( num && p == buf )
                return -WERR_BUFFER_OVERFLOW;
            }
          p--;
          do {  /* reverse digitals */
            t = *p;
            *p = *offset;
            *offset = t;
            --p;

            ++offset;
          } while (offset < p);
          break;
        }
      buf++;
    }
  return 0;
}

/* required radio_update_server(), using common buffer */
int
NC_P(radio_server)(int server_id, int stream_id, const char **host, const char **file)
{
  int rc, pos;
  radiolist_token_t token;
  
  if( snprintf(api_buff, sizeof api_buff, api_server_list, server_id) < 0 )
    return -WERR_BUFFER_OVERFLOW;
  if( (rc = api_request_chunk(api_buff, API_SERVER_LIST)) )
    return rc;

  radiolist_reset_parser();

  if( (rc = radiolist_parse_token(&token)) ) /* skip <host> field */
    return rc;
  if( token.type == RADLST_STRING )
    {
      char *file_buf;
      if( token.length >= sizeof api_buff )
        return -WERR_BUFFER_OVERFLOW;
      memcpy(api_buff, token.u.string, token.length);
      api_buff[(pos = token.length)] = 0;
      
      *host = api_buff;
      file_buf = &api_buff[++pos];
      
      if( (rc = radiolist_parse_token(&token)) ) /* <file> field */
        return rc;
      if( token.type == RADLST_STRING )
        {
          pos += token.length;
          if( pos >= sizeof api_buff )
            return -WERR_BUFFER_OVERFLOW;
          memcpy(file_buf, token.u.string, token.length);
          api_buff[pos] = 0;
          
          if( !apply_url_pattern(file_buf, sizeof api_buff, stream_id) )
            {
              *file = file_buf;
              return 0;
            }
        }
    }
  *host = NULL;
  *file = NULL;
  return -WERR_PARSE_DATABASE;
}

/* return < 0 if failed, otherwise the number of chunks */
int
NC_P(radio_program_chunkinfo)(int channel_id, int day_id)
{
  int rc;
  radiolist_token_t token;

  if( snprintf(api_buff, sizeof api_buff, api_program_info, channel_id, day_id) < 0 )
    return -WERR_BUFFER_OVERFLOW;
  if( (rc = api_request_chunk(api_buff, API_PROGRAM_INFO)) )
    return rc;

  radiolist_reset_parser();
  if( (rc = radiolist_parse_token(&token)) ) /* <chunk_count> field */
    return rc;
  if( token.type == RADLST_NUMBER )
    {
      return token.u.number;
    }
  return -WERR_PARSE_DATABASE;
}

int
NC_P(radio_update_program)(int channel_id, int day_id, int chunk_id)
{
  int rc;
  if( snprintf(api_buff, sizeof api_buff, api_program_list, channel_id, day_id, chunk_id) < 0 )
    return -WERR_BUFFER_OVERFLOW;
  if( (rc = api_request_chunk(api_buff, API_PROGRAM_LIST)) )
    return rc;
  return 0;
}

/* required radio_update_program() */
int
NC_P(radio_program)(pfn_radio_entry_s entry_callback, void *opaque)
{
  int rc;
  radiolist_token_t token;
  if( api_current != API_PROGRAM_LIST ) return -WERR_FAILED;

  radiolist_reset_parser();

  for(;;)
    {
      CALL_PARSE_TOKEN( radiolist_parse_token(&token) ); /* skip <start_time> field */
      CALL_PARSE_TOKEN( radiolist_parse_token(&token) ); /* skip <end_time> field */
      CALL_PARSE_TOKEN( radiolist_parse_token(&token) ); /* skip <program_name> field */
      
      if( token.type == RADLST_STRING )
        {
          if( (rc = entry_callback(token.u.string, token.length, opaque)) )
            return rc;
        }
      else
        return -WERR_PARSE_DATABASE;
    }
  return 0;
}

/* required radio_update_program() */
int
NC_P(radio_program_start_time)(int index, radiolist_time_t *radtime)
{
  int rc;
  radiolist_token_t token;
  if( api_current != API_PROGRAM_LIST ) return -WERR_FAILED;

  radiolist_parser_goto(index);

  if( (rc = radiolist_parse_token(&token)) ) /* <start_time> field */
    return rc;

  if( token.type == RADLST_TIME )
    {
      *radtime = token.u.time;
      return 0;
    }
  return -WERR_PARSE_DATABASE;
}

/* required radio_update_program() */
int
NC_P(radio_program_end_time)(int index, radiolist_time_t *radtime)
{
  int rc;
  radiolist_token_t token;
  if( api_current != API_PROGRAM_LIST ) return -WERR_FAILED;

  radiolist_parser_goto(index);

  if( (rc = radiolist_parse_token(&token)) ) /* <start_time> field */
    return rc;
  if( (rc = radiolist_parse_token(&token)) ) /* <start_time> field */
    return rc;

  if( token.type == RADLST_TIME )
    {
      *radtime = token.u.time;
      return 0;
    }
  return -WERR_PARSE_DATABASE;
}
