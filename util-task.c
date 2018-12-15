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
util_create_task(pfn_util_task_callback callback, int stack_size, util_coreid_t coreid, void *opaque)
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
  WS_UNUSED(stack_size);
#endif
  return 0;
}

void
util_task_exit(void)
{
}

util_semaphore_t
util_create_semaphore(int value)
{
  sem_t sem;
  if( sem_init(&sem, 0, value) )
    return 0;
  return (util_semaphore_t)sem;
}

void
util_destroy_semaphore(util_semaphore_t sem)
{ sem_destroy((sem_t*)(&sem)); }

int
util_semaphore_take(util_semaphore_t sem)
{ return sem_wait((sem_t*)(&sem)); }

int
util_semaphore_give(util_semaphore_t sem)
{ return sem_post((sem_t*)(&sem)); }

util_mutex_t
util_create_mutex(void)
{
  pthread_mutex_t mutex;
  if( pthread_mutex_init(&mutex, NULL) )
    return 0;
  return (util_mutex_t)mutex;
}

void
util_mutex_take(util_mutex_t mutex)
{ pthread_mutex_lock((pthread_mutex_t *)(&mutex)); }

void
util_mutex_give(util_mutex_t mutex)
{ pthread_mutex_unlock((pthread_mutex_t *)(&mutex)); }

void
util_mutex_destroy(util_mutex_t mutex)
{ pthread_mutex_destroy((pthread_mutex_t *)(&mutex)); }

void
util_task_yield(void)
{
}
