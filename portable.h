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

#ifndef PORTABLE_H_
#define PORTABLE_H_

/*
 * Configuration definitions
 */
/* #define USING_PORT_POSIX 1 */
#undef USING_PORT_POSIX

#define USING_PORT_ESP8266 1
/* #undef USING_PORT_ESP8266 */

/* #define USING_PORT_SOCKET 1 */
#undef USING_PORT_SOCKET

#define USING_PORT_LWIP 1
/* #undef USING_PORT_LWIP */

/* #define USING_PORT_WINSOCK2 1 */
#undef USING_PORT_WINSOCK2

/* #define USING_MBEDTLS 1 */
#undef USING_MBEDTLS

#define USING_SSL 1
/* #undef USING_SSL */

#define USING_PORT_FREE_RTOS 1
/* #undef USING_PORT_FREE_RTOS */

#define USING_ADIF_ESP_I2S_DSM 1
/* #undef USING_ADIF_ESP_I2S_DSM */


#define ENABLE_INNER_SRAM_BUFF 1
/* #undef ENABLE_INNER_SRAM_BUFF */

#define SPIREADSIZE 64

#define SPIRAMSIZE 8 * 1024 * 1024

#define CHUNK_SIZE 8192

#ifdef USING_PORT_ESP8266
# define ENABLE_BSS_ALIGN 1
#else
# undef ENABLE_BSS_ALIGN
#endif

#define PORT(type)  (defined USING_PORT_##type && USING_PORT_##type)

#define ENABLE(type)  (defined ENABLE_##type && ENABLE_##type)

#define USING(type)  (defined USING_##type && USING_##type)


/*
 * Platform-relative macros
 */
#if PORT(ESP8266)
# include "esp_common.h"

/*
 * GCC doesn't provide an appropriate macro for [u]intptr_t
 * For now, use __PTRDIFF_TYPE__
 */
#if defined(__PTRDIFF_TYPE__)
typedef signed __PTRDIFF_TYPE__ intptr_t;
typedef unsigned __PTRDIFF_TYPE__ uintptr_t;
#else
/*
 * Fallback to hardcoded values, 
 * should be valid on cpu's with 32bit int/32bit void*
 */
typedef signed long intptr_t;
typedef unsigned long uintptr_t;
#endif

#endif

#ifndef ICACHE_FLASH_ATTR
# define ICACHE_FLASH_ATTR
#endif
#define NC_P(p) ICACHE_FLASH_ATTR p

/*
 * Memory management helper functions
 */
#define ws_malloc( /* size_t */ size )  malloc(size)
#define ws_calloc( /* size_t */ size, /* size_t */ count )  calloc(size, count)
#define ws_realloc( /* void * */ mem, /* size_t */ newsize ) realloc(mem, newsize)
#define ws_free( /* void * */ mem )     free(mem)
#define ws_memcpy( /* void * */ dst, /* void * */ src, /* size_t */ size ) memcpy(dst, src, size)
#define ws_memset( /* void * */ dst, /* int ch */ ch, /* size_t */ size )  memset(dst, ch, size)
#ifndef ws_bzero
# define ws_bzero( /* void * */ dst, /* size_t */ size ) ws_memset((dst), 0, (size))
#endif

static inline char *ws_strdup( const char *src )
{
  char *mem = (char *)ws_malloc( strlen(src) + 1 );
  if( !mem ) return mem;
  strcpy( mem, src );
  return mem;
}

static inline char *ws_strndup( const char *src, size_t len )
{
  char *mem;
  size_t maxsize = strlen(src);
  mem = (char *)ws_malloc( (len < maxsize ? maxsize : len) + 1 );
  if( !mem ) return mem;
  memcpy( mem, src, len );
  return mem;
}

#define WS_UNUSED(x) ((void)x)

/*
 * Error codes
 */
#define WINF_STOPPED      (1)
#define WINF_SUCCESS      (0)
#define WERR_FAILED       (1)
#define WERR_NO_MEMORY    (2)
#define WERR_CONN_TIMEOUT (3)     /* connection timeout */
#define WERR_CONN_FATAL   (4)     /* unable to resolve name */
#define WERR_PORT         (5)     /* unable to connect to a particular port */
#define WERR_TCP_RECV     (6)
#define WERR_HTTP_HEADER  (7)
#define WERR_PARSE_DATABASE (8)
#define WERR_NO_DEVICES   (9)
#define WERR_OPEN_DEVICE  (10)
#define WERR_BUSY         (11)
#define WERR_UNKNOW_TYPE  (12)
#define WERR_BUFFER_OVERFLOW (13)

#endif //!defined(PORTABLE_H_)

#ifndef NO_RTL
# include <stdarg.h>
# include <stdlib.h>
# include <stdio.h>
# include <string.h>
# include <ctype.h>
# if ! PORT(ESP8266)
#  include <stdint.h>
#  include <errno.h>
# endif
#endif
