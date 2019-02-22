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

#define TRACE_UNIT "task-ctrl"

#include "portable.h"
#include "util-task.h"
#include "tasks.h"
#include "util-logtrace.h"
#include "xapi.h"
#include "tasks.h"

enum controls_mode
{
  CTRL_MODE_INITAL = 0,
  CTRL_MODE_CATALOG_ROOT,
};

#if PORT(POSIX)
static enum controls_mode g_mode = CTRL_MODE_INITAL;
static int count_catalog_root_entries;
#endif

static void
controls_error_report(int rc)
{
#if PORT(POSIX)
  perror("An error has aborted with code. (%s)", rc);
#endif
}

static int
catalog_root_entry_proc(const char *value, int length, void *opaque)
{
  printf("\t%d - %.*s\n", count_catalog_root_entries++, length, value);
  return 0;
}

static void
qurey_catalog(void)
{
  int rc;

  if( (rc = radio_update_root_catalog()) )
    {
      controls_error_report(rc);
      return;
    }

#if PORT(POSIX)

  g_mode = CTRL_MODE_CATALOG_ROOT;
  count_catalog_root_entries = 0;

  printf("Select a entry from the following list:\n");

  if( (rc = radio_root_catalog(catalog_root_entry_proc, NULL)) )
    {
      controls_error_report(rc);
      return;
    }
#endif
}

static void
input_catalog_root(const char *answer)
{
}

static void
enter_catalog(const char *command)
{
}

static void
task_controls(void *opaque)
{
#if PORT(POSIX)
#define CMD_BUFF 1024
  char buff[CMD_BUFF];
  WS_UNUSED(opaque);

  while( fgets(stdin, buff) )
    {
      switch( g_mode )
        {
        case CTRL_MODE_INITAL:
          {
            if( strncmp(buff, "catalog", sizeof("catalog")-1) == 0 )
              qurey_catalog();
            else if( strncmp(buff, "catalog", sizeof("catalog")-1) == 0 )
              enter_catalog(buff);
          }
          break;

        case CTRL_MODE_CATALOG_ROOT:
          input_catalog_root(buff);
          break;
        }
    }
#endif
}

int
NC_P(task_controls_init)(void)
{
  return util_create_task(task_controls, "controls", 2048, CORE_PROTO_STACK, NULL);
}
