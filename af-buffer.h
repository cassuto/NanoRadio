#ifndef AUDIO_BUFFER_H_
#define AUDIO_BUFFER_H_

extern int NC_P(audio_buffer_init)(void);
extern void NC_P(audio_buffer_reset)(void);
extern void NC_P(audio_buffer_read)(char *buff, int len);
extern void NC_P(audio_buffer_write)(const char *buff, int buffLen);
extern int NC_P(audio_buffer_fill)(void);
extern int NC_P(audio_buffer_free)(void);
extern int NC_P(audio_buffer_size)(void);
extern long NC_P(audio_buffer_get_overflow)(void);
extern long NC_P(audio_buffer_get_underflow)(void);

#endif
