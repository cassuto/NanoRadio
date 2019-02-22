#ifndef AUDIO_IF_H_
#define AUDIO_IF_H_

#include "portable.h"

#define ADIF_CB(x) x

typedef struct {
  int sample_rate;
  short bit_depth;
  short channels;
} adif_format_t;

typedef struct {
  int adif_index;
  int ADIF_CB((*init))(int opaque);
  void ADIF_CB((*uninit))(int opaque);
  int ADIF_CB((*config))(const adif_format_t *format, int opaque);
  unsigned ADIF_CB((*write))(const void *buff, unsigned size, int opaque);
} adif_t;

#define ADIF_RT 1   /* RtAudio */
#define ADIF_I2S 2  /* I2S output Device */

extern const adif_t *NC_P(adif_init)(int adif_index);
extern void NC_P(adif_uninit)(const adif_t *adif);
extern int NC_P(adif_config)(const adif_t *adif, const adif_format_t *format);
extern unsigned NC_P(adif_write)(const adif_t *adif, const void *buff, unsigned size);

extern const adif_t *adif_instance;

#endif
