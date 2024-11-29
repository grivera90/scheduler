/**
*******************************************************************************
* @file           : scheduler.c
* @brief          : Simple scheduler time-triggered for embedded systems implementation
* @author         : Gonzalo Rivera.
* @date           : 18/01/2023
*******************************************************************************
* @attention
*
* Copyright (c) <date> grivera. All rights reserved.
*
*/
/******************************************************************************
    Includes
******************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "bsp.h"
#include "scheduler.h"
#define LOG_LOCAL_LEVEL LOG_WARN
#define LOG_CONFIG_TIMESTAMP LOG_TIMESTAMP
#include "log.h"
/******************************************************************************
    Defines and constants
******************************************************************************/
#define BIT_SET(REG, BIT) ((REG)|= (1 << BIT))
#define BIT_CLEAR(REG, BIT) ((REG)&= ~(1 << BIT))
#define BIT_GET(REG, BIT) ((REG >> BIT) & 1)

static const char *MODULE_NAME = "[OP. SYSTEM]";
/******************************************************************************
    Data types
******************************************************************************/
typedef enum
{
	SCH_STATUS_OK = 0,
	SCH_TASK_CREATE_OK,
	SCH_TASK_CREATE_FAIL,
	SCH_TASK_DELETE_OK,
	SCH_TASK_DELETE_FAIL,
	SCH_TO_MUCH_TASK,
	SCH_RUN,
	SCH_ERROR = -1

}sch_sts_t;

/**
 * @brief
 * Convention of status values:
 * OK/STATUS = 0, ERROR/FAIL/STOP = 1
 *
 */
typedef enum
{
	SCH_STATUS_FLAG = 0,
	SCH_TASK_CREATE_FLAG,
	SCH_TASK_DELETE_FLAG,
	SCH_OVERFLOW_TASK_FLAG,
	SCH_RUN_FLAG

}sch_flags_t;
/******************************************************************************
    Local variables
******************************************************************************/
static uint32_t tasks_counter = 0;
static uint8_t scheduler_status = 0;
static task_t task_array[MAX_TASKS] = {NULL};
/******************************************************************************
    Local function prototypes
******************************************************************************/
static void scheduler_report_status(void);

/**
 * @brief
 * Function to update the status of tasks each tick.
 * This function will be called from Systick isr.
 *
 * @param void *param for something ;)
 *
 * @return 0 = OK, -1 = fail, 1 = other
 */
static int scheduler_update(void);

/******************************************************************************
    Local function definitions
******************************************************************************/
static void scheduler_report_status(void)
{
	uint32_t index = 0;

	LOGW(MODULE_NAME, "***********************************");
	LOGW(MODULE_NAME, "**     SCHEDULER REPORT STATUS   **");
	LOGW(MODULE_NAME, "***********************************");
	LOGW(MODULE_NAME, " v1.0.0");
	LOGW(MODULE_NAME, "***********************************");
	LOGW(MODULE_NAME, "Task creates:\t%d", (int)tasks_counter);
	LOGW(MODULE_NAME, "***********************************");
	LOGW(MODULE_NAME, "Scheduler status flags:");
	LOGW(MODULE_NAME, "SCH_STATUS:\t\t%d", BIT_GET(scheduler_status, SCH_STATUS_OK));
	LOGW(MODULE_NAME, "SCH_TASK_CREATE:\t\t%d", BIT_GET(scheduler_status, SCH_TASK_CREATE_OK));
	LOGW(MODULE_NAME, "SCH_TASK_DELETE_OK:\t%d", BIT_GET(scheduler_status, SCH_TASK_DELETE_OK));
	LOGW(MODULE_NAME, "SCH_OVERFLOW_TASK_FLAG:\t%d", BIT_GET(scheduler_status, SCH_TO_MUCH_TASK));
	LOGW(MODULE_NAME, "SCH_RUN_FLAG:\t\t%d", BIT_GET(scheduler_status, SCH_RUN));

    for(index = 0; index < tasks_counter; index++)
    {
    	LOGW(MODULE_NAME, "***********************************");
    	LOGW(MODULE_NAME, "Task name:\t%s", task_array[index].task_name);
    	LOGW(MODULE_NAME, "***********************************");
    	LOGW(MODULE_NAME, "Task status:\t%d", task_array[index].status);
    	LOGW(MODULE_NAME, "Task period:\t%d", (int)task_array[index].period);
    	LOGW(MODULE_NAME, "Task index:\t%d", (int)task_array[index].task_handler.index);
    }

    LOGW(MODULE_NAME, "***********************************");
}

static int scheduler_update(void)
{
	int ret = SCH_STATUS_OK;
	int index = 0;

	for(index = 0; index < MAX_TASKS; index++)
	{
		if(NULL != task_array[index].ptask)
		{
			if(0 == task_array[index].delay)
			{
				if(task_array[index].status != RUN_ALWAYS && task_array[index].status != SUSPENDED)
				{
					task_array[index].status = READY;
				}

				if(task_array[index].period > 0)
				{
					task_array[index].delay = task_array[index].period;
				}
			}
			else
			{
				task_array[index].delay -= 1;
			}
		}
	}

	return ret;
}

/******************************************************************************
    Public function definitions
******************************************************************************/
int scheduler_init(void)
{
	int ret = SCH_STATUS_OK;
	uint32_t task_index = 0;

	tasks_counter = 0;
	scheduler_status = 0;

	for(task_index = 0; task_index < MAX_TASKS; task_index++)
	{
		if(SCH_STATUS_OK != scheduler_delete_task(task_index))
		{
			BIT_SET(scheduler_status, SCH_STATUS_FLAG);
			ret = SCH_ERROR;
			break;
		}
	}

	if(ret == SCH_STATUS_OK)
	{
		BIT_CLEAR(scheduler_status, SCH_STATUS_FLAG);
		BIT_CLEAR(scheduler_status, SCH_RUN_FLAG);
	}
	else
	{
		BIT_SET(scheduler_status, SCH_STATUS_FLAG);
		BIT_SET(scheduler_status, SCH_RUN_FLAG);
	}

	bsp_set_cb_systick(scheduler_update);

	return ret;
}

int scheduler_start(void)
{
	int ret = SCH_STATUS_OK;

	/* chequear los demas flag que faltan. Optimizarlo para hacer un and con una mascara o algo asi */
	if((SCH_STATUS_OK == BIT_GET(scheduler_status, SCH_STATUS_FLAG)) &&
			(SCH_STATUS_OK == BIT_GET(scheduler_status, SCH_OVERFLOW_TASK_FLAG)) &&
			(SCH_STATUS_OK == BIT_GET(scheduler_status, SCH_TASK_CREATE_FLAG)) &&
			(SCH_STATUS_OK == BIT_GET(scheduler_status, SCH_TASK_DELETE_FLAG)) &&
			(SCH_STATUS_OK == BIT_GET(scheduler_status, SCH_RUN_FLAG)) &&
			(tasks_counter > 0) &&
			(tasks_counter <= MAX_TASKS))
	{
		BIT_CLEAR(scheduler_status, SCH_RUN_FLAG);
	}
	else
	{
		ret = SCH_ERROR;
	}

	scheduler_report_status();

	if(SCH_STATUS_OK != ret)
	{
		LOGE(MODULE_NAME, "scheduler start error.");
		while(1);
	}

	return ret;
}
int scheduler_add_task(callback_task_t task, const char *task_name, void *task_param, const uint32_t delay, const uint32_t period)
{
	int ret = SCH_STATUS_OK;
	uint32_t task_index = 0;

	if(tasks_counter < MAX_TASKS)
	{
		// chequea si no esta agregada la misma tarea.
		for(task_index = 0; task_index < MAX_TASKS; task_index++)
		{
			if(task_array[task_index].ptask == task)
			{
				BIT_SET(scheduler_status, SCH_TASK_CREATE_FLAG);
				ret = SCH_ERROR;
				break;
			}
		}

		if(SCH_STATUS_OK == ret)
		{
			task_array[tasks_counter].ptask = task;
			task_array[tasks_counter].task_name = task_name;
			task_array[tasks_counter].delay = delay;
			task_array[tasks_counter].period = period;

			if(0 == period)
			{
				task_array[tasks_counter].status = RUN_ALWAYS;
			}
			else
			{
				task_array[tasks_counter].status = STOPPED;
			}

			task_array[tasks_counter].task_handler.index = tasks_counter;
			task_array[tasks_counter].task_handler.parameter = task_param;
			tasks_counter++;
			BIT_CLEAR(scheduler_status, SCH_TASK_CREATE_FLAG);
		}

	}
	else
	{
		BIT_SET(scheduler_status, SCH_TASK_CREATE_FLAG);
		BIT_SET(scheduler_status, SCH_STATUS_FLAG);
		ret = SCH_ERROR;
	}

	return ret;
}

int scheduler_delete_task(uint32_t index)
{
	int ret = SCH_STATUS_OK;

	if(NULL != task_array[index].ptask)
	{
		task_array[index].ptask = NULL;
		task_array[index].task_handler.parameter = NULL;
		task_array[index].task_handler.index = 0;
		task_array[index].delay = 0;
		task_array[index].status = STOPPED;
		tasks_counter--;
	}

	return ret;
}

void scheduler_dispatch_task(void)
{
	uint32_t index = 0;

	for(index = 0; index < tasks_counter; index++)
	{
		if(task_array[index].status == RUN_ALWAYS)
		{
			(*task_array[index].ptask)(&task_array[index].task_handler);	// call task function.
		}

		if(task_array[index].status == READY)
		{
			task_array[index].status = RUNNING;

			(*task_array[index].ptask)(&task_array[index].task_handler);	// call task function.

			if(RUNNING == task_array[index].status)
			{
				task_array[index].status = STOPPED;
			}

			if(0 == task_array[index].period)
			{
				scheduler_delete_task(index);
			}
		}
	}
}
