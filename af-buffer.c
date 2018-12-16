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

#include "util-task.h"
#include "af-buffer.h"

static int buf_read_pos;
static int buf_write_pos;
static int buf_fill;
static util_semaphore_t sem_read;
static util_semaphore_t sem_write;
static util_mutex_t mux;
static long buf_overflow_count, buf_underflow_count;

/* Low watermark where we restart the reading task. */
#define FIFO_LOWMARK (112*1024)

#if ENABLE(INNER_SRAM_BUFF)
#undef SPIRAMSIZE

#define SPIRAMSIZE 32000
  static char buffer[SPIRAMSIZE]; /* Utilize SRAM built in SoC */
# define spi_ram_init() while(0)
# define spi_ram_test() 1
# define spi_ram_write(pos, buf, n) memcpy(&buffer[pos], buf, n)
# define spi_ram_read(pos, buf, n) memcpy(buf, &buffer[pos], n)
#endif

int
audio_buffer_init(void)
{
  buf_read_pos = 0;
  buf_write_pos = 0;
  buf_fill = 0;
  buf_overflow_count = 0;
  buf_underflow_count = 0;
  
  sem_read = util_create_semaphore(0);
  sem_write = util_create_semaphore(0);
  mux = util_create_mutex();
  
  spi_ram_init();
  return spi_ram_test();
}

void
audio_buffer_reset(void)
{
  util_mutex_take(mux);
  buf_read_pos = 0;
  buf_write_pos = 0;
  buf_fill = 0;
  buf_overflow_count = 0;
  buf_underflow_count = 0;
  util_semaphore_give(sem_write);
  util_semaphore_take(sem_read);
  util_mutex_give(mux);
}

void
audio_buffer_read(char *buff, int len)
{
  int n;
  while (len > 0)
    {
      n = len;
      if (n>SPIREADSIZE) n = SPIREADSIZE;   /* don't read more than SPIREADSIZE */
      if (n>(SPIRAMSIZE-buf_read_pos))
        {
          n = SPIRAMSIZE - buf_read_pos;    /* don't read past end of buffer */
        }
        
      util_mutex_take(mux);
      if (buf_fill < n)
        {
          /* not enough data in FIFO. Wait till there's some written and try again. */
          buf_underflow_count++;
          util_mutex_give(mux);
          if (buf_fill < FIFO_LOWMARK) util_semaphore_take(sem_read);
        }
      else
        {
          spi_ram_read(buf_read_pos, buff, n);
          buff += n;
          len -= n;
          buf_fill -= n;
          buf_read_pos += n;
          if( buf_read_pos >= SPIRAMSIZE ) buf_read_pos=0;
          util_mutex_give(mux);
          util_semaphore_give(sem_write); /* Indicate writer thread there's some free room in the fifo */
        }
    }
}

void
audio_buffer_write(const char *buff, int buffLen)
{
  int n;
  while (buffLen > 0)
    {
      n = buffLen;
      
      if (n > SPIREADSIZE) n = SPIREADSIZE; /* don't read more than SPIREADSIZE */
      if (n > (SPIRAMSIZE - buf_write_pos)) /* don't read past end of buffer */
        {
          n = SPIRAMSIZE - buf_write_pos;
        }

      util_mutex_take(mux);
      if ((SPIRAMSIZE - buf_fill) < n)
        {
          /* not enough free room in FIFO. Wait till there's some read and try again. */
          buf_overflow_count++;
          util_mutex_give(mux);
          util_semaphore_take(sem_write);
          util_task_yield();
        }
      else
        {
          spi_ram_write(buf_write_pos, buff, n);
          buff += n;
          buffLen -= n;
          buf_fill += n;
          buf_write_pos += n;
          if (buf_write_pos >= SPIRAMSIZE) buf_write_pos = 0;
          util_mutex_give(mux);
          util_semaphore_give(sem_read); /* Tell reader thread there's some data in the fifo. */
        }
    }
}

/* get amount of bytes in use */
int
audio_buffer_fill(void)
{
  int ret;
  util_mutex_take(mux);
  ret = buf_fill;
  util_mutex_give(mux);
  return ret;
}

/* get amount of bytes free */
int
audio_buffer_free(void)
{ return (SPIRAMSIZE-audio_buffer_fill()); }

int
audio_buffer_size(void)
{ return SPIRAMSIZE; }

long
audio_buffer_get_overflow(void)
{
  long ret;
  util_mutex_take(mux);
  ret = buf_overflow_count;
  util_mutex_take(mux);
  return ret;
}

long
audio_buffer_get_underflow(void)
{
  long ret;
  util_mutex_take(mux);
  ret = buf_underflow_count;
  util_mutex_take(mux);
  return ret;
}
