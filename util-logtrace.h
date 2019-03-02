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
#ifndef UTIL_LOGTRACE_H_
#define UTIL_LOGTRACE_H_

#include "portable.h"

#ifndef TRACE_UNIT
# define TRACE_UNIT 0
#endif

typedef enum ws_log_level_e
{
  LOG_PANIC = 0,
  LOG_ERROR,
  LOG_WARNING,
  LOG_INFO,
  LOG_DEBUG
} ws_log_level_t;

#define ws_log_lock()
#define ws_log_unlock()

void ws_log_init( void );
void ws_log_set_unit( const char *unitname, const char *filename, int line );
void ws_log_set_level( ws_log_level_t level );
//#if PORT(POSIX)
void ws_log_trace( const char *format, ... );
//#else
//# define ws_log_trace printf
//#endif

void ws_panic( int rc );

#define trace_panic(msg) \
  do { \
    ws_log_lock(); \
    ws_log_set_unit (TRACE_UNIT, __FILE__, __LINE__); \
    ws_log_set_level (LOG_PANIC); \
    ws_log_trace msg ; \
    ws_log_unlock(); \
  } while(0)

#define trace_error(msg) \
  do { \
    ws_log_lock(); \
    ws_log_set_unit (TRACE_UNIT, __FILE__, __LINE__); \
    ws_log_set_level (LOG_ERROR); \
    ws_log_trace msg ; \
    ws_log_unlock(); \
  } while(0)

#define trace_warning(msg) \
  do { \
    ws_log_lock(); \
    ws_log_set_unit (TRACE_UNIT, __FILE__, __LINE__); \
    ws_log_set_level (LOG_WARNING); \
    ws_log_trace msg ; \
    ws_log_unlock(); \
  } while(0)

#define trace_info(msg) \
  do { \
    ws_log_lock(); \
    ws_log_set_unit (TRACE_UNIT, __FILE__, __LINE__); \
    ws_log_set_level (LOG_INFO); \
    ws_log_trace msg ; \
    ws_log_unlock(); \
  } while(0)

#define trace_debug(msg) \
  do { \
    ws_log_lock(); \
    ws_log_set_unit (TRACE_UNIT, __FILE__, __LINE__); \
    ws_log_set_level (LOG_DEBUG); \
    ws_log_trace msg ; \
    ws_log_unlock(); \
  } while(0)

#define TRACE_ASSERT(expr) \
  do { \
    if (!(expr)) \
      { \
        trace_panic(("assert failure. expt = %s\n", #expr)); \
        ws_panic(1); \
      } \
  } while(0)

static inline void
trace_assert( int expr )
{
  if (!(expr)) \
    { \
      trace_panic(("assert failure.\n")); \
      ws_panic(1); \
    } \
}

#endif //!defined(UTIL_LOGTRACE_H_)
