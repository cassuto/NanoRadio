#ifndef AUDIO_CODEC_H_
#define AUDIO_CODEC_H_

#include "portable.h"
#include "util-task.h"

typedef enum
{
  AUDIO_IDLE = 0,
  AUDIO_CACHING,
  AUDIO_DECODING,
  AUDIO_STALLED
} audio_status_t;

typedef enum
{
  STREAM_MPEG,
  STREAM_AAC,
  STREAM_MP4,
  STREAM_OCTET
} stream_type_t;

extern const adif_t *adif_instance;
extern volatile audio_status_t audio_status;
extern volatile char audio_running;
extern void task_mpeg_decode(void *); /* codec-mpeg.c */

#endif
