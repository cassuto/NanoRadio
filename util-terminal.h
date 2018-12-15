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

#ifndef UTIL_TERMINAL_H_
#define UTIL_TERMINAL_H_

void ws_putc (char c);
void ws_puts (const char* str);
void ws_printf (const char* fmt, ...);
void ws_xsprintf (char* buff, const char* fmt, ...);
void put_dump (const void* buff, unsigned long addr, int len, int width);

int ws_gets (char* buff, int len);

extern void (*term_func_out)(unsigned char);
extern unsigned char (*term_func_in)(void);

#define ws_set_dev_out(func) term_func_out = (void(*)(unsigned char))(func)
#define ws_set_dev_in(func) term_func_in = (unsigned char(*)(void))(func)

#endif //!defined(UTIL_TERMINAL_H_)
