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

#define TRACE_UNIT "cdmpg"

#include "util-logtrace.h"
#include "af-interface.h"
#include "af-codec.h"
#include "af-buffer.h"

#include "mad.h"
#include "stream.h"
#include "frame.h"
#include "synth.h"

// The theoretical maximum frame size is 2881 bytes,
// MPEG 2.5 Layer II, 8000 Hz @ 160 kbps, with a padding slot plus 8 byte MAD_BUFFER_GUARD.
#define MAX_FRAME_SIZE (2889)

// The theoretical minimum frame size of 24 plus 8 byte MAD_BUFFER_GUARD.
#define MIN_FRAME_SIZE (32)

static char mpg_buf[8192];
static size_t mpg_buf_size;

static adif_format_t buffer_fmt =
  {
    .sample_rate = 0,
    .bit_depth = 16,
    .channels = 2
  };

/* Routine to print out an error */
static enum mad_flow
error(void *data, struct mad_stream *stream, struct mad_frame *frame)
{
  trace_error(("dec err 0x%04x (%s)\n", stream->error, mad_stream_errorstr(stream)));
  return MAD_FLOW_CONTINUE;
}

static enum mad_flow
decode_input(struct mad_stream *stream){
  size_t remaining = 0;
  
  if (stream->next_frame != 0)
    {
      remaining  = stream->bufend - stream->next_frame;
      
      if (remaining != 0)
        {
          memmove(mpg_buf, stream->next_frame, remaining);
        }
    }
  
  size_t len = sizeof mpg_buf - remaining;
  audio_buffer_read(mpg_buf + remaining, len);
  if(!audio_running)
    {
      return MAD_FLOW_STOP;
    }

  mpg_buf_size = len + remaining;
  
  mad_stream_buffer(stream, (const unsigned char *)mpg_buf, mpg_buf_size);
  return MAD_FLOW_CONTINUE;
}

void
task_mpeg_decode(void *opaque)
{
    int ret;
    struct mad_stream *stream;
    struct mad_frame *frame;
    struct mad_synth *synth;
    
    stream = ws_malloc(sizeof(struct mad_stream));
    frame = ws_malloc(sizeof(struct mad_frame));
    synth = ws_malloc(sizeof(struct mad_synth));

    if( !stream ) { trace_error(("malloc(stream) failed\n")); goto err_out; }
    if( !synth ) { trace_error(("malloc(synth) failed\n")); goto err_out; }
    if( !frame ) { trace_error(("malloc(frame) failed\n")); goto err_out; }

    mad_stream_init(stream);
    mad_frame_init(frame);
    mad_synth_init(synth);
    
    while(audio_running)
      {
        while( audio_status == AUDIO_STALLED )
          util_task_yield();
        
        if( decode_input(stream) == MAD_FLOW_STOP ) /* fill in the bufer */
          break;

        for(;;)
          {
            ret = mad_frame_decode(frame, stream); /* returns 0 or -1 */
            if( ret == -1 )
              {
                if (!MAD_RECOVERABLE(stream->error))
                  {
                    /* We're most likely out of buffer and need to call input() again */
                    break;
                  }
                error(NULL, stream, frame);
                continue;
              }
            mad_synth_frame(synth, frame);
          }
      }

    free(synth);
    free(frame);
    free(stream);
    
    WS_UNUSED(opaque);
    
err_out:
    util_task_exit();
}

/* Called by the NXP modifications of libmad. Sets the needed output sample rate. */
void
set_dac_sample_rate(int rate)
{
  buffer_fmt.sample_rate = rate;
}

static void
render_send_sample(uint16_t smpl)
{
  int len = sizeof smpl;
  while(len)
  {
    len -= adif_write(adif_instance, &smpl, sizeof smpl);
  }
}

/* render callback for the libmad synth */
void
render_sample_block(short *sample_buff_ch0, short *sample_buff_ch1, int num_samples, unsigned int num_channels)
{
  buffer_fmt.channels = num_channels;
  uint32_t len = num_samples /** sizeof(short) * num_channels*/;
  
  adif_config(adif_instance, &buffer_fmt);

  while(len--)
    {
      int16_t dat = (int16_t)(*sample_buff_ch0++);
      render_send_sample(dat);
      
      dat = (int16_t)(*sample_buff_ch1++);
      render_send_sample(dat);
    }
  return;
}
