#ifndef TASKS_H_
#define TASKS_H_

extern int NC_P(task_audio_init)(void);
extern int NC_P(task_audio_open)(const char *host, const char *file, int port);
extern int NC_P(task_controls_init)(void);

#endif
