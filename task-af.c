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

#include "af-buffer.h"
#include "af-interface.h"
#include "af-codec.h"

#include "http-protocol.h"
#include "tcp-socket.h"
#include "util-task.h"
#include "tasks.h"

#define TRACE_UNIT "task-af"
#include "util-logtrace.h"

const adif_t *adif_instance;
volatile audio_status_t audio_status;
volatile char audio_running;
static http_t stream_http;
static volatile int stream_socket;
static volatile int audio_error; /* non-atomic, not for accurate controling */
static volatile stream_type_t stream_type;

static int
create_audio_mainloop_task(void)
{
  pfn_util_task_callback task_func;
  uint16_t stack_depth;

  switch(stream_type)
    {
      case STREAM_MPEG:
        task_func = task_mpeg_decode;
        stack_depth = 8448;
        break;
#if 0
      case STREAM_MP4:
        task_func = libfaac_decoder_task;
        stack_depth = 55000;
        break;

      case STREAM_AAC:
      case STREAM_OCTET: // probably .aac
        task_func = fdkaac_decoder_task;
        stack_depth = 6144;
        break;
#endif
      default:
        return -WERR_FAILED;
    }

  audio_running = 1;
  if( util_create_task(task_func, stack_depth, CORE_CODEC, NULL) )
    {
      trace_error(("creating decoder task\n"));
      return -WERR_FAILED;
    }
  audio_status = AUDIO_DECODING;

  trace_debug(("created decoder task: %d\n", stream_type));
  return 0;
}

static int
event_redirect(char *url)
{
  return 0; /* todo */
}

static int
event_content_type(char *at, int length)
{
  if (strstr(at, "application/octet-stream"))
    stream_type = STREAM_OCTET;
  else if (strstr(at, "audio/aac"))
    stream_type = STREAM_AAC;
  else if (strstr(at, "audio/mp4"))
    stream_type = STREAM_MP4;
  else if (strstr(at, "audio/x-m4a"))
    stream_type = STREAM_MP4;
  else if (strstr(at, "audio/mpeg"))
    stream_type = STREAM_MPEG;
  else
    return -WERR_UNKNOW_TYPE;
  return 0;
}

static int
event_body(char *at, int length)
{
  audio_buffer_write( at, length );
  if( audio_status == AUDIO_IDLE )
    {
      return create_audio_mainloop_task(); /* run mainloop after buffer pre-filled */
    }
  return 0;
}

static void
task_audio_http_connect(void *opaque)
{
  http_event_procs_t procs =
    {
      .event_body = event_body,
      .event_redirect = event_redirect,
      .event_content_type = event_content_type
    };
  audio_error = http_read_response(&stream_http, stream_socket, &procs);

  util_task_exit();
  WS_UNUSED(opaque);
}

int
task_audio_open(const char *host, const char *file, int port)
{
  if( audio_status == AUDIO_IDLE )
    {
      stream_socket = http_request(&stream_http, host, file, port, -1, -1);
      return util_create_task(task_audio_http_connect, 2048, CORE_CODEC, NULL);
    }
  return -WERR_BUSY;
}

int
task_audio_init(void)
{
  if( (adif_instance = adif_init(ADIF_RT)) )
    {
      int rc = audio_buffer_init();
      
      if(!rc)
        {
          audio_status = AUDIO_IDLE;
          audio_error = 0;
          audio_running = 0;
          return rc;
        }
    }
  return -WERR_FAILED;
}
