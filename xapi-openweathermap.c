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
#define TRACE_UNIT "xapi_openweathermap"

#include "util-logtrace.h"
#include "json-parser.h"
#include "portable.h"
#include "xapi.h"

#define API_KEY "a17c3c1d9c03230fa90bf4693ab4d63e"
#define API_HOST_URL "api.openweathermap.org"
#define API_CURRENT_WEATHER "/data/2.5/weather" /* q={city name} */

static char xapi_buff[128];

int
query_current_data( const char *city_name, current_weather_t *data )
{
  const char *http_body = NULL;

  memset( data, 0, sizeof(current_weather_t) );

  trace_assert( strlen(city_name) < sizeof(xapi_buff) - 3+7+32 );
  strcpy(xapi_buff, API_CURRENT_WEATHER);
  strcat(xapi_buff, "?q=");
  strcat(xapi_buff, city_name);
  strcat(xapi_buff, "&appid=");
  strcat(xapi_buff, API_KEY);

  int rc = http_request(API_HOST_URL, xapi_buff, -1, -1);

  if( !rc )
    rc = http_parse_response( &http_body );
  if( !rc )
    {
      JSON *current_weather = json_parse( http_body );
      if( current_weather && JSON_Object == current_weather->type )
        {
          JSON *node = current_weather->child;
          while( node )
            {
              if( 0 == strcmp(node->string, "weather") && JSON_Array == node->type )
                {
                  JSON *weather = node->child;
                  if( weather ) weather = weather->child;
                  while( weather )
                    {
                      if( 0 == strcmp(weather->string, "id") && JSON_Number == weather->type )
                        data->id = weather->valueint;
                      weather = weather->next;
                    }
                }
              else if( 0 == strcmp(node->string, "main") && JSON_Object == node->type )
                {
                  JSON *main_node = node->child;
                  while( main_node )
                    {
                      if( 0 == strcmp(main_node->string, "temp") && JSON_Number == main_node->type )
                        data->temp = main_node->valuedouble;
                      if( 0 == strcmp(main_node->string, "temp_min") && JSON_Number == main_node->type )
                        data->temp_min = main_node->valuedouble;
                      if( 0 == strcmp(main_node->string, "temp_min") && JSON_Number == main_node->type )
                        data->temp_max = main_node->valuedouble;
                      if( 0 == strcmp(main_node->string, "pressure") && JSON_Number == main_node->type )
                        data->pressure = main_node->valuedouble;
                      if( 0 == strcmp(main_node->string, "humidity") && JSON_Number == main_node->type )
                        data->humidity = main_node->valuedouble;
                      if( 0 == strcmp(main_node->string, "sea_level") && JSON_Number == main_node->type )
                        data->sea_level = main_node->valuedouble;
                      if( 0 == strcmp(main_node->string, "grnd_level") && JSON_Number == main_node->type )
                        data->grnd_level = main_node->valuedouble;
                      
                      main_node = main_node->next;
                    }
                }
              else if( 0 == strcmp(node->string, "wind") && JSON_Object == node->type )
                {
                  JSON *wind_node = node->child;
                  while( wind_node )
                    {
                      if( 0 == strcmp(wind_node->string, "speed") && JSON_Number == wind_node->type )
                        data->wind_speed = wind_node->valuedouble;
                      if( 0 == strcmp(wind_node->string, "deg") && JSON_Number == wind_node->type )
                        data->wind_deg = wind_node->valuedouble;

                      wind_node = wind_node->next;
                    }
                }
              else if( 0 == strcmp(node->string, "sys") && JSON_Object == node->type )
                {
                  JSON *sys_node = node->child;
                  while( sys_node )
                    {
                      if( 0 == strcmp(sys_node->string, "sunrise") && JSON_Number == sys_node->type )
                        data->sunrise = sys_node->valuedouble;
                      if( 0 == strcmp(sys_node->string, "sunset") && JSON_Number == sys_node->type )
                        data->sunset = sys_node->valuedouble;

                      sys_node = sys_node->next;
                    }
                }
              node = node->next;
            }

          trace_debug(("id: %d\n", data->id));
          trace_debug(("temp: %f\n", data->temp));
          trace_debug(("temp_min: %f\n", data->temp_min));
          trace_debug(("temp_min: %f\n", data->temp_min));
          trace_debug(("pressure: %f\n", data->pressure));
          trace_debug(("humidity: %f\n", data->humidity));
          trace_debug(("sea_level: %f\n", data->sea_level));
          trace_debug(("grnd_level: %f\n", data->grnd_level));
          trace_debug(("wind_speed: %f\n", data->wind_speed));
          trace_debug(("wind_deg: %f\n", data->wind_deg));
          trace_debug(("sunrise: %f\n", data->sunrise));
          trace_debug(("sunset: %f\n", data->sunset));

          json_delete( current_weather );
          return 0;
        }
      return -WERR_PARSE_DATABASE;
    }

  return rc;
}
