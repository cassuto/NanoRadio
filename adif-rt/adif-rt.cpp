/*
 *  WebClock (Open source IoT hardware)
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

#define TRACE_UNIT "rtds"

extern "C" {
#include "af-interface.h"
#include "util-logtrace.h"

}
#include "RtAudio.h"

static RtAudio *audio = 0l;
static int channels = 2;
static int m_buffersize = 0;
static int m_outburst = CHUNK_SIZE;
static unsigned char *buffer;
// may only be modified by SDL's playback thread or while it is stopped
static volatile int read_pos;
// may only be modified by audio thread
static volatile int write_pos;


extern "C" int
ADIF_CB(adif_rt_init)(int opaque)
{ WS_UNUSED(opaque);
  if( audio ) delete audio;
  audio = new RtAudio;
  
  if( audio->getDeviceCount() < 1 )
    {
      return -WERR_NO_DEVICES;
    }
    
  m_buffersize = 256 * 41400 * 2 + CHUNK_SIZE; //!fixme?
  m_outburst = CHUNK_SIZE;
  trace_info(("buf size = %d chunk size = %d\n",m_buffersize, m_outburst));

  /* Allocate ring-buffer memory */
  buffer = (unsigned char *)ws_malloc(m_buffersize);
  if (!buffer)
      return WERR_NO_MEMORY;
  read_pos = 0;
  write_pos = 0;
  return 0;
}

extern "C" void
ADIF_CB(adif_rt_uninit)(int opaque)
{ WS_UNUSED(opaque);
  if ( audio->isStreamOpen() ) audio->closeStream();
  delete audio;
  audio = 0l;
}

/* may only be called by SDL's playback thread
 * return value may change between immediately following two calls,
 * and the real number of buffered bytes might be larger!
 */
static int
buf_used(void) {
  int used = write_pos - read_pos;
  if (used < 0) used += m_buffersize/*BUFFSIZE*/;
  return used;
}

/* may only be called by audio thread
 * return value may change between immediately following two calls,
 * and the real number of free bytes might be larger!
 */
int
buf_free(void)
{
  int free = read_pos - write_pos - m_outburst/*CHUNK_SIZE*/;
  if (free < 0) free += m_buffersize/*BUFFSIZE*/;
  return free;
}

// Two-channel sawtooth wave generator.
static int rt_read_samples( void *voutputBuffer, void *inputBuffer, unsigned int nBufferFrames,
         double streamTime, RtAudioStreamStatus status, void *userData )
{
  unsigned int i, j;
  char *outputBuffer = (char *)voutputBuffer;
  
  if ( status )
    trace_warning(("buffer underflow!\n"));

  int len = nBufferFrames * channels * 2;
  int first_len = m_buffersize/*BUFFSIZE*/ - read_pos;
  int buffered = buf_used();
  if (len > buffered) len = buffered;
  if (first_len > len) first_len = len;
  if (!buffer) return 0; /* till end of buffer */

  ws_memcpy (outputBuffer, &buffer[read_pos], first_len);

  if (len > first_len) /* we have to wrap around */
    {
      ws_memcpy (&outputBuffer[first_len], buffer, len - first_len); /* remaining part from beginning of buffer */
    }
  read_pos = (read_pos + len) % m_buffersize/*BUFFSIZE*/;
  return 0;
}

extern "C" int
ADIF_CB(adif_rt_config)(const adif_format_t *format, int opaque)
{ WS_UNUSED(opaque);

  if( audio->isStreamOpen() ) audio->closeStream();

  RtAudio::StreamParameters parameters;
  parameters.deviceId = audio->getDefaultOutputDevice();
  parameters.nChannels = format->channels;
  parameters.firstChannel = 0;
  unsigned int sampleRate = format->sample_rate;
  unsigned int bufferFrames = format->sample_rate / 4 / 8 * format->bit_depth; // 256 sample frames

  try
    {
      channels = channels;
      audio->openStream( &parameters, NULL, RTAUDIO_SINT16,
                      sampleRate, &bufferFrames, &rt_read_samples, NULL );
      audio->startStream();
      
      trace_debug(("config: sample_rate=%d\n", sampleRate));
    }
  catch ( RtAudioError& e )
    {
      e.printMessage();
      return -WERR_OPEN_DEVICE;
    }
  return audio->isStreamOpen();
}

extern "C" unsigned
ADIF_CB(adif_rt_write)(const void *buff, unsigned size, int opaque)
{ WS_UNUSED(opaque);

  const char *data = (const char *)buff;
  int len = (int)size;
  int first_len = m_buffersize/*BUFFSIZE*/ - write_pos;
  int free = buf_free();
  if (len > free) len = free;
  if (first_len > len) first_len = len;
  if (!buffer) return 0; /* till end of buffer */
  
  ws_memcpy (&buffer[write_pos], data, first_len);
  if (len > first_len) /* we have to wrap around */
    {
      ws_memcpy (buffer, &data[first_len], len - first_len); /* remaining part from beginning of buffer */
    }
  write_pos = (write_pos + len) % m_buffersize/*BUFFSIZE*/;
  return len;
}

adif_t adif_rt = {
  ADIF_RT,
  &adif_rt_init,
  &adif_rt_uninit,
  &adif_rt_config,
  &adif_rt_write
};
