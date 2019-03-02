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
#include "util-task.h"

#if PORT(POSIX)

#include <pthread.h>
#include <semaphore.h>

typedef struct
{
  pfn_util_task_callback callback;
  util_coreid_t coreid;
  void *opaque;
} thread_param_t;

static void*
thread_entry(void *opaque)
{
  thread_param_t *param = (thread_param_t *)opaque;
  param->callback(param->opaque);
  ws_free(param);
  return NULL;
}

#elif PORT(FREE_RTOS)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#endif

int
NC_P(util_create_task)(pfn_util_task_callback callback, const char *name, int stack_size, util_coreid_t coreid, void *opaque)
{
#if PORT(POSIX)
  pthread_t tid;
  thread_param_t *param = (thread_param_t *)ws_malloc(sizeof(*param));
  if(!param)
    return -WERR_NO_MEMORY;
  
  param->callback = callback;
  param->coreid = coreid;
  param->opaque = opaque;
  
  return pthread_create(&tid, NULL, thread_entry, param);
#elif PORT(FREE_RTOS)
  int priority = 1;
  int rc;
  switch(coreid)
    {
      case CORE_PROTO_STACK:
        priority = 11; break;
      case CORE_STREAM:
        priority = 11; break;
      case CORE_CODEC:
        priority = 2; break;
      case CORE_CTRL:
        priority = 1; break;
    }
  if( xTaskCreate(callback, name, stack_size, opaque, priority, NULL) != pdPASS )
    return -WERR_FAILED;
  WS_UNUSED(coreid);
#else
#error port me!
#endif
  return 0;
}

void
NC_P(util_task_exit)(void)
{
}

util_semaphore_t
util_create_semaphore(int value)
{
#if PORT(POSIX)
  sem_t sem;
  if( sem_init(&sem, 0, value) )
    return 0;
  return (util_semaphore_t)sem;
#elif PORT(FREE_RTOS)
  xSemaphoreHandle sem;
  vSemaphoreCreateBinary(sem);
  return (util_semaphore_t)sem;
#endif
  return 0;
}

void
util_destroy_semaphore(util_semaphore_t sem)
{
#if PORT(POSIX)
  sem_destroy((sem_t*)(&sem));
#elif PORT(FREE_RTOS)
  xSemaphoreHandle xSem = (xSemaphoreHandle)sem;
  vSemaphoreDelete(xSem);
#endif
}

int
util_semaphore_take(util_semaphore_t sem)
{
#if PORT(POSIX)
  return sem_wait((sem_t*)(&sem));
#elif PORT(FREE_RTOS)
  xSemaphoreHandle xSem = (xSemaphoreHandle)sem;
  return xSemaphoreTake(xSem, portMAX_DELAY);
#endif
}

int
util_semaphore_give(util_semaphore_t sem)
{
#if PORT(POSIX)
  return sem_post((sem_t*)(&sem));
#elif PORT(FREE_RTOS)
  xSemaphoreHandle xSem = (xSemaphoreHandle)sem;
  return xSemaphoreGive(xSem);
#endif
}

util_mutex_t
util_create_mutex(void)
{
#if PORT(POSIX)
  pthread_mutex_t mutex;
  if( pthread_mutex_init(&mutex, NULL) )
    return 0;
  return (util_mutex_t)mutex;
#elif PORT(FREE_RTOS)
  xSemaphoreHandle mutex = xSemaphoreCreateMutex();
  return (util_mutex_t)mutex;
#endif
  return 0;
}

void
util_mutex_take(util_mutex_t mutex)
{
#if PORT(POSIX)
  pthread_mutex_lock((pthread_mutex_t *)(&mutex));
#elif PORT(FREE_RTOS)
  xSemaphoreHandle xSem = (xSemaphoreHandle)mutex;
  xSemaphoreTake(xSem, portMAX_DELAY);
#endif
}

void
util_mutex_give(util_mutex_t mutex)
{
#if PORT(POSIX)
  pthread_mutex_unlock((pthread_mutex_t *)(&mutex));
#elif PORT(FREE_RTOS)
  xSemaphoreHandle xSem = (xSemaphoreHandle)mutex;
  xSemaphoreGive(xSem);
#endif
}

void
util_mutex_destroy(util_mutex_t mutex)
{
#if PORT(POSIX)
  pthread_mutex_destroy((pthread_mutex_t *)(&mutex));
#elif PORT(FREE_RTOS)
  xSemaphoreHandle xSem = (xSemaphoreHandle)mutex;
  vSemaphoreDelete(xSem);
#endif
}

void
NC_P(util_task_yield)(void)
{
#if PORT(FREE_RTOS)
  vPortYield();
#endif
}

void
NC_P(util_task_sleep)(int ms)
{
#if PORT(FREE_RTOS)
  vTaskDelay(ms / portTICK_RATE_MS);
#endif
}

int
NC_P(util_task_get_free_heap)(void)
{
  return xPortGetFreeHeapSize();
}
