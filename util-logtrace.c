/** @file
 * Log trace
 */

/*
 *  NanoRadio (Opensource IoT hardware)
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
#include "util-terminal.h"

#define log_printf        ws_printf

#if PORT(POSIX)
# define log_putc(c)       fputc( c, stdout )
# define log_getc()        (char)fgetc( stdin )
#else
# define log_putc(c)       ((void)c)
# define log_getc()        0
#endif

static const char *log_unit_name = 0;
static const char *log_unit_filename = 0;
static int log_unit_line = 0;
static ws_log_level_t log_level = LOG_INFO;


static
void dev_out( unsigned char c )
{ log_putc( c ); }

static
unsigned char dev_in( void )
{ return log_getc(); }

void
ws_log_init( void )
{
  ws_set_dev_out( dev_out );
  ws_set_dev_in( dev_in );
}

void
ws_log_set_unit( const char *unitname, const char *filename, int line )
{
  log_unit_name = unitname ? unitname : "all";
  log_unit_filename = filename ? filename : "unknown";
  log_unit_line = line;
}

void
ws_log_set_level( ws_log_level_t level )
{
  log_level = level;
}

void
ws_log_trace( const char *format, ... )
{
#if PORT(POSIX)
  char buff[1024];
  va_list args;
  va_start (args, format);

  vsnprintf(buff, sizeof(buff), format, args);
  va_end(args);

  if( log_level == LOG_PANIC )
    log_printf( "Panic! in %s: %d\n", log_unit_filename, log_unit_line );

  log_printf( "[%s] %s", log_unit_name, buff );
#endif
}

void
ws_panic( int rc )
{
#if PORT(POSIX)
  exit( 1 );
#endif
  while (1); /* loop forever until hard reseting */
}

