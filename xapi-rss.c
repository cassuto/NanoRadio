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
#define TRACE_UNIT "xapi_rss"

#include "util-logtrace.h"
#include "portable.h"
#include "xapi.h"

static int
request(const char *host, const char *file, int start, int end, const char **http_body)
{
  int rc = http_request(host, file, start, end);
  if( !rc )
    return http_parse_response( http_body );
  return rc;
}

#define XML_LABEL(var, start, symbol) \
  do { \
    var = strstr( start, symbol ); \
    if( !var ) \
      return -WERR_PARSE_DATABASE; \
    var += sizeof(symbol)-1; \
  } while(0)

#define XML_CLOSELABEL(var, start, symbol) \
  do { \
    var = strstr( start, symbol ); \
    if( !var ) \
      return -WERR_PARSE_DATABASE; \
  } while(0)

int
query_rss(const char *host, const char *file)
{
  const char *http_body = NULL;
  int rc = request(host, file, -1, -1, &http_body);
  if( !rc )
    {
      const char *title_label, *title_end;
      const char *channel_label = strstr( http_body, "<channel>" );
      const char *item_lable;
      const char *pubdate_label, *pubdate_end;
      const char *description_label, *description_end;

      int next_pos = 0;

      char *rss_title = NULL;
      if( !channel_label )
        return -WERR_PARSE_DATABASE;
      channel_label += sizeof("<channel>")-1;

      XML_LABEL( title_label, channel_label, "<title>" );
      XML_CLOSELABEL( title_end, title_label, "</title>" );
      rss_title = ws_strndup(title_label, title_end-title_label);

      XML_LABEL( item_lable, title_end, "<item>" );

      next_pos = item_lable - http_body;
      if( (rc = request(host, file, next_pos, -1, &http_body)) )
        return rc;

      printf("body: %s\n", http_body);

      trace_debug(("Title: %s\n", rss_title));

      XML_LABEL( title_label, http_body, "<title>" );
      XML_CLOSELABEL( title_end, title_label, "</title>" );

      XML_LABEL( pubdate_label, title_end, "<pubDate>");
      XML_CLOSELABEL( pubdate_end, pubdate_label, "</pubDate>");

      XML_LABEL( description_label, title_end, "<description>" );
      description_end = strstr(description_label, "</description>" );
      if( !description_end )
        return -WERR_FAILED; /* fixme! processing long text */

      char *item_title = ws_strndup(title_label, title_end-title_label);
      char *item_pubdate = ws_strndup(pubdate_label, pubdate_end-pubdate_label);;
      char *item_description = ws_strndup(description_label, description_end-description_label);

      trace_debug(("\titem_title: %s\n", item_title));
      trace_debug(("\titem_pubdate: %s\n", item_pubdate));
      trace_debug(("\titem_desc: %s\n", item_description));
    }
  return rc;
}
