/* k_server.c - microkernel server */

/*
 * Copyright (c) 2010, 2012-2015 Wind River Systems, Inc.
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

/*
DESCRIPTION
This module implements the microkernel server, which processes service requests
from tasks (and, less commonly, fibers and ISRs). The requests are service by
a high priority fiber, thereby ensuring that requests are processed in a timely
manner and in a single threaded manner that prevents simultaneous requests from
interfering with each other.
 */

#include <toolchain.h>
#include <sections.h>
#include <micro_private.h>
#include <nano_private.h>
#include <microkernel.h>
#include <nanokernel.h>
#include <misc/__assert.h>
#include <drivers/system_timer.h>

extern const kernelfunc _k_server_dispatch_table[];

/**
 *
 * @brief Select task to be executed by microkernel
 *
 * Locates that highest priority task queue that is non-empty and chooses the
 * task at the head of that queue. It's guaranteed that there will always be
 * a non-empty queue, since the idle task is always executable.
 *
 * @return pointer to selected task
 */

static struct k_task *next_task_select(void)
{
	int K_PrioListIdx;

#if (CONFIG_NUM_TASK_PRIORITIES <= 32)
	K_PrioListIdx = find_lsb_set(_k_task_priority_bitmap[0]) - 1;
#else
	int bit_map;
	int set_bit_pos;

	K_PrioListIdx = -1;
	for (bit_map = 0; ; bit_map++) {
		set_bit_pos = find_lsb_set(_k_task_priority_bitmap[bit_map]);
		if (set_bit_pos) {
			K_PrioListIdx += set_bit_pos;
			break;
		}
		K_PrioListIdx += 32;
	}
#endif

	return _k_task_priority_list[K_PrioListIdx].head;
}

/**
 *
 * @brief The microkernel thread entry point
 *
 * This function implements the microkernel fiber.  It waits for command
 * packets to arrive on its command stack. It executes all commands on the
 * stack and then sets up the next task that is ready to run. Next it
 * goes to wait on further inputs on the command stack.
 *
 * @return Does not return.
 */

FUNC_NORETURN void _k_server(int unused1, int unused2)
{
	struct k_args *pArgs;
	struct k_task *pNextTask;

	ARG_UNUSED(unused1);
	ARG_UNUSED(unused2);

	/* indicate that failure of this fiber may be fatal to the entire system
	 */

	_nanokernel.current->flags |= ESSENTIAL;

	while (1) { /* forever */
		pArgs = (struct k_args *)nano_fiber_stack_pop_wait(
			&_k_command_stack); /* will schedule */
		do {
			kevent_t event;
			/* if event < _k_num_events, it's a well-known event */
			event = (kevent_t)(pArgs);
			if (event < (kevent_t)_k_num_events) {
#ifdef CONFIG_TASK_MONITOR
				if (_k_monitor_mask & MON_EVENT) {
					_k_task_monitor_args(pArgs);
				}
#endif
				_k_do_event_signal(event);
			} else {
#ifdef CONFIG_TASK_MONITOR
				if (_k_monitor_mask & MON_KSERV) {
					_k_task_monitor_args(pArgs);
				}
#endif
				(*pArgs->Comm)(pArgs);
			}

			/* check if another fiber (of equal or greater priority)
			 * needs to run */

			if (_nanokernel.fiber) {
				fiber_yield();
			}
		} while (nano_fiber_stack_pop(&_k_command_stack, (void *)&pArgs));

		pNextTask = next_task_select();

		if (_k_current_task != pNextTask) {

			/* switch from currently selected task to a different one */

#ifdef CONFIG_WORKLOAD_MONITOR
			if (pNextTask->id == 0x00000000) {
				_k_workload_monitor_idle_start();
			} else if (_k_current_task->id == 0x00000000) {
				_k_workload_monitor_idle_end();
			}
#endif

			_k_current_task = pNextTask;
			_nanokernel.task = (struct tcs *)pNextTask->workspace;

#ifdef CONFIG_TASK_MONITOR
			if (_k_monitor_mask & MON_TSWAP) {
				_k_task_monitor(_k_current_task, 0);
			}
#endif
		}
	}

	/*
	 * Code analyzers may complain that _k_server() uses an infinite loop
	 * unless we indicate that this is intentional
	 */

	CODE_UNREACHABLE;
}
