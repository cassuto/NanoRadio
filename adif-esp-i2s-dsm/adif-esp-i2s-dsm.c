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

#include "portable.h"

#if USING(ADIF_ESP_I2S_DSM)
 
#define TRACE_UNIT "dsm"
#include "af-interface.h"
#include "util-logtrace.h"

#include "i2s_freertos.h"

int
ADIF_P(adif_esp_i2s_dsm_init)(int opaque)
{ WS_UNUSED(opaque);
  i2sInit();
  return 0;
}

void
ADIF_P(adif_esp_i2s_dsm_uninit)(int opaque)
{ WS_UNUSED(opaque);
}

int
ADIF_P(adif_esp_i2s_dsm_config)(const adif_format_t *format, int opaque)
{ WS_UNUSED(opaque);
  i2sSetRate(format->sample_rate, 0);
  return 0;
}

/* 2nd order delta-sigma DAC */
static inline int
samp_to_delta_sigma(short s)
{
	int x;
	int val=0;
	int w;
	static int i1v=0, i2v=0;
	static int reg=0;
	for (x=0; x<32; x++)
    {
      val<<=1;
      w=s;
      if (reg>0) w-=32767; else w+=32767; //Difference 1
      w+=i1v; i1v=w; /* Integrator 1 */
      if (reg>0) w-=32767; else w+=32767; //Difference 2
      w+=i2v; i2v=w; /* Integrator 2 */
      reg=w;
      if (w>0) val|=1; /* comparator */
    }
	return val;
}

/** note: pointer 'buff' MUST be aligned by 4 bytes boundary. */
unsigned
ADIF_P(adif_esp_i2s_dsm_write)(const void *buff, unsigned size, int opaque)
{
  short *short_sample_buff = (short *)buff;
	unsigned i;
	int samp;
	for (i=0; i<size; i++)
    {
      samp = samp_to_delta_sigma(short_sample_buff[i]);
      i2sPushSample(samp);
    }
  return size;
  WS_UNUSED(opaque);
}

adif_t adif_esp_i2s_dsm = {
  ADIF_ESP_I2S_DSM,
  &adif_esp_i2s_dsm_init,
  &adif_esp_i2s_dsm_uninit,
  &adif_esp_i2s_dsm_config,
  &adif_esp_i2s_dsm_write
};

#endif /* USING(ADIF_ESP_I2S_DSM) */
