/* microkernel/ticks.h - microkernel tick header file */

/*
 * Copyright (c) 1997-2015 Wind River Systems, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TICKS_H
#define TICKS_H

/**
 * @brief Microkernel Timers
 * @defgroup microkernel_timer Microkernel Timers
 * @ingroup microkernel_services
 * @{
 */

#include <microkernel.h>
#include <sys_clock.h>

/* externs */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set time slicing period and scope
 *
 * This routine controls how task time slicing is performed by the task
 * scheduler, by specifying the maximum time slice length (in ticks) and
 * the highest priority task level for which time slicing is performed.
 *
 * To enable time slicing, a non-zero time slice length must be specified.
 * The task scheduler then ensures that no executing task runs for more than
 * the specified number of ticks before giving other tasks of that priority
 * a chance to execute. (However, any task whose priority is higher than the
 * specified task priority level is exempted, and may execute as long as
 * desired without being pre-empted due to time slicing.)
 *
 * Time slicing only limits that maximum amount of time a task may continuously
 * execute. Once the scheduler selects a task for execution, there is no minimum
 * guaranteed time the task will execute before tasks of greater or equal
 * priority are scheduled.
 *
 * If the currently executing task is the only one of that priority eligible
 * for execution this routine has no effect, as that task will be immediately
 * rescheduled once the slice period expires.
 *
 * To disable timeslicing, call the API with both parameters set to zero.
 *
 * @return N/A
 */
extern void sys_scheduler_time_slice_set(int32_t t, kpriority_t p);

/**
 * @brief Allocate a timer and return its object identifier
 *
 * @return timer identifier
 */
extern ktimer_t task_timer_alloc(void);

/**
 * @brief Deallocate a timer
 *
 * This routine frees the resources associated with the timer.  If a timer was
 * started, it has to be stopped using task_timer_stop() before it can be freed.
 *
 * @param timer   Timer to deallocate.
 *
 * @return N/A
 */
extern void task_timer_free(ktimer_t timer);


/**
 *
 * @brief Start or restart the specified low resolution timer
 *
 * This routine starts or restarts the specified low resolution timer.
 *
 * When the specified number of ticks, set by @a duration, expires, the semaphore
 * is signalled.  The timer repeats the expiration/signal cycle each time
 * @a period ticks has elapsed.
 *
 * Setting @a period to 0 stops the timer at the end of the initial delay.

 * If either @a duration or @a period is passed a invalid value (@a duration <= 0,
 * @a period < 0), then this kernel API acts like a task_timer_stop(): if the
 * allocated timer was still running (from a previous call), it will be
 * cancelled; if not, nothing will happen.
 *
 * @param timer      Timer to start.
 * @param duration   Initial delay in ticks.
 * @param period     Repetition interval in ticks.
 * @param sema       Semaphore to signal.
 *
 * @return N/A
 */
extern void task_timer_start(ktimer_t timer,
			     int32_t duration,
			     int32_t period,
			     ksem_t sema);
/**
 *
 * @brief Restart a timer
 *
 * This routine restarts the timer specified by @a timer. The timer must have
 * already been started by a call to task_timer_start().
 *
 * @param timer      Timer to restart.
 * @param duration   Initial delay.
 * @param period     Repetition interval.
 *
 * @return N/A
 */
static inline void task_timer_restart(ktimer_t timer, int32_t duration,
					int32_t period)
{
	task_timer_start(timer, duration, period, _USE_CURRENT_SEM);
}

/**
 * @brief Stop a timer
 *
 * This routine stops the specified timer. If the timer period has already
 * elapsed, the call has no effect.
 *
 * @param timer   Timer to stop.
 *
 * @return N/A
 */
extern void task_timer_stop(ktimer_t timer);

/**
 *
 * @brief Sleep for a number of ticks
 *
 * This routine suspends the calling task for the specified number of timer
 * ticks.  When the task is awakened, it is rescheduled according to its
 * priority.
 *
 * @param ticks   Number of ticks for which to sleep.
 *
 * @return N/A
 */
extern void task_sleep(int32_t ticks);

/**
 *
 * @brief Read the processor workload
 *
 * This routine returns the workload as a number ranging from 0 to 1000.
 *
 * Each unit equals 0.1% of the time the idle task was not scheduled by the
 * microkernel during the period set by sys_workload_time_slice_set().
 *
 * IMPORTANT: This workload monitor ignores any time spent servicing ISRs and
 * fibers! Thus, a system which has no meaningful task work to do may spend
 * up to 100% of its time servicing ISRs and fibers, yet report a workload of 0%
 * because the idle task is always the task selected by the microkernel.
 *
 * @return workload
 */
extern int task_workload_get(void);

/**
 *
 * @brief Set workload period
 *
 * This routine specifies the workload measuring period for task_workload_get().
 * @param t Time slice
 * @return N/A
 */
extern void sys_workload_time_slice_set(int32_t t);

#ifdef __cplusplus
}
#endif
/**
 * @}
 */
#endif /* TICKS_H */
