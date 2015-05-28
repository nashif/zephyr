/* task IRQ kernel services */

/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3) Neither the name of Wind River Systems nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
DESCRIPTION
This module manages the interrupt functionality between a task level device
driver and the kernel.

A task level device driver allocates a specific task IRQ object by providing
an IRQ and priority level; the registration process allocates an interrupt
vector, sets up an ISR, and enables the associated interrupt. When an
interrupt occurs, the ISR handler signals an event specific to the IRQ object.
The task level device driver tests/waits on this event number to determine
if/when the interrupt has occurred. As the ISR also disables the interrupt, the
task level device driver subsequently make a request to have the interrupt
enabled again. If desired, the device driver can free an IRQ object that
it is no longer interested in using.

These routines perform error checking to ensure that an IRQ object can only be
allocated by a single task, and that subsequent operations on that IRQ object
are only performed by that task. This checking is necessary to ensure that
a task cannot impact the operation of an IRQ object it does not own.
*/

#if (CONFIG_MAX_NUM_TASK_IRQS > 0)

#include <nanok.h>
#include <microkernel.h>
#include <minik.h>
#include <misc/__assert.h>

#define MAX_TASK_IRQS CONFIG_MAX_NUM_TASK_IRQS

#define INVALID_TASK -1

/* task IRQ object registration request */

struct irq_obj_reg_arg {
	kirq_t irq_obj;   /* IRQ object identifier */
	uint32_t irq;  /* IRQ of device */
	ktask_t taskId; /* requesting task */
};

/* task IRQ object type */

struct task_irq_info {
	ktask_t taskId;  /* task ID of task IRQ object's owner */
	uint32_t irq;    /* IRQ used by task IRQ object */
	kevent_t event;  /* event number assigned to task IRQ object */
	uint32_t vector; /* interrupt vector assigned to task IRQ object */
};

/* task IRQ object array */

static struct task_irq_info task_irq_object[MAX_TASK_IRQS] = {
	[0 ...(MAX_TASK_IRQS - 1)].taskId = INVALID_TASK
};

/* architecture-specific */

#if defined(VXMICRO_ARCH_x86)

/* task IRQ interrupt stub array */
NANO_CPU_INT_STUB_DECL(irq_obj_mem_stub[MAX_TASK_IRQS]);

/* note the comma */
#define IRQ_STUB , irq_obj_mem_stub[irq_obj]

#define RELEASE_VECTOR(v) _IntVecMarkFree(v)

#elif defined(CONFIG_CPU_CORTEXM3)
#include <arch/cpu.h>
#define IRQ_STUB
#define RELEASE_VECTOR(v) irq_disconnect(v)
#else
#error "Unknown target"
#endif

/* event id used by first task IRQ object */

extern const kevent_t _TaskIrqEvt0_objId;

/*******************************************************************************
*
* task_irq_int_handler - ISR for task IRQ objects
*
* This ISR handles interrupts generated by registered task IRQ objects.
*
* The ISR triggers an event signal specified by the event number associated
* with a particular task IRQ object; the interrupt for the task IRQ object
* is then disabled. The parameter provided to the ISR is a structure that
* contains information about the objects's vector, IRQ, and event number.
*
* This ISR does not facilitate an int acknowledgment as it presumes that an
* End of Interrupt (EOI) routine is provided by the PIC that is being used.
*
* RETURNS: N/A
*/

static void task_irq_int_handler(
	void *parameter /* ptr to task IRQ object */
	)
{
	struct task_irq_info *irq_obj_ptr = parameter;

	isr_event_send(irq_obj_ptr->event);
	irq_disable(irq_obj_ptr->irq);
}

/*******************************************************************************
*
* task_irq_free - free a task IRQ object
*
* The task IRQ object's interrupt is disabled, and the associated event
* is flushed; the object's interrupt vector is then freed, and the object's
* global array entry is marked as unused.
*
* RETURNS: N/A
*/

void task_irq_free(kirq_t irq_obj /* IRQ object identifier */
			     )
{
	__ASSERT(irq_obj < MAX_TASK_IRQS, "Invalid IRQ object");
	__ASSERT(task_irq_object[irq_obj].taskId == task_id_get(),
			 "Incorrect Task ID");

	irq_disable(task_irq_object[irq_obj].irq);
	RELEASE_VECTOR(task_irq_object[irq_obj].vector);
	(void)task_event_recv(task_irq_object[irq_obj].event);
	task_irq_object[irq_obj].taskId = INVALID_TASK;
}

/*******************************************************************************
*
* task_irq_ack - re-enable a task IRQ object's interrupt
*
* This re-enables the interrupt for a task IRQ object.
*
* RETURNS: N/A
*/

void task_irq_ack(kirq_t irq_obj /* IRQ object identifier */
					    )
{
	__ASSERT(irq_obj < MAX_TASK_IRQS, "Invalid IRQ object");
	__ASSERT(task_irq_object[irq_obj].taskId == task_id_get(),
			 "Incorrect Task ID");

	irq_enable(task_irq_object[irq_obj].irq);
}

/*******************************************************************************
*
* _task_irq_test - determine if a task IRQ object has had an interrupt
*
* This tests a task IRQ object to see if it has signalled an interrupt.
*
* RETURNS: RC_OK, RC_FAIL, or RC_TIME
*/

int _task_irq_test(kirq_t irq_obj, /* IRQ object identifier */
				     int32_t time /* time to wait (in ticks) */
				     )
{
	__ASSERT(irq_obj < MAX_TASK_IRQS, "Invalid IRQ object");
	__ASSERT(task_irq_object[irq_obj].taskId == task_id_get(),
			 "Incorrect Task ID");

	return _task_event_recv(task_irq_object[irq_obj].event, time);
}

/*******************************************************************************
*
* _k_task_irq_alloc - allocate a task IRQ object
*
* This routine allocates a task IRQ object to a task.
*
* RETURNS: ptr to allocated task IRQ object if successful, NULL if not
*/

static int _k_task_irq_alloc(
	void *arg /* ptr to registration request arguments */
	)
{
	struct irq_obj_reg_arg *argp = (struct irq_obj_reg_arg *)arg;
	struct task_irq_info *irq_obj_ptr; /* ptr to task IRQ object */
	int curr_irq_obj;	/* IRQ object loop counter */

	/* Fail if the requested IRQ object is already in use */

	if (task_irq_object[argp->irq_obj].taskId != INVALID_TASK) {
		return (int)NULL;
	}

	/* Fail if the requested IRQ is already in use */

	for (curr_irq_obj = 0; curr_irq_obj < MAX_TASK_IRQS; curr_irq_obj++) {
		if ((task_irq_object[curr_irq_obj].irq == argp->irq) &&
		    (task_irq_object[curr_irq_obj].taskId != INVALID_TASK)) {
			return (int)NULL;
		}
	}

	/* Take ownership of specified IRQ object */

	irq_obj_ptr = &task_irq_object[argp->irq_obj];
	irq_obj_ptr->taskId = argp->taskId;
	irq_obj_ptr->irq = argp->irq;
	irq_obj_ptr->event = (_TaskIrqEvt0_objId + argp->irq_obj);
	irq_obj_ptr->vector = INVALID_VECTOR;

	return (int)irq_obj_ptr;
}

/*******************************************************************************
*
* task_irq_alloc - register a task IRQ object
*
* This routine connects a task IRQ object to a system interrupt based
* upon the specified IRQ and priority values.
*
* IRQ allocation is done via K_swapper so that simultaneous allocation
* requests are single-threaded.
*
* RETURNS: assigned interrupt vector if successful, INVALID_VECTOR if not
*/

uint32_t task_irq_alloc(
	kirq_t irq_obj,   /* IRQ object identifier */
	uint32_t irq,     /* requested IRQ */
	uint32_t priority /* requested interrupt priority */
	)
{
	struct irq_obj_reg_arg arg;  /* IRQ object registration request arguments */
	struct task_irq_info *irq_obj_ptr; /* ptr to task IRQ object */

	/* Allocate the desired IRQ object and IRQ */

	arg.irq_obj = irq_obj;
	arg.irq = irq;
	arg.taskId = task_id_get();

	irq_obj_ptr = (struct task_irq_info *)task_offload_to_fiber(_k_task_irq_alloc,
						     (void *)&arg);
	if (irq_obj_ptr == NULL) {
		return INVALID_VECTOR;
	}

	/*
	 * NOTE: the comma that seems to be missing is part of the IRQ_STUB
	 *       definition to abstract the different irq_connect signatures
	 */
	irq_obj_ptr->vector = irq_connect(
		irq, priority, task_irq_int_handler, (void *)irq_obj_ptr IRQ_STUB);
	irq_enable(irq);

	return irq_obj_ptr->vector;
}


#endif /* (CONFIG_MAX_NUM_TASK_IRQS > 0) */
